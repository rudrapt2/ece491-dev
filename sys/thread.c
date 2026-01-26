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
#include "error.h"
#include "misc.h"
#include "timer.h"

#ifndef MP2
#include "process.h"
#include "memory.h"
#endif

// COMPILE-TIME PARAMETERS
//

#ifndef NTHR // maximum number of threads
#define NTHR 32
#endif

#ifndef SCHED_SLICE_MS // scheduler time slice
#define SCHED_SLICE_MS 20
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

// [MP3cp2] The /thread_stack_anchor/ structure is placed at the base of the
// stack to allow us to restore the thread pointer when entering the kernel from
// U mode.

struct thread {
    struct thread_context ctx;  // must be first (thrasm.s)
    int id; // index into thrtab[]
    enum thread_state state;
    const char * name;
    struct thread_stack_anchor * stack_anchor;
    void * stack_lowest;
    struct process * proc;
    struct thread * parent;
    struct thread * list_next;
    struct condition * wait_cond;
#ifndef STUDENT
    struct condition child_exit;
#endif

#ifdef STUDENT
    // (you may add additional structure members here)
#else
    // Accounting. All times and durations (ticks) are based on rdtime()

    unsigned long long time_spawned;    // when thread was spawned
    unsigned long long time_exited;     // when thread exited
    unsigned long long time_resumed;    // when thread was last resumed
    unsigned long long running_ticks;   // number of ticks in RUNNING state
    int ticks_balance;                  // how many ticks available in slice
#endif // STUDENT
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
    __attribute__ ((unused)); // may be unused

// Returns a string representing a thread state. Used by debug and trace
// statements, so marked unused to avoid compiler warnings.


#ifndef STUDENT
static struct thread * create_thread(const char * name);

// Creates a new thread structure, assigns it a TID, and allocates stack for it.
// The new thread does not have a parent or an associated process.

// The following functions manipulate a thread list (struct thread_list). Note
// that threads form a linked list via the list_next member of each thread
// structure. Thread lists are used for the ready-to-run list (ready_list) and
// for the list of waiting threads of each condition variable. These functions
// are not interrupt-safe! The caller must disable interrupts before calling any
// thread list function that may modify a list that is used in an ISR.

static void reclaim_thread_stack(struct thread * thr);

// Reclaims storage used for the thread stack. Sets the /stack_lowest/ and
// /stack_anchor/ members of the /thread/ structure to NULL.

static void reclaim_thread_struct(struct thread * thr);

// Frees the /thread/structure clears its /thrtab/ entry.

#endif

static void tlclear(struct thread_list * list);
static int tlempty(const struct thread_list * list);
static struct thread * tlpeek(const struct thread_list * list);
static void tlinsert(struct thread_list * list, struct thread * thr);
static struct thread * tlremove(struct thread_list * list);
static void tlappend(struct thread_list * l0, struct thread_list * l1);
static void tlprepend(struct thread_list * l0, struct thread_list * l1);

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

#ifndef STUDENT
static int sched_slice_ticks; // ticks in a scheduler time slice
static int sched_min_ticks;   // minimum tick balance needed to run
#endif

static struct thread main_thread = {
    .id = MAIN_TID,
    .name = "main",
    .state = THREAD_RUNNING,
    .stack_anchor = (void*)_main_stack_anchor,
    .stack_lowest = _main_stack_lowest,
    .child_exit = { .name = "main_thread.child_exit" }
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

#ifndef STUDENT
    assert (timer_frequency > 0);
    sched_slice_ticks = timer_frequency / 1000 * SCHED_SLICE_MS;
    sched_min_ticks = sched_slice_ticks / 16;

    debug("sched_slice_ticks = %u", sched_slice_ticks);
    debug("sched_min_ticks = %u", sched_min_ticks);
#endif

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
#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    struct thread * child;
    va_list ap;
    int pie;
    int i;

    trace("%s(\"%s\",%p) in <%s:%d>", __func__, name, entry, TP->name, TP->id);

    child = create_thread(name);

    if (child == NULL)
        return -EMTHR;

    child->parent = TP;
    set_thread_state(child, THREAD_READY);
    child->time_spawned = rdtime();

    // Prepare arguments to child thread function. We use a special startup
    // function to pass arguments to the child thread function. See
    // _thread_startup() in thrasm.s.

    va_start(ap, entry);

    child->ctx.s[10] = (uintptr_t)NULL;
    child->ctx.s[8] = (uintptr_t)entry;
    child->ctx.s[11] = (uintptr_t)&exit_running_thread;
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
#endif // STUDENT
}


const char * thread_name(int tid) {
    assert (0 <= tid && tid < NTHR);
    assert (thrtab[tid] != NULL);
    return thrtab[tid]->name;
}

const char * running_thread_name(void) {
    return TP->name;
}

void exit_running_thread(void) {
#ifndef STUDENT
    int ctid; // child TID
#endif

    if (TP == &main_thread)
        halt();
    
    set_thread_state(TP, THREAD_EXITED);

#ifdef STUDENT
    // (your MP3cp3 code here)
#else
    // Signal parent (if we have one) in case it is waiting for us to exit

    if (TP->parent != NULL)
        condition_broadcast(&TP->parent->child_exit);

    // Reclaim any of our children that have exited and orphan any that have not
    // (by setting their /parent/ pointer to NULL).

    for (ctid = 1; ctid < NTHR; ctid++) {
        if (thrtab[ctid] != NULL && thrtab[ctid]->parent == TP) {
            if (thrtab[ctid]->state == THREAD_EXITED)
                reclaim_thread_struct(thrtab[ctid]);
            else
                thrtab[ctid]->parent = NULL;
        }   
    }

    // Note: A thread in the EXITED state no longer needs its stack. However, we
    // can't free it just yet, because we're still using it. So we free it in
    // the context of the next scheduled thread. The /thread/ structure itself
    // will be freed either in thread_join() or, when the parent exists in
    // exit_running_thread() if the parent exists before joining with us.

    yield_running_thread(); // should not return
    panic(NULL);
#endif // STUDENT
}

#ifndef MP2
void submit_running_thread(void) {
#ifdef STUDENT
    // (your MP3cp3 code here)
#else
    unsigned long long time_must_suspend; // time thread must suspend
    unsigned long long time_now;

    time_now = rdtime();
    time_must_suspend = TP->time_resumed + TP->ticks_balance;

    if (time_must_suspend <= time_now)
        yield_running_thread();
#endif // STUDENT
}
#endif // MP2

void yield_running_thread(void) {
    extern void switch_running_thread(struct thread *); // thrasm.s
#ifndef STUDENT
    struct thread * susp_thread; // suspending thread
    struct thread * next_thread; // resuming thread
    int pie;
#endif

    trace("%s() in <%s:%d>", __func__, TP->name, TP->id);

    // The idle thread is always runnable, and the idle thread only calls
    // yield() if the ready_list is not empty.

    assert (!tlempty(&ready_list));

#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    susp_thread = TP;

    // Add current thtread to the ready list if it is still considered RUNNING.
    // (Its stgate may be set to EXITED or WAITING before yield_running_thread()
    // is called). Interrupts must be disabled while we manipulate the ready
    // list. 

    pie = disable_interrupts();

    if (susp_thread->state == THREAD_RUNNING) {
        set_thread_state(susp_thread, THREAD_READY);
        tlinsert(&ready_list, susp_thread);
    }
    
    // Determine which thread we want to run next. We use round-robin scheduling
    // with accounting for time used. Each thread gets /sched_slice_ticks/ of
    // time added to its /ticks_balance/ variable each time its turn comes up in
    // the round-robin schedule. If the balance is less than /sched_ticks_min/,
    // we skip it.

    for (;;) {
        next_thread = tlremove(&ready_list);
        assert(next_thread->state == THREAD_READY);

        debug("Next thread candidate <%s:%d> has tick balance of %d",
            next_thread->name, next_thread->id, next_thread->ticks_balance);

        // Optional: if the next thread ready to run is the idle thread and
        // there are other threads on the ready list, skip it.

        if (next_thread == &idle_thread && tlpeek(&ready_list) != NULL) {
            tlinsert(&ready_list, next_thread);
            continue;
        }

        next_thread->ticks_balance += sched_slice_ticks;

        if (next_thread->ticks_balance < sched_min_ticks) {
            tlinsert(&ready_list, next_thread);
            continue;
        } else
            break;
    }

    enable_interrupts();

    // No hoarding!

    if (next_thread->ticks_balance > 2*sched_slice_ticks)
        next_thread->ticks_balance = 2*sched_slice_ticks;

    debug("Next thread to run is <%s:%d> with a balance of %d ticks",
        next_thread->name, next_thread->id, next_thread->ticks_balance);
    
    set_thread_state(next_thread, THREAD_RUNNING);

#ifndef MP2
    // If the thread to be resumed has an associated process, switch to its
    // memory space. Otherwise, switch to the main thread's memory space. If
    // there is no process associated with the main thread, then virtual address
    // translation is not active, and we don't need to switch memory spaces.

    if (main_thread.proc != NULL) {
        mtag_t next_mtag;

        if (next_thread->proc != NULL)
            next_mtag = next_thread->proc->mtag;
        else
            next_mtag = main_thread.proc->mtag;
    
        if (active_mspace() != next_mtag) {
            debug("Switching to main memory space");
            switch_mspace(next_mtag);
        }
    }
#endif // !defined(MP2)

    trace("Thread <%s:%d> calling switch_running_thread(<%s:%d>)",
        TP->name, TP->id, next_thread->name, next_thread->id);

    next_thread->time_resumed = rdtime();

    switch_running_thread(next_thread);

    trace("switch_running_thread() returned in <%s:%d>", TP->name, TP->id);
    
    restore_interrupts(pie);
#endif // STUDENT
}

#ifdef STUDENT
// The finish thread function is only called from switch_running_thread() in
// thrasm.s. It is not declared in thread.h.
#else
// The finish thread function is only called from switch_running_thread() in
// thrasm.s. It is not declared in thread.h. It's responsible for freeing the
// stack of an exited thread. If the exited thread has no parent, it reclaims
// the /thread/ structure and entry in /thrtab/.
#endif

void finish_thread_switch(struct thread * susp_thread) {
#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    unsigned long long time_now;
    unsigned int ticks_used;

    trace("%s(<%s:%d>)", __func__, susp_thread->name, susp_thread->id);

    time_now = rdtime();

    // Accounting: add number of ticks used to total and subtract ticks used
    // from current balance.

    ticks_used = time_now - susp_thread->time_resumed;
    susp_thread->running_ticks += ticks_used;
    susp_thread->ticks_balance -= ticks_used;

    // If the suspended thread just exited, free its stack. We can't do this
    // while the thread is still running, so we do it here when we switch away
    // from it. If the thread is an orphan, free the structure and TID also. 

    if (susp_thread->state == THREAD_EXITED) {
        reclaim_thread_stack(susp_thread);
        if (susp_thread->parent == NULL)
            reclaim_thread_struct(susp_thread);
        else
            susp_thread->time_exited = time_now;
    }
#endif // STUDENT
}

int join_thread(int u_tid) {
#ifndef STUDENT
    struct thread * thr;
    int haschild = 0;
    int ctid;
#endif

    trace("%s(%d) in <%s:%d>", __func__, u_tid, TP->name, TP->id);

#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    if (u_tid < 0 || NTHR <= u_tid)
        return -ECHILD;
    
    // if _tid_ is zero, we wait for any child. Check if if there are any
    // children of the current thread, and if they have already exited. 

    while (u_tid == 0) {
        for (ctid = 1; ctid < NTHR; ctid++) {
            if (thrtab[ctid] != NULL && thrtab[ctid]->parent == TP) {
                if (thrtab[ctid]->state == THREAD_EXITED)
                    return join_thread(ctid); // ctid != 0
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
    
    reclaim_thread_struct(thr);
    
    return u_tid;
#endif
}

#ifndef MP2
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
    // assert (thrtab[tid]->proc == NULL);
    // assert (proc != NULL);
    thrtab[tid]->proc = proc;
}

void * running_thread_stack_anchor(void) {
    return TP->stack_anchor;
}
#endif

// EXPORTED CONDITION VARIABLE FUNCTION DEFINITIONS
//

void condition_init(struct condition * cond, const char * name) {
    memset(cond, 0, sizeof(*cond));
    tlclear(&cond->wait_list);
    cond->name = (name != NULL) ? name : "anon";
}

const char * condition_name(const struct condition * cond) {
    return cond->name;
}

void condition_wait(struct condition * cond) {
#ifndef STUDENT
    int pie;
#endif

    trace("%s(<%s>) in <%s:%d>", __func__, cond->name, TP->name, TP->id);

    assert(TP->state == THREAD_RUNNING);

#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    // Insert current thread into condition wait list
    
    set_thread_state(TP, THREAD_WAITING);
    TP->wait_cond = cond;

    pie = disable_interrupts();
    tlinsert(&cond->wait_list, TP);
    restore_interrupts(pie);

    yield_running_thread();
#endif // STUDENT
}

void condition_broadcast(struct condition * cond) {
#ifndef STUDENT
    struct thread_list list;
    struct thread * thr;
    int pie;
#endif

    trace("%s(<%s>) in <%s:%d>", __func__, cond->name, TP->name, TP->id);

    // Fast path: if there are no threads waiting, return.

    if (tlempty(&cond->wait_list))
        return;

#ifdef STUDENT
    // (your MP2cp3 code here)
#else
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

#if 1
    tlappend(&ready_list, &list);
#else
    tlprepend(&ready_list, &list);
#endif

    restore_interrupts(pie);
#endif // STUDENT
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
// The configuration (owner != NULL && cnt == 0) is not valid.
//

void rwlock_init(struct rwlock * rwlk, const char * name) {
#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    memset(rwlk, 0, sizeof(*rwlk));
    condition_init(&rwlk->released, "rwlock.released");
#endif
    rwlk->name = (name != NULL) ? name : "anon";
}

const char * rwlock_name(const struct rwlock * rwlk) {
    return rwlk->name;
}

void rwlock_acquire(struct rwlock * rwlk, int exclusive) {
    trace("%s(<%s>,%d)", __func__, rwlk->name, exclusive);

#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    if (exclusive) {
        if (rwlk->owner != TP) {
            while (rwlk->cnt > 0)
                condition_wait(&rwlk->released);
            rwlk->owner = TP;
        }
    } else {
        while (rwlk->owner != NULL)
            condition_wait(&rwlk->released);
    }

    rwlk->cnt += 1;

    if (rwlk->cnt == 0)
        panic("rwlock.cnt overflow");
#endif // STUDENT
}

void rwlock_release(struct rwlock * rwlk) {
    trace("%s(<%s>)", __func__, rwlk->name);

#ifdef STUDENT
    // (your MP2cp3 code here)
#else
    assert (rwlk->cnt > 0);
    rwlk->cnt -= 1;
    rwlk->owner = NULL; //

    if (rwlk->cnt == 0) {
        assert (rwlk->owner == NULL || rwlk->owner == TP);
        condition_broadcast(&rwlk->released);
        rwlk->owner = NULL;
    }
#endif // STUDENT
}


// INTERNAL FUNCTION DEFINITIONS
//

void init_main_thread(void) {
#ifndef STUDENT
    unsigned long long time_now;

    time_now = rdtime();

#ifndef MP2
    // Most of the main thread structure is initialized statically (near the top
    // of this file). What's left is to initialize the main thread stack anchor
    // to point to the main thread structure itself (a circular reference), and
    // the thread start time.

    main_thread.stack_anchor->ktp = &main_thread;
#endif
    main_thread.time_spawned = time_now;
    main_thread.time_resumed = time_now;
#endif // STUDENT
}

void init_idle_thread(void) {
#ifndef STUDENT
    unsigned long long time_now;

    time_now = rdtime();

    // Initialize stack anchor with pointer to self (see init_main_thread()).
    idle_thread.stack_anchor->ktp = &idle_thread;
    main_thread.time_spawned = time_now;
#endif // STUDENT
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

#ifndef STUDENT
struct thread * create_thread(const char * name) {
    struct thread_stack_anchor * anchor;
    struct thread * thr;
    void * stkmem;
    int tid;

    trace("%s(\"%s\") in <%s:%d>", __func__, name, TP->name, TP->id);

    // Find a free thread slot.

    tid = 0;
    while (++tid < NTHR)
        if (thrtab[tid] == NULL)
            break;
    
    if (tid == NTHR)
        return NULL;
    
    // Allocate a struct thread and a stack

    thr = kcalloc(1, sizeof(struct thread));

#ifndef MP2
    stkmem = alloc_phys_page();
    anchor = stkmem + PAGE_SIZE;
#else
    stkmem = kmalloc(HEAP_ALLOC_MAX);
    anchor = stkmem + HEAP_ALLOC_MAX;
#endif
    assert (stkmem != NULL);
    anchor -= 1; // anchor is at base of stack
    thr->stack_lowest = stkmem;
    thr->stack_anchor = anchor;
#ifndef MP2
    anchor->ktp = thr;
    anchor->kgp = NULL;
#endif

    thrtab[tid] = thr;

    thr->name = (name != NULL) ? name : "anon";
    thr->id = tid;

    condition_init(&thr->child_exit, "thread.child_exit");

    return thr;
}

void reclaim_thread_stack(struct thread * thr) {
    assert (thr->stack_lowest != NULL);
#ifndef MP2
    free_phys_page(thr->stack_lowest);
#else
    kfree(thr->stack_lowest);
#endif
    thr->stack_lowest = NULL;
    thr->stack_anchor = NULL;
}

void reclaim_thread_struct(struct thread * thr) {
    int const tid = thr->id;

    assert (thr->stack_lowest == NULL);

    kfree(thr);
    thrtab[tid] = NULL;
}

#endif // STUDENT

void tlclear(struct thread_list * list) {
    list->head = NULL;
    list->tail = NULL;
}

int tlempty(const struct thread_list * list) {
    return (list->head == NULL);
}

struct thread * tlpeek(const struct thread_list * list) {
    return list->head;
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

void tlprepend(struct thread_list * l0, struct thread_list * l1) {
    if (l1->head == NULL) {
        assert (l1->tail == NULL);
        return;
    }

    assert(l1->tail != NULL);
        
    if (l0->head != NULL) {
        assert(l0->tail != NULL);
        l1->tail->list_next = l0->head;
    } else {
        assert(l0->tail == NULL);
        l0->tail = l1->tail;
    }

    l0->head = l1->head;
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
            yield_running_thread();
        
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