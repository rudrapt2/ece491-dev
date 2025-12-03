// thread.c - Threads
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

// FIXME Need to handle re-parenting child

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

/**
 * @brief defines the current state of the thread
 */
enum thread_state {
    THREAD_UNINITIALIZED = 0,
    THREAD_WAITING,
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_EXITED
};

/**
 * @brief defines the context of the thread
 */
struct thread_context {
    union {
        uint64_t s[12];
        struct {
            uint64_t a[8];      // s0 .. s7
            void (*pc)(void);   // s8
            uint64_t _pad;      // s9
            void * fp;          // s10
            void * ra;          // s11
        } startup;
    };

    void * ra;
    void * sp;
};

/**
 * @brief defines the stack anchor of the thread
 */
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
    struct lock * lock_list;
};

// INTERNAL MACRO DEFINITIONS
// 

// Pointer to running thread, which is kept in the tp (x4) register.

#define TP ((struct thread*)__builtin_thread_pointer())

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

// Initializes the main and idle threads. called from threads_init().

static void init_main_thread(void);
static void init_idle_thread(void);

// Sets the RISC-V thread pointer to point to a thread.

static void set_running_thread(struct thread * thr);

// Returns a string representing the state name. Used by debug and trace
// statements, so marked unused to avoid compiler warnings.

static const char * thread_state_name(enum thread_state state)
    __attribute__ ((unused));

// void thread_reclaim(int tid)
//
// Reclaims a thread's slot in thrtab and makes its parent the parent of its
// children. Frees the struct thread of the thread.

static void thread_reclaim(int tid);

// struct thread * create_thread(const char * name)
//
// Creates and initializes a new thread structure. The new thread is not added
// to any list and does not have a valid context (_thread_switch cannot be
// called to switch to the new thread).

static struct thread * create_thread(const char * name);

// void running_thread_suspend(void)
// Suspends the currently running thread and resumes the next thread on the
// ready-to-run list using _thread_swtch (in threasm.s). Must be called with
// interrupts enabled. Returns when the current thread is next scheduled for
// execution. If the current thread is TP, it is marked READY and placed
// on the ready-to-run list. Note that running_thread_suspend will only return if the
// current thread becomes READY.

static void running_thread_suspend(void);

void lock_release_completely(struct lock * lock);

// void release_all_thread_locks(struct thread * thr)
// Releases all locks held by a thread. Called when a thread exits.

static void release_all_thread_locks(struct thread * thr);

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

// IMPORTED FUNCTION DECLARATIONS
// defined in thrasm.s
//

extern struct thread * _thread_swtch(struct thread * thr);

extern void _thread_startup(void);

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
    .ctx.ra = &_thread_startup,
    .ctx.startup.fp = NULL,
    .ctx.startup.ra = &running_thread_exit,
    .ctx.startup.pc = &idle_thread_func
};

static struct thread * thrtab[NTHR] = {
    [MAIN_TID] = &main_thread,
    [IDLE_TID] = &idle_thread
};

static struct thread_list ready_list = {
    .head = &idle_thread,
    .tail = &idle_thread
};

// EXPORTED FUNCTION DEFINITIONS
//

/**
 * @brief Returns current TID.
 * @details Uses the pointer to running thread, which is kept in the tp (x4) register.
 * @param void void argument
 * @return current thread's ID
 */
int running_thread(void) {
    return TP->id;
}

/**
 * @brief Initializes the thread manager.
 * @details Initializes the main and idle threads, and sets the main thread to
 * running state. Must be called before performing any thread operations.
 * @param void void argument
 * @return void 
 */
void thrmgr_init(void) {
    trace("%s()", __func__);
    init_main_thread();
    init_idle_thread();
    set_running_thread(&main_thread);
    thrmgr_initialized = 1;
}

/**
 * @brief Creates and starts a new thread.
 * @details The thread is added to the thread ready list. The thread's context is set up. 
 * @param name is the name of the thread (optional, may be NULL)
 * @param entry is the thread entry point
 * @param arg is an argument passed to the thread
 * @return the TID of the spawned thread
 */
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

    set_thread_state(child, THREAD_READY);

    pie = disable_interrupts();
    tlinsert(&ready_list, child);
    restore_interrupts(pie);

    child->ctx.startup.fp = NULL;
    child->ctx.startup.pc = entry;
    child->ctx.startup.ra = &running_thread_exit;
    child->ctx.ra = &_thread_startup;
    child->ctx.sp = child->stack_anchor;    

    va_start(ap, entry);
    for (i = 0; i < 8; i++)
        child->ctx.startup.a[i] = va_arg(ap, uint64_t);
    va_end(ap);
    
    return child->id;
}

/**
 * @brief Terminates the currently running thread.
 * @details The current thread will be set to EXITED state, and all locks will
 * be released on the thread. The function signals the parent if it is waiting
 * for the child thread to exit. The currently running thread will be suspended.
 * @param void void argument
 * @return void
 */
void running_thread_exit(void) {
    if (TP == &main_thread)
        halt_success();
    
    set_thread_state(TP, THREAD_EXITED);
    release_all_thread_locks(TP);

    // Signal parent in case it is waiting for us to exit

    assert(TP->parent != NULL);
    condition_broadcast(&TP->parent->child_exit);

    // Note: A thread in the EXITED state no longer needs its stack. However,
    // we can't free it just yet, because we're still using it. So we free it
    // *after* we return from _thread_swtch.

    running_thread_suspend(); // should not return

    halt_failure();
}

/**
 * @brief Yields the CPU to another thread and returns when the current thread is next scheduled to run.
 * @details The currently running thread will be suspended.
 * @param void void argument
 * @return void
 */
void running_thread_yield(void) {
    trace("%s() in <%s:%d>", __func__, TP->name, TP->id);
    running_thread_suspend();
}

/**
 * @brief Waits for a child of the current thread to exit. 
 * @details If TID is not zero, the function waits for the identified child of the running thread to exit. Otherwise, the function waits for any child of the running thread. Returns -EINVAL if TID is not zero and the identified thread does not exist or is not a child of the running thread. Returns -EINVAL if TID is zero and the running thread does not have any children. Reclaims the child thread when it is in exited state.
 * @param tid the TID of the child thread of the current thread that needs to exit
 * @return the TID of the child thread that exited or an error
 */
int thread_join(int tid) {
    int haschild = 0;

    trace("%s(tid=%d) in <%s:%d>", __func__, tid, TP->name, TP->id);

    if (tid < 0 || NTHR <= tid)
        return -EINVAL;
    
    // if _tid_ is zero, we wait for any child. Check if if there are any
    // children of the current thread, and if they have already exited. 

    while (tid == 0) {
        for (tid = 1; tid < NTHR; tid++) {
            if (thrtab[tid] != NULL && thrtab[tid]->parent == TP) {
                if (thrtab[tid]->state == THREAD_EXITED)
                    return thread_join(tid); // tid != 0
                haschild = 1;
            }
        }

        if (!haschild)
            return -EINVAL;
        
        // Wait for child exit condition to be signalled, then try again.

        condition_wait(&TP->child_exit);
        tid = 0;
    }

    // Can only wait for child if we're the parent

    if (thrtab[tid] == NULL || thrtab[tid]->parent != TP)
        return -EINVAL;
    
    // Wait for child to exit. Whenever a child exits, it signals its parent's
    // child_exit condition.

    while (thrtab[tid]->state != THREAD_EXITED)
        condition_wait(&TP->child_exit);
    
    thread_reclaim(tid);

    return tid;
}

/**
 * @brief Returns a pointer to the process struct of a thread's process.
 * @details The process of a thread can be accessed from the thrtab. Returns NULL if the specified thread does not have an associated process (e.g. idle thread).
 * @param tid TID of a thread
 * @return pointer to the thread's process struct
 */
struct process * thread_process(int tid) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    return thrtab[tid]->proc;
}

/**
 * @brief Returns a pointer to the process struct of the currently running thread's process.
 * @details There can only be one currently running thread which is why there is no input parameters. 
 * @param void void argument
 * @return pointer to the currently running thread's process struct
 */
struct process * running_thread_process(void) {
    return TP->proc;
}

/**
 * @brief Sets a thread's associated process.
 * @details The proc argument can be NULL if a thread is a kernel thread (e.g. idle).
 * @param tid thread's ID for which a process needs to be asociated
 * @param proc process to be associated for the given thread
 * @return void 
 */
void thread_set_process(int tid, struct process * proc) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    thrtab[tid]->proc = proc;
}

void thread_detach(int tid) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    thrtab[tid]->parent = NULL;
}

/**
 * @brief Returns a pointer to the name of the input thread.
 * @details The name of a thread can be accessed from the thrtab.
 * @param tid thread ID for which name needs to be returned
 * @return pointer to the given thread's name
 */
const char * thread_name(int tid) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    return thrtab[tid]->name;
}

/**
 * @brief Returns a pointer to the name of the currently running thread.
 * @details There can only be one currently running thread which is why there is no input parameters. 
 * @param void void argument
 * @return pointer to the currently running thread's name
 */
const char * running_thread_name(void) {
    return TP->name;
}

void * running_thread_stack_base(void){
    return TP->stack_anchor;
}

/**
 * @brief Initializes a condition variable. 
 * @details The function clears the condition's wait_list and sets the name of the condition.
 * @param cond pointer to a condition struct to initialize
 * @param name name of the condition variable which may be NULL
 * @return void
 */
void condition_init(struct condition * cond, const char * name) {
    tlclear(&cond->wait_list);
    cond->name = name;
}

/**
 * @brief Makes the current thread wait on a condition variable. The function sets the currently running thread to the WAITING state and sets up its wait condition.
 * @details The function will suspend the currently running thread until a condition is signalled by another thread or interrupt service routine. The condition_wait function may be called with interrupts disabled. It will enable interrupts while the thread is suspended and will restore interrupt enable/disable state to its value when called. When a thread needs to wait for a condition to be signalled by an ISR, condition_wait() should be called with interrupts disabled to avoid a race condition.
 * @param cond pointer to a condition struct to be waited on 
 * @return void
 */
void condition_wait(struct condition * cond) {
    int pie;

    trace("%s(cond=<%s>) in <%s:%d>", __func__,
        cond->name, TP->name, TP->id);

    assert(TP->state == THREAD_RUNNING);

    // Insert current thread into condition wait list
    
    set_thread_state(TP, THREAD_WAITING);
    TP->wait_cond = cond;
    TP->list_next = NULL;

    pie = disable_interrupts();
    tlinsert(&cond->wait_list, TP);
    restore_interrupts(pie);

    running_thread_suspend();
}

/**
 * @brief Wakes up all threads waiting on a condition variable.
 * @details This function may be called from an ISR. Calling condition_broadcast() does not cause a context switch from the currently running thread. Waiting threads are added to the ready-to-run list in the order they were added to the wait queue.
 * @param cond pointer to a condition struct to broadcast on
 * @return void
 */
void condition_broadcast(struct condition * cond) {
    struct thread_list list;
    struct thread * thr;
    int pie;

    // Fast path: if there are no threads waiting, return.

    if (tlempty(&cond->wait_list))
        return;

    // Copy wait list and clear the one in the condition itself

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

/**
 * @brief Initializes a lock.
 * @details Initializes a lock condition variable on lock_release.
 * @param lock pointer to a lock struct
 * @return void
 */
void lock_init(struct lock * lock) {
    memset(lock, 0, sizeof(struct lock));
    condition_init(&lock->release, "lock_release");
}

/**
 * @brief Acquires a lock.
 * @details The function increments the count on locks if the lock owner is the currently running thread. Otherwise it will set the lock owner to the currently running thread. If the lock is owned by anything else that is not the currently running thread, the function will condition wait on the lock release condition until the owner is NULL before setting the new owner as the currently running thread.
 * @param lock pointer to a lock struct
 * @return void
 */
void lock_acquire(struct lock * lock) {
    if (lock->owner != TP) {
        while (lock->owner != NULL)
            condition_wait(&lock->release);
        
        lock->owner = TP;
        lock->cnt = 1;
        lock->next = TP->lock_list;
        TP->lock_list = lock;
    } else
        lock->cnt += 1;
}

/**
 * @brief Releases a lock.
 * @details The function decrements the count on the lock and if there are no more locks on the currently running thread, the function will release the lock completely.
 * @param lock pointer to a lock struct
 * @return void
 */
void lock_release(struct lock * lock) {
    assert (lock->owner == TP);
    assert (lock->cnt != 0);

    lock->cnt -= 1;

    if (lock->cnt == 0)
        lock_release_completely(lock);
}

/**
 * @brief Abandons a lock.
 * @details The function checks if the owner of the lock is the currently running thread, and releases the lock completely. The count of the lock doesn't matter for abandon.
 * @param lock pointer to a lock struct
 * @return void
 */
void lock_abandon(struct lock * lock) {
    if (lock->owner == TP) {
        debug("Abandoning held lock %p", lock);
        lock_release_completely(lock);
    }
}

// INTERNAL FUNCTION DEFINITIONS
//

/**
 * @brief Initializes the main thread.
 * @details This function initializes the stack anchor with a pointer to self.
 * @param void void argument
 * @return void
 */
void init_main_thread(void) {
    // Initialize stack anchor with pointer to self
    main_thread.stack_anchor->ktp = &main_thread;
}

/**
 * @brief Initializes the idle thread.
 * @details This function initializes the stack anchor with a pointer to the idle thread.
 * @param void void argument
 * @return void
 */
void init_idle_thread(void) {
    // Initialize stack anchor with pointer to self
    idle_thread.stack_anchor->ktp = &idle_thread;
}

/**
 * @brief sets the RISC-V thread pointer to point to the input thread.
 * @details inline assembly function to set the input thread to running.
 * @param thr pointer to a thread struct
 * @return void
 */
static void set_running_thread(struct thread * thr) {
    asm inline ("mv tp, %0" :: "r"(thr) : "tp");
}

/**
 * @brief Returns a string representing the state name. 
 * @details This function is used by debug and trace statements, so marked unused to avoid compiler warnings.
 * @param state thread state
 * @return state name
 */
const char * thread_state_name(enum thread_state state) {
    static const char * const names[] = {
        [THREAD_UNINITIALIZED] = "UNINITIALIZED",
        [THREAD_WAITING] = "WAITING",
        [THREAD_RUNNING] = "SELF",
        [THREAD_READY] = "READY",
        [THREAD_EXITED] = "EXITED"
    };

    if (0 <= (int)state && (int)state < sizeof(names)/sizeof(names[0]))
        return names[state];
    else
        return "UNDEFINED";
};

/**
 * @brief Reclaims a thread's slot in thrtab and makes its parent the parent of its children. 
 * @details All the threads from the thrtab will be scanned to find the children of the given thread and will be assigned the new parent. The function also frees the struct thread of the input TID.
 * @param tid thread ID of the thread to be reclaimed
 * @return void
 */
void thread_reclaim(int tid) {
    struct thread * const thr = thrtab[tid];
    int ctid;

    assert (0 < tid && tid < NTHR && thr != NULL);
    assert (thr->state == THREAD_EXITED);

    // Make our parent thread the parent of our child threads. We need to scan
    // all threads to find our children. We could keep a list of all of a
    // thread's children to make this operation more efficient.

    for (ctid = 1; ctid < NTHR; ctid++) {
        if (thrtab[ctid] != NULL && thrtab[ctid]->parent == thr)
            thrtab[ctid]->parent = thr->parent;
    }

    thrtab[tid] = NULL;
    kfree(thr);
}

/**
 * @brief This function creates and initializes a new thread structure. 
 * @details A new thread is created by finding an available slot on the thrtab. A new struct thread and stack is allocated.
 * @param name pointer to a character type for name
 * @return pointer to a newly created thread
 */
struct thread * create_thread(const char * name) {
    struct thread_stack_anchor * anchor;
    void * stack_page;
    struct thread * thr;
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
    thr->parent = TP;
    thr->proc = TP->proc;
    return thr;
}

/**
 * @brief Suspends the currently running thread. 
 * @details This function suspends currently running thread and resumes the next thread on the ready-to-run list using _thread_swtch. Must be called with interrupts enabled. Returns when the current thread is next scheduled for execution. If the current thread is TP, it is marked READY and placed on the ready-to-run list. Note that running_thread_suspend will only return if the current thread becomes READY.
 * @param void void argument
 * @return void
 */
void running_thread_suspend(void) {
    struct thread * prev_thread; // previously running thread
    struct thread * susp_thread; // suspending thread
    struct thread * next_thread; // resuming thread
    int pie;

    trace("%s() in <%s:%d>", __func__, TP->name, TP->id);

    // The idle thread is always runnable, and the idle thread only calls
    // running_thread_suspend() if the ready_list is not empty.

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
    
    if (next_thread->proc != NULL) {
        debug("Switching to memory space 0x%lx", next_thread->proc->mtag);
        switch_mspace(next_thread->proc->mtag);
    }

    trace("Thread <%s:%d> calling _thread_swtch(<%s:%d>)",
        TP->name, TP->id, next_thread->name, next_thread->id);
    
    prev_thread = _thread_swtch(next_thread);

    trace("_thread_swtch() returned in <%s:%d>", TP->name, TP->id);

    // If the previously running thread (the one that most recently called
    // running_thread_suspend) exited, free its stack. We can't do it any
    // earlier as we are still using the stack.

    if (prev_thread->state == THREAD_EXITED) {
        free_phys_page(prev_thread->stack_lowest);
        prev_thread->stack_lowest = NULL;
        prev_thread->stack_anchor = NULL;
    }

    restore_interrupts(pie);
}

/**
 * @brief Clears the input thread list.
 * @details The function resets the head and tail pointers of the list. The caller must disable interrupts before calling the function.
 * @param list pointer to thread_list struct
 * @return void
 */
void tlclear(struct thread_list * list) {
    list->head = NULL;
    list->tail = NULL;
}

/**
 * @brief Checks if a thread list is empty.
 * @details The function checks if the head of the list points to NULL.
 * @param list pointer to a thread_list struct
 * @return integer value on if list is empty or not
 */
int tlempty(const struct thread_list * list) {
    return (list->head == NULL);
}

/**
 * @brief Inserts a node (thread) in the input thread list.
 * @details The function inserts the input thread to the tail of the input list.
 * @param list pointer to a thread_list struct
 * @param thr pointer to a thread struct
 * @return void
 */
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

/**
 * @brief Removes a thread from the thread list.
 * @details The function removes a thread node from the head of the thread list.
 * @param list pointer to a thread_list struct
 * @return pointer to a thread struct
 */
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

/**
 * @brief Appends elements of l1 to the end of l0 and clears l1.
 * @details The function appends list l1 to l0. It takes care of cases if l0 and/or l1 are empty. l1 will be cleared after appending to l0.
 * @param l0 pointer to a thread_list struct
 * @param l1 pointer to a thread_list struct
 * @return void
 */
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

/**
 * @brief Releases a lock completely and removes it from the currently running thread's lock_list.
 * @details All the threads waiting on the lock are signalled before the lock is completely released. This function must only be called if the thread is still holding the lock that needs to be released.
 * @param lock pointer to a lock struct
 * @return void
 */
void lock_release_completely(struct lock * lock) {
    struct lock ** hptr;

    condition_broadcast(&lock->release);
    hptr = &TP->lock_list;
    while (*hptr != lock && *hptr != NULL)
        hptr = &(*hptr)->next;
    assert (*hptr != NULL);
    *hptr = (*hptr)->next;
    lock->owner = NULL;
    lock->next = NULL;
}

/**
 * @brief Releases all locks held by a thread.
 * @details This function is called when a thread exits. It iterates through the thread's lock_list and resets the owner, name, and count for each lock, and notifies the threads on the lock release condition variable.
 * @param thr pointer to a thread struct
 * @return void
 */
void release_all_thread_locks(struct thread * thr) {
    struct lock * head;
    struct lock * next;

    head = thr->lock_list;

    while (head != NULL) {
        next = head->next;
        head->next = NULL;
        head->owner = NULL;
        head->cnt = 0;
        condition_broadcast(&head->release);
        head = next;
    }

    thr->lock_list = NULL;
}

/**
 * @brief The idle thread sleeps using wfi if the ready list is empty.
 * @details The idle thread sleeps using wfi if the ready list is empty. If there are runnable threads, yield to them. Note that we need to disable interrupts before checking if the thread list is empty to avoid a race condition where an ISR marks a thread ready to run between the call to tlempty() and the wfi instruction.
 * @param void void argument
 * @return void
 */
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