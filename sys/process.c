// process.c - user process
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

/*!
* @brief Enables trace messages for process.c
*/
#ifdef PROCESS_TRACE
#define TRACE
#endif

/*!
* @brief Enables debug messages for process.c
*/
#ifdef PROCESS_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "process.h"
#include "elf.h"
#include "filesys.h"
#include "uio.h"
#include "string.h"
#include "thread.h"
#include "riscv.h"
#include "trap.h"
#include "memory.h"
#include "heap.h"
#include "error.h"
#include "misc.h"

// COMPILE-TIME PARAMETERS
//

/*! 
* @brief Maximum number of processes
*/
#ifndef NPROC
#define NPROC 16
#endif

// INTERNAL FUNCTION DECLARATIONS
//

static int build_stack(void * stack, int argc, char ** argv);

static void fork_func(struct condition * forked, struct trap_frame * tfr);

// INTERNAL GLOBAL VARIABLES
//

/*!
* @brief The main user process struct
*/
static struct process main_proc;

static struct process * proctab[NPROC] = {
    &main_proc
};

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//

void procmgr_init(void) {
    assert (memory_initialized && heap_initialized);
    assert (!procmgr_initialized);

    main_proc.tid = running_thread();
    main_proc.mtag = active_mspace();
    thread_attach(main_proc.tid, &main_proc);
    procmgr_initialized = 1;
}

int process_exec(struct uio * exefile, int argc, char ** argv) {
    struct trap_frame tfr;
    void (*entry)(void);
    void * stack;
    int stksz;
    int result;

    trace("%s(exefile=%p)", __func__, exefile);

    // The exec system call is a tricky. If something goes wrong after we reset
    // the process memory space, we cannot just return an error, since there is
    // nothing to return to. We try to do as much as possible before resetting
    // the memory space so that we can return an error.

    // Create the stack page. We will map it into the new memory space later.

    stack = alloc_phys_page();

    // If we are implementing command-line argument support, call build_stack()
    // to build the new stack page containing argv[] and the strings it points
    // to. Otherwise, set _argc_ to zero and argv[0] = NULL.

    stksz = build_stack(stack, argc, argv);

    if (stksz < 0)
        return stksz;

    // Clear user memory mapping

    reset_active_mspace();

    // Load process image from ELF file. Note that at this point, we can't
    // return an error, since we have reset the address space. So we print an
    // error to the console and terminate our thread.

    result = elf_load(exefile, &entry);
    uio_close(exefile);

    if (result != 0) {
        debug("exec: elf_load: %s\n", error_name(result));
        process_exit();
    }

    // Map the stack page we allocated earlier into the process memory space.

    map_page(UMEM_END_VMA - PAGE_SIZE, stack, PTE_R | PTE_W | PTE_U);

    // Create and initialize a trap frame to jump to user space.

    memset(&tfr, 0, sizeof(tfr));
    tfr.sp = UMEM_END - stksz;
    tfr.sepc = (void*)entry;
    tfr.a0 = argc;
    tfr.a1 = UMEM_END_VMA - stksz; // build_stack() places argv here
    tfr.sstatus = csrr_sstatus();
    tfr.sstatus |= RISCV_SSTATUS_SPIE;
    tfr.sstatus &= ~RISCV_SSTATUS_SPP;
    
    trap_frame_jump(&tfr, running_thread_stack_anchor() - sizeof(tfr));
}

