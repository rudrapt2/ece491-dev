// process.c - Processes and process manager
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//


#ifdef PROCESS_TRACE
#define TRACE
#endif

#ifdef PROCESS_DEBUG
#define DEBUG
#endif

#include "process.h"

#include "conf.h"
#include "elf.h"
#include "error.h"
#include "filesys.h"
#include "heap.h"
#include "memory.h"
#include "misc.h"
#include "riscv.h"
#include "string.h"
#include "thread.h"
#include "trap.h"
#include "io.h"

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

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//

void procmgr_init(void) {
    assert(memory_initialized && heap_initialized);
    assert(!procmgr_initialized);

    main_proc.tid = running_thread();
    main_proc.mtag = active_mspace();
    thread_attach_process(main_proc.tid, &main_proc);
    procmgr_initialized = 1;
}

int process_exec(struct io * exeio, int argc, char ** argv) {
    struct process * self;
    struct trap_frame tfr;
    void (*entry)(void);
    void * stack;
    int stksz;
    int result;

    trace("%s(exeio=%p)", __func__, exeio);

    self = current_process();

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

    // Clear user memory mapping.

    reset_active_mspace();

    // Load process image from ELF file. At this point we can't return an error,
    // since we have reset the address space. So we print an error to the
    // console and terminate our thread. A better way to handle this would be to
    // create an elf_validate() function that could return an error if there is
    // something wrong with the ELF file.

    result = elf_load(exeio, &entry);

    // Save the io object of the executable in the /exeio/ member of the
    // /process/ structure.

    if (self->exeio != NULL)
        iodropref(self->exeio);
    
    self->exeio = exeio;

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
    int i;

    trace("%s(tfr=%p)", __func__, tfr);

    parent = current_process();
    
    // Allocate process struct for child and initialize.

    child = kcalloc(1, sizeof(struct process));
    child->mtag = clone_active_mspace();

    // Copy io object pointers and increment ref count

    child->exeio = ioaddref(parent->exeio);

    for (i = 0; i < PROCESS_IOMAX; i++) {
        child->iotab[i] = parent->iotab[i];
        if (child->iotab[i] != NULL)
            ioaddref(child->iotab[i]);
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

    // this has a memory leak since we never free the child
    if (ctid < 0)
        return ctid;
    
    thread_attach_process(ctid, child);
    condition_wait(&done);

    return ctid;
}

/** \brief
 *
 *  
 *  Discard memory space, close your associated uio, free the memory you're supposed to free.
 *  
 *
 */
void process_exit(void) {
    struct process * self;
    int i;

    self = current_process();

    trace("%s() in %s", __func__, thread_name(running_thread()));

    if (running_thread() == 0) {
        flush_all_filesys();
        shutdown();
    }

    discard_active_mspace();

    iodropref(self->exeio);
    
    for (i = 0; i < PROCESS_IOMAX; i++) {
        if (self->iotab[i] != NULL)
            iodropref(self->iotab[i]);
    }
    
    // Detach process from running thread and free the process structure.

    thread_attach_process(running_thread(), NULL);
    kfree(self);

    exit_running_thread();
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
    void * sscratch;

    condition_broadcast(done); // signal parent we're done using trap frame

    tfr->a0 = 0;
    sscratch = running_thread_stack_anchor() - sizeof(struct trap_frame);
    trap_frame_jump(tfr, sscratch);
}