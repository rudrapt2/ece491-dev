// thread.c - Thread creation and synchronization
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef THREAD_TRACE
#define TRACE
#endif

#ifdef THREAD_DEBUG
#define DEBUG
#endif

#include "thread.h"

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "heap.h"
#include "string.h"
#include "riscv.h"
#include "intr.h"
#include "process.h"
#include "memory.h"
#include "error.h"
#include "misc.h"
#include "see.h"

// COMPILE-TIME PARAMETERS
//

// NTHR is the maximum number of threads

#ifndef NTHR
#define NTHR 16
#endif

// EXPORTED GLOBAL VARIABLES
//

char thrmgr_initialized = 0;

// INTERNAL TYPE DEFINITIONS
//

enum thread_state {
    THREAD_UNDEFINED = 0,
    THREAD_WAITING,
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_EXITED
};

struct thread_context {
    uintptr_t s[12];
    void * ra;
    void * sp;
};

struct thread_stack_anchor {
    struct thread * ktp;
    void * kgp;
};

struct thread {
    struct thread_context ctx;  // must be first member (thrasm.s)
    int id; // index into thrtab[]
    enum thread_state state;
    const char * name;
    struct thread_stack_anchor * stack_anchor;
    void * stack_lowest;
    struct process * proc;
    struct thread * parent;
    struct thread * list_next;
    struct condition * wait_cond;
    struct condition child_exit;
};

// INTERNAL MACRO DEFINITIONS
// 

// Pointer to running thread, which is kept in the tp (x4) register.

#define TP ((struct thread*)get_thread_pointer())

// Macro for changing thread state. If compiled for debugging (DEBUG is
// defined), prints function that changed thread state.

#define set_thread_state(t,s) do { \
    debug("Thread <%s:%d> state changed from %s to %s by <%s:%d> in %s", \
        (t)->name, (t)->id, \
        thread_state_name((t)->state), \
        thread_state_name(s), \
        TP->name, TP->id, \
        __func__); \
    (t)->state = (s); \
} while (0)

// INTERNAL FUNCTION DECLARATIONS
//

static void init_main_thread(void);
static void init_idle_thread(void);

// Initializes the main and idle threads. called from threads_init().

static const char * thread_state_name(enum thread_state state)
    __attribute__ ((unused));

// Returns a string representing a thread state. Used by debug and trace
// statements, so marked unused to avoid compiler warnings.

static struct thread * create_thread(const char * name);

// Creates a new thread structure, assigns it a TID, and allocates stack for it.
// The new thread does not have a parent or an associated process.

// The following functions manipulate a thread list (struct thread_list). Note
// that threads form a linked list via the list_next member of each thread
// structure. Thread lists are used for the ready-to-run list (ready_list) and
// for the list of waiting threads of each condition variable. These functions
// are not interrupt-safe! The caller must disable interrupts before calling any
// thread list function that may modify a list that is used in an ISR.

static void tlclear(struct thread_list * list);
static int tlempty(const struct thread_list * list);
static void tlinsert(struct thread_list * list, struct thread * thr);
static struct thread * tlremove(struct thread_list * list);
static void tlappend(struct thread_list * l0, struct thread_list * l1);

static void idle_thread_func(void);

// IMPORTED QUASI-FUNCTION DECLARATIONS
// defined in thrasm.s
//

extern struct thread * switch_threads(struct thread * next_thread);
extern void start_thread(void);

// INTERNAL GLOBAL VARIABLES
//

#define MAIN_TID 0
#define IDLE_TID (NTHR-1)

static struct thread main_thread;
static struct thread idle_thread;

extern char _main_stack_lowest[]; // from start.s
extern char _main_stack_anchor[]; // from start.s

static struct thread main_thread = {
    .id = MAIN_TID,
    .name = "main",
    .state = THREAD_RUNNING,
    .stack_anchor = (void*)_main_stack_anchor,
    .stack_lowest = _main_stack_lowest,
    .child_exit.name = "main.child_exit"
};

extern char _idle_stack_lowest[]; // from thrasm.s
extern char _idle_stack_anchor[]; // from thrasm.s

static struct thread idle_thread = {
    .id = IDLE_TID,
    .name = "idle",
    .state = THREAD_READY,
    .parent = &main_thread,
    .stack_anchor = (void*)_idle_stack_anchor,
    .stack_lowest = _idle_stack_lowest,
    .ctx.sp = _idle_stack_anchor,
    .ctx.ra = &idle_thread_func
};

static struct thread * thrtab[NTHR] = {
    [MAIN_TID] = &main_thread,
    [IDLE_TID] = &idle_thread
};

static struct thread_list ready_list = {
    .head = &idle_thread,
    .tail = &idle_thread
};

// EXPORTED THREAD FUNCTION DEFINITIONS
//

int running_thread(void) {
    return TP->id;
}