int process_fork(const struct trap_frame * tfr) {
    struct process * parent; // parent process
    struct process * child; // child process
    struct condition done; // child done using tfr
    int ctid; // child tid
    int idx;
    int i;

    trace("%s(tfr=%p)", __func__, tfr);

    parent = current_process();

    // Find an open process slot for the child

    idx = 0;
    while (idx < NPROC) {
        if (proctab[idx] == NULL)
            break;
        idx += 1;
    }

    if (idx == NPROC)
        return -EMPROC;
    
    // Allocate process struct for child and initialize.

    child = kcalloc(1, sizeof(struct process));
    proctab[idx] = child;
    child->mtag = clone_active_mspace();

    // Copy io object pointers and increment ref count

    for (i = 0; i < PROCESS_IOMAX; i++) {
        child->uiotab[i] = parent->uiotab[i];
        if (child->uiotab[i] != NULL)
            uio_addref(child->uiotab[i]);
    }

    // Spawn a new thread for the child. The child thread will use the parent's
    // trap frame to return to U mode, signalling the parent that it is done
    // with the trap frame.

    // create condition variable
    // spawn_child with pointer to condition variable and trap frame
    // parent waits for child to signal it is done
    // child signals parent, then jumps on trap frame
    // parent writes return value into trap frame and returns normally

    condition_init(&done, "fork_child_done");
    ctid = spawn_thread("fork_child", (void*)&fork_func, &done, tfr);
    thread_attach(ctid, child);

    if (ctid < 0)
        return ctid;
    
    condition_wait(&done);

    return ctid;
}

void process_exit(void) {
    struct process * self;
    int i;

    self = current_process();

    trace("%s() in %s", __func__, thread_name(running_thread()));

    if (running_thread() == 0) {
        fsmgr_flushall();
        panic("Main process exited");
    }

    discard_active_mspace();

    for (i = 0; i < PROCESS_UIOMAX; i++) {
      if (self->uiotab[i] != NULL)
      {
        debug("process_exit: closing fd=%d, refcnt=%lu", i, uio_refcnt(self->uiotab[i]));
        uio_close(self->uiotab[i]);
      }
    }
    
    // Free process struct. First, though, remove references to it from thread
    // struct and proctab.

    thread_attach(running_thread(), NULL);
    
    for (i = 0; i < NPROC; i++) {
        if (proctab[i] == self) {
            proctab[i] = NULL;
            break;
        }
    }

    kfree(self);

    running_thread_exit();
}

// INTERNAL FUNCTION DEFINITIONS
//

int build_stack(void * stack, int argc, char ** argv) {
    size_t stksz, argsz;
    uintptr_t * newargv;
    char * p;
    int i;

    // We need to be able to fit argv[] on the initial stack page, so _argc_
    // cannot be too large. Note that argv[] contains argc+1 elements (last one
    // is a NULL pointer).

    if (PAGE_SIZE / sizeof(char*) - 1 < argc)
        return -ENOMEM;
    
    stksz = (argc+1) * sizeof(char*);

    // Add the sizes of the null-terminated strings that argv[] points to.

    for (i = 0; i < argc; i++) {
        argsz = strlen(argv[i])+1;
        if (PAGE_SIZE - stksz < argsz)
            return -ENOMEM;
        stksz += argsz;
    }

    // Round up stksz to a multiple of 16 (RISC-V ABI requirement).

    stksz = ROUND_UP(stksz, 16);
    assert (stksz <= PAGE_SIZE);

    // Set _newargv_ to point to the location of the argument vector on the new
    // stack and set _p_ to point to the stack space after it to which we will
    // copy the strings. Note that the string pointers we write to the new
    // argument vector must point to where the user process will see the stack.
    // The user stack will be at the highest page in user memory, the address of
    // which is `(UMEM_END_VMA - PAGE_SIZE)`. The offset of the _p_ within the
    // stack is given by `p - newargv'.

    newargv = stack + PAGE_SIZE - stksz;
    p = (char*)(newargv+argc+1);

    for (i = 0; i < argc; i++) {
        newargv[i] = (UMEM_END_VMA - PAGE_SIZE) + ((void*)p - (void*)stack);
        argsz = strlen(argv[i])+1;
        memcpy(p, argv[i], argsz);
        p += argsz;
    }

    newargv[argc] = 0;
    return stksz;
}

void fork_func(struct condition * done, struct trap_frame * tfr) {
    condition_broadcast(done); // signal parent we're done using trap frame

    tfr->a0 = 0;
    trap_frame_jump(tfr, running_thread_stack_anchor() - sizeof(struct trap_frame));
}