void thrmgr_init(void) {
    trace("%s()", __func__);
    init_main_thread();
    init_idle_thread();
    set_thread_pointer(&main_thread);
    thrmgr_initialized = 1;
}

int spawn_thread (
    const char * name,
    void (*entry)(void),
    ...)
{
    struct thread * child;
    va_list ap;
    int pie;
    int i;

    child = create_thread(name);

    if (child == NULL)
        return -EMTHR;

    child->parent = TP;
    child->proc = TP->proc;
    set_thread_state(child, THREAD_READY);

    // Prepare arguments to child thread function. We use a special startup
    // function to pass arguments to the child thread function. See
    // _thread_startup() in thrasm.s.

    va_start(ap, entry);

    child->ctx.s[10] = (uintptr_t)NULL;
    child->ctx.s[8] = (uintptr_t)entry;
    child->ctx.s[11] = (uintptr_t)&running_thread_exit;
    child->ctx.ra = &start_thread;
    child->ctx.sp = child->stack_anchor;

    for (i = 0; i < 8; i++)
        child->ctx.s[i] = va_arg(ap, uint64_t);
    
    va_end(ap);

    // Insert thread into ready list

    pie = disable_interrupts();
    tlinsert(&ready_list, child);
    restore_interrupts(pie);

    return child->id;
}

void running_thread_exit(void) {
    int ctid; // child TID

    if (TP == &main_thread)
        halt_success();
    
    set_thread_state(TP, THREAD_EXITED);

    // Signal parent in case it is waiting for us to exit

    assert(TP->parent != NULL);
    condition_broadcast(&TP->parent->child_exit);

    // Reparent the current thread's children to main_thread

    for (ctid = 1; ctid < NTHR; ctid++) {
        if (thrtab[ctid] != NULL && thrtab[ctid]->parent == TP)
            thrtab[ctid]->parent = &main_thread;
    }

    // Note: A thread in the EXITED state no longer needs its stack. However,
    // we can't free it just yet, because we're still using it. So we free it
    // in the context of the next scheduled thread.

    running_thread_yield(); // should not return
    halt_failure();
}

void running_thread_yield(void) {
    extern void switch_running_thread(struct thread *); // thrasm.s
    struct thread * susp_thread; // suspending thread
    struct thread * next_thread; // resuming thread
    int pie;

    trace("%s() in <%s:%d>", __func__, TP->name, TP->id);

    // The idle thread is always runnable, and the idle thread only calls
    // yield() if the ready_list is not empty.

    assert (!tlempty(&ready_list));

    susp_thread = TP;

    // Get a READY thread from the ready list and mark it running

    pie = disable_interrupts();

    next_thread = tlremove(&ready_list);
    assert(next_thread->state == THREAD_READY);
    set_thread_state(next_thread, THREAD_RUNNING);
    
    // If the suspending thread is still running, mark it ready-to-run and put
    // it in the back of the ready-to-run list.

    if (susp_thread->state == THREAD_RUNNING) {
        set_thread_state(susp_thread, THREAD_READY);
        tlinsert(&ready_list, susp_thread);
    }

    enable_interrupts();
    
    // If the thread to be resumed has an associated process, switch to its
    // memory space. Otherwise, switch to the main thread's memory space. If
    // there is no process associated with the main thread, then virtual address
    // translation is not active, and we don't need to switch memory spaces.

    if (next_thread->proc != NULL) {
        debug("Switching to memory space 0x%lx", next_thread->proc->mtag);
        switch_mspace(next_thread->proc->mtag);
    } else if (main_thread.proc != NULL) {
        debug("Switching to main memory space");
        switch_mspace(main_thread.proc->mtag);
    }

    trace("Thread <%s:%d> calling switch_threads(<%s:%d>)",
        TP->name, TP->id, next_thread->name, next_thread->id);
    
    switch_running_thread(next_thread);

    trace("switch_threads() returned in <%s:%d>", TP->name, TP->id);

    restore_interrupts(pie);
}

// The finish thread function is only called from switch_running_thread() in
// thrasm.s. It is not declared in thread.h. It's responsible for freeing the
// stack of an exited thread.

void finish_thread_switch(struct thread * suspended_thread) {
    // If the suspended thread exited, free its stack. We can't do it any
    // earlier as we are still using the stack. This function is tail-called by
    // switch_threads().

    if (suspended_thread->state == THREAD_EXITED) {
        free_phys_page(suspended_thread->stack_lowest);
        suspended_thread->stack_lowest = NULL;
        suspended_thread->stack_anchor = NULL;
    }
}

int thread_join(int u_tid) {
    struct thread * thr;
    int ctid;
    int haschild = 0;

    trace("%s(u_tid=%d) in <%s:%d>", __func__, u_tid, TP->name, TP->id);

    if (u_tid < 0 || NTHR <= u_tid)
        return -ECHILD;
    
    // if _tid_ is zero, we wait for any child. Check if if there are any
    // children of the current thread, and if they have already exited. 

    while (u_tid == 0) {
        for (ctid = 1; ctid < NTHR; ctid++) {
            if (thrtab[ctid] != NULL && thrtab[ctid]->parent == TP) {
                if (thrtab[ctid]->state == THREAD_EXITED)
                    return thread_join(ctid); // ctid != 0
                haschild = 1;
            }
        }

        if (!haschild)
            return -ECHILD;
        
        // Wait for child exit condition to be signalled, then try again.

        condition_wait(&TP->child_exit);
        u_tid = 0;
    }

    thr = thrtab[u_tid];

    // Can only wait for child if we're the parent
    
    if (thr == NULL || thr->parent != TP)
        return -ECHILD;
    
    // Wait for child to exit. Whenever a child exits, it signals its parent's
    // child_exit condition.

    while (thr->state != THREAD_EXITED)
        condition_wait(&TP->child_exit);
    
    thrtab[u_tid] = NULL;
    kfree(thr);
    
    return u_tid;
}

// TODO Do we call this anywhere? If not, get rid of it.

struct process * thread_process(int tid) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    return thrtab[tid]->proc;
}

struct process * running_thread_process(void) {
    return TP->proc;
}

void thread_attach_process(int tid, struct process * proc) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    assert (proc != NULL);
    thrtab[tid]->proc = proc;
}

const char * thread_name(int tid) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    return thrtab[tid]->name;
}

const char * running_thread_name(void) {
    return TP->name;
}

void * running_thread_stack_anchor(void){
    return TP->stack_anchor;
}

// EXPORTED CONDITION VARIABLE FUNCTION DEFINITIONS
//

void condition_init(struct condition * cond, const char * name) {
    tlclear(&cond->wait_list);
    cond->name = name;
}

void condition_wait(struct condition * cond) {
    int pie;

    trace("%s(cond=<%s>) in <%s:%d>", __func__, cond->name, TP->name, TP->id);

    assert(TP->state == THREAD_RUNNING);

    // Insert current thread into condition wait list
    
    set_thread_state(TP, THREAD_WAITING);
    TP->wait_cond = cond;

    pie = disable_interrupts();
    tlinsert(&cond->wait_list, TP);
    restore_interrupts(pie);

    running_thread_yield();
}

void condition_broadcast(struct condition * cond) {
    struct thread_list list;
    struct thread * thr;
    int pie;

    // Fast path: if there are no threads waiting, return.

    if (tlempty(&cond->wait_list))
        return;

    // Copy wait list and clear the one in the condition itself. Once we save
    // the wait list in the /list/ variable and clear the wait list in the
    // condition structure, it is safe to re-enable interrupts. This is because
    // the only thread list-related function that may be called from an ISR is
    // condition_broadcast(). If it gets called again on this condition, the
    // wait list will be empty.

    pie = disable_interrupts();
    list = cond->wait_list;
    tlclear(&cond->wait_list);
    restore_interrupts(pie);

    // Change state of each thread in list from WAITING to READY

    for (thr = list.head; thr != NULL; thr = thr->list_next) {
        assert (thr->state == THREAD_WAITING);
        assert (thr->wait_cond == cond);
        set_thread_state(thr, THREAD_READY);
        thr->wait_cond = NULL;
    }

    // Append condition variable wait list to run list

    pie = disable_interrupts();
    tlappend(&ready_list, &list);
    restore_interrupts(pie);
}

// EXPORTED READERS-WRITER LOCK FUNCTION DEFINITIONS
//

// A readers-writer lock (rw-lock) is implemented using the rwlock structure:
//
// struct rwlock {
//     struct condition released;
//     struct thread * owner;
//     unsigned long cnt;
// };
//
// The three states are:
// - UNLOCKED when (owner == NULL && cnt == 0),
// - LOCKED-SHARED when (owner == NULL && cnt > 0), and
// - LOCKED-EXCLUISIVE when (owner != NULL && cnt > 0).
//
// The configuration (owner == NULL && cnt > 0) is not valid.
//

void rwlock_init(struct rwlock * rwlk) {
    memset(rwlk, 0, sizeof(*rwlk));
    condition_init(&rwlk->released, "rwlock.released");
}

void rwlock_acquire_shared(struct rwlock * rwlk) {
    assert (rwlk->owner != TP);

    while (rwlk->owner != NULL)
        condition_wait(&rwlk->released);

    rwlk->cnt += 1;

    if (rwlk->cnt == 0)
        panic("rwlock.cnt overflow");
}

void rwlock_release_shared(struct rwlock * rwlk) {
    assert (rwlk->owner == NULL);
    assert (rwlk->cnt > 0);

    rwlk->cnt -= 1;

    if (rwlk->cnt == 0)
        condition_broadcast(&rwlk->released);
}

void rwlock_acquire_exclusive(struct rwlock * rwlk) {
    if (rwlk->owner != TP) {
        while (rwlk->owner != NULL)
            condition_wait(&rwlk->released);
        rwlk->owner = TP;
    }
    
    rwlk->cnt += 1;

    if (rwlk->cnt == 0)
        panic("rwlock.cnt overflow");
}

void rwlock_release_exclusive(struct rwlock * rwlk) {
    assert (rwlk->owner == TP);
    assert (rwlk->cnt != 0);

    rwlk->cnt -= 1;

    if (rwlk->cnt == 0)
        condition_broadcast(&rwlk->released);
}

// INTERNAL FUNCTION DEFINITIONS
//

void init_main_thread(void) {
    // Most of the main thread structure is initialized statically (near the top
    // of this file). What's left is to initialize the main thread stack anchor
    // to point to the main thread structure itself (a circular reference).
    main_thread.stack_anchor->ktp = &main_thread;
}

void init_idle_thread(void) {
    // Initialize stack anchor with pointer to self (see init_main_thread()).
    idle_thread.stack_anchor->ktp = &idle_thread;
}

const char * thread_state_name(enum thread_state state) {
    static const char * const names[] = {
        [THREAD_UNDEFINED] = "UNDEFINED",
        [THREAD_WAITING] = "WAITING",
        [THREAD_RUNNING] = "RUNNING",
        [THREAD_READY] = "READY",
        [THREAD_EXITED] = "EXITED"
    };

    if (0 <= (int)state && (int)state < sizeof(names)/sizeof(names[0]))
        return names[state];
    else
        return names[THREAD_UNDEFINED];
};

struct thread * create_thread(const char * name) {
    struct thread * thr;
    void * stack_page;
    struct thread_stack_anchor * anchor;
    int tid;

    trace("%s(name=\"%s\") in <%s:%d>", __func__, name, TP->name, TP->id);

    // Find a free thread slot.

    tid = 0;
    while (++tid < NTHR)
        if (thrtab[tid] == NULL)
            break;
    
    if (tid == NTHR)
        return NULL;
    
    // Allocate a struct thread and a stack

    thr = kcalloc(1, sizeof(struct thread));
    
    stack_page = alloc_phys_page();
    anchor = stack_page + PAGE_SIZE;
    anchor -= 1; // anchor is at base of stack
    thr->stack_lowest = stack_page;
    thr->stack_anchor = anchor;
    anchor->ktp = thr;
    anchor->kgp = NULL;

    thrtab[tid] = thr;

    thr->id = tid;
    thr->name = name;
    return thr;
}

void tlclear(struct thread_list * list) {
    list->head = NULL;
    list->tail = NULL;
}

int tlempty(const struct thread_list * list) {
    return (list->head == NULL);
}

void tlinsert(struct thread_list * list, struct thread * thr) {
    thr->list_next = NULL;

    if (thr == NULL)
        return;

    if (list->tail != NULL) {
        assert (list->head != NULL);
        list->tail->list_next = thr;
    } else {
        assert(list->head == NULL);
        list->head = thr;
    }

    list->tail = thr;
}

struct thread * tlremove(struct thread_list * list) {
    struct thread * thr;

    thr = list->head;
    
    if (thr == NULL)
        return NULL;

    list->head = thr->list_next;
    
    if (list->head != NULL)
        thr->list_next = NULL;
    else
        list->tail = NULL;

    thr->list_next = NULL;
    return thr;
}

void tlappend(struct thread_list * l0, struct thread_list * l1) {
    if (l0->head != NULL) {
        assert(l0->tail != NULL);
        
        if (l1->head != NULL) {
            assert(l1->tail != NULL);
            l0->tail->list_next = l1->head;
            l0->tail = l1->tail;
        }
    } else {
        assert(l0->tail == NULL);
        l0->head = l1->head;
        l0->tail = l1->tail;
    }

    l1->head = NULL;
    l1->tail = NULL;
}

void idle_thread_func(void) {
    // The idle thread sleeps using wfi if the ready list is empty. Note that we
    // need to disable interrupts before checking if the thread list is empty to
    // avoid a race condition where an ISR marks a thread ready to run between
    // the call to tlempty() and the wfi instruction.

    for (;;) {
        // If there are runnable threads, yield to them.

        while (!tlempty(&ready_list))
            running_thread_yield();
        
        // No runnable threads. Sleep using the wfi instruction. Note that we
        // need to disable interrupts and check the runnable thread list one
        // more time (make sure it is empty) to avoid a race condition where an
        // ISR marks a thread ready before we call the wfi instruction.

        disable_interrupts();
        if (tlempty(&ready_list))
            asm ("wfi");
        enable_interrupts();
    }
}