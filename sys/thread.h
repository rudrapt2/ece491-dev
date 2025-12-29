// thread.h - Thread creation and synchronization
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _THREAD_H_
#define _THREAD_H_

struct thread; // opaque decl.
struct process; // forward opaque decl. (process.h)

// EXPORTED FUNCTION DECLARATIONS
//

extern char thrmgr_initialized;
extern void thrmgr_init(void);

// TODO

// struct thread * running_thread(void)
// Returns the currently running thread (pointer to struct thread).

extern int running_thread(void);

// int spawn_thread(const char * name, void (*start)(void *), ...)
// 
// Creates and starts a new thread. Argument _name_ is the name of the thread
// (optional, may be NULL), _start_ is the thread entry point, and _arg_ is an
// argument passed to the thread. The thread is added to the runnable thread
// list. If _start_ returns, this is equivalent to calling running_thread_exit from
// _start_. The new thread is associated with the same process as the calling
// thread. To change a thread's associated process, use thread_attach.
// Returns the TID of the spawned thread or a negative value on error.

extern int spawn_thread (
    const char * name,
    void (*entry)(void),
    ...);

// void running_thread_yield(void)
// 
// Yields the CPU to another thread and returns when the current thread is next
// scheduled to run.

extern void running_thread_yield(void);

// int thread_join(int tid)
//
// Waits for a child of the current thread to exit. if _tid_ is not zero, the
// function waits for the identified child of the running thread to exit.
// Otherwise, the function waits for any child of the running thread. The
// function returns the thread id of the child thrad that exited or an error.
// The thread_join function returns -EINVAL if _tid_ is not zero and the
// identified thread does not exist or is not a child of the running thread. The
// thread_join function returns -EINVAL if _tid_ is zero and the running thread
// does not have any children.

extern int thread_join(int tid);

extern void __attribute__ ((noreturn)) running_thread_exit(void);

// Termimates the currently running thread.

extern struct process * thread_process(int tid);

// Returns a pointer to the process structure (`struct process *`) of the
// process associated with the thread specified by /tid/, or NULL if there is no
// associated process (kernel threads). A process may be associated with a
// thread using thread_attach_process().
//
// [MP3cp2] During a context switch, the active memory space is switched to the
// memory space of the resuming thread, if not NULL. If the resuming thread does
// not have an associated process, the active memory space is not switched.
//
// On entry thread_process() assumes:
// - /tid/ is a thread id of an existing thread.
//
// On return thread_process() guarantees:
// - If the return value is NULL, the thread has no associated process.
// - If the return value is not NULL, the returned pointer points to a process
//   structure most recently associated with the specified process.
//
// See also: running_thread_process(), thread_attach_process(),
// thread_detach_process(), spawn_thread().


extern struct process * running_thread_process(void);

// Returns a pointer to the process structure (`struct process *`) of the
// process associated with the running thread. This function is equivalent to:
// 
//    thread_process(running_thread())
//
// See also: thread_process(), running_thread().


extern void thread_attach(int tid, struct process * proc);

// Sets the process associated with a thread. The /tid/ argument specifies a
// thread and the /proc/ argument specifies the process to associate with that
// thread. The /proc/ argument may be NULL, in which case, the call is
// equivalent to calling thread_detach() with the thread id. If thread /tid/
// already has an associated process, the association with /proc/ replaces it.
//
// On entry thread_attach() assumes:
// - /tid/ is a thread id of an existing thread.
// - /proc/ is a pointer to the process struct of an existing process or NULL.
//
// On return thread_attach() guarantees:
// - If /proc/ is NULL, thread /tid/ is no longer has an associated process.
// - If /proc/ is not NULL, it becomes the process associated thread /tid/.
//
// See also: thread_process(), thread_detach().


extern void thread_detach(int tid);

// Resets the process association of a thread so it is no longer associated with
// a process. The /tid/ argument specifies a thread whose association is to be
// reset.
//
// On entry thread_detach() assumes:
// - /tid/ is a thread id of an existing thread.
// 
// On return thread_detach() guarantees:
// - The thread /tid/ is no longer has an associated process.


extern const char * thread_name(int tid);

// Returns the name of thread /tid/ or NULL. For spawned threads, this is the
// pointer that was passed as the /name/ parameter to spawn_thread().
//
// On entry thread_name() assumes:
// - /tid/ is a thread id of an existing thread.
//
// On return thread_name() guarantees:
// - If thread /tid/ was created using spawn_thread(), the value is the same as
//   the /name/ argument passed to spawn_thread() when /tid/ was created, which
//   may be NULL.
// - If thread /tid/ refers to either the main or the idle thread, the return
//   value is a pointer to a null-terminated string.
//
// See also spawn_thread(), running_thread_name().


extern const char * running_thread_name(void);

// Returns the name of the running thread. This function call is equivalent to:
//
//    thread_name(running_thread())
//
// See also: thread_name(), running_thread().

extern void * running_thread_stack_anchor(void);

// Returns a pointer to the thread stack anchor structure that is placed at the
// base of the running thread's stack. This function is used by functions that
// call trap_frame_jump() to set up the initial kernel stack pointer to be used
// on re-entry into the kernel from user mode. See process_exec() and
// process_fork() for usage.
//
// The thread_stack_anchor struct is defined in thread.c as
//
//     struct thread_stack_anchor {
//        struct thread * ktp;
//        void * kgp;
//     };
//
// This structure is accessed in _smode_trap_entry() in trap.s to recover the
// kernel thread pointer and kernel global pointer values on re-entry into the
// kernel from U mode. The thread stack anchor structure is initialized in
// spawn_thread for spawned threads and by thrmgr_init() for the main and idle
// threads. Because it should not be accessed anywhere else, it is defined in
// thread.c and (implicitly) in trap.s.
//
// See also: _smode_trap_entry in trap.s, spawn_thread(), process_exec(),
// process_fork().


// 
// CONDITION VARIABLES
//

// The main synchronization mechanism in between threads is the _condition
// variable_ represented by a /condition/ structure. All other synchronization
// mechanisms (e.g. readers-writer locks) are constructed using condition
// variables.

// The /thread_list/ structure is used internally by the thread manager and must
// not be accessed outside thread.c. (Inside thread.c use only the provided
// tl-prefixed functions.) It is included here because it is required for the
// definition of the condition structure below.

struct thread_list {
    // NO DIRECT ACCESS!
    struct thread * head;
    struct thread * tail;
};

// A /condition/ structure represents a condition variable. The definition
// should be treated as opaque; it is provided here so that condition variables
// can be allocated statically. Do not access members of a /condition/ structure
// directly; all operations on condition variables should be via the functions
// declared below.

struct condition {
    // NO DIRECT ACCESS!
    const char * name; // optional
	struct thread_list wait_list;
};

extern void condition_init(struct condition * cond, const char * name);

// Initializes a condition variable. The /cond/ argument must point to a region
// of memory large enough to hold an instance of `struct condition`. (This
// function does _not_ allocate space for this structure.) The /name/ argument
// must be a pointer to a null-terminated string or NULL, which will serve as
// the name of the condition variable for diagnostics and debugging. The value
// of the /name/ argument be be retrived later using condition_name().
//
// The condition variable is initialized to a state with no threads waiting for
// the condition.
//
// As an alternative to calling this function, the condition structure may be
// zero-initialized. The result of zero-initializing the memory of a `struct
// condition` variable is equivalent to calling condition_init() with a NULL
// /name/ argument.
//
// On entry condition_init() assumes:
// - /cond/ is a valid pointer to a region of memory large enough to hold an
//   instance of `struct condition`.
// - /name/ is a valid pointer to a null-terminated string or NULL.
//
// On return condition_init() guarantees:
// - /cond/ points to an initialized instance of a condition variable.
// - No threads are considered to be waiting on the condition variable.
//
// Performance guarantees:
// - The number of condition variables in the system is unlimited.
//
// See also condition_name(), condition_wait(), condition_broadcast().


static inline const char * condition_name(const struct condition * cond);

// Returns the name of a condition variable. The return value is the /name/
// argument passed to condition_init() when condition /cond/ was initialized.
// Note that the name may be NULL. However, if not NULL, it is a pointer to a
// null-terminated string.
//
// See also: condition_init().


extern void condition_wait(struct condition * cond);

// Suspends the current thread until a condition is _signalled_ by a call to
// condition_broadcast() from another thread or from an interrupt service
// routine. The /cond/ argument must be a properly initialized condition
// variable. This function will cause a context switch and must not be called
// from an ISR. Note that interrupts are re-enabled during a context switch and
// restored to their original enabled/disabled state when the thread is resumed.
// In cases where a thread needs to wait for a condition to be signalled by an
// ISR, condition_wait() should be called with interrupts disabled to ensure
// avoid a race condition.
//
// On entry condition_wait() assumes:
// - /cond/ is a pointer to properly initialized condition structure (either
//   zero-initialized or initialized using condition_init()).
//
// On return condition_wait() guarantees:
// - The associated condition was signalled using condition_broadcast() at least
//   once between entry into condition_wait() and its return.
//
// This function may context-switch.
//
// Performance guarantees:
// - The number of threads waiting on a condition variable is unlimited.
//
// While a call to condition_broadcast() is guaranteed to wake up all waiting
// threads, there are _no_ guarantees about the order in which such threads will
// be resumed.
//
// Despite the name _condition variable_, there no no guarantee that any
// particular condition, in the broad sense of the word, holds when
// condition_wait() returns.
//
// See also: condition_init(), condition_broadcast().

extern void condition_broadcast(struct condition * cond);

// Wakes up all threads waiting on a condition. The /cond/ argument must point
// to a properly initialized `struct condition`. All threads currently waiting
// on this condition are woken up (i.e., they become runnable). Calling this
// function does not cause a context switch from the currently running thread
// and is safe to call from an ISR.
//
// On entry condition_wait() assumes:
// - /cond/ is a pointer to properly initialized condition structure.
//
// On return condition_wait() guarantees:
// - All threads that were waiting on the condition associated with /cond/ on
//   entry are woken up.
// - No threads are considered to be waiting on the condition variable.
//
// Note that there are _no_ guarantees about the order in which woken threads
// will be executed.
//
// See also: condition_init(), condition_wait().

//
// READERS-WRITER LOCK
//

// The thread manager provides a readers-writer lock (also called a
// shared-exclusive lock) for managing access to shared resources between
// threads. The lock may either be _unlocked_, held _shared_ by one or more
// threads, or held _exclusvely_ by a single thread. Recursive locking is
// supported: A thread may acquire the lock as a reader multiple times, in which
// case it must also release it the same number of times in order to be
// considered no longer holding the lock. A thread may acquire the lock as a
// writer multiple times, in which case it must also release the lock the same
// number of times in order to be considered no longer holding the lock. If a
// thread acquired the lock multiple times, it is said to be holding the lock
// _with multiplicity_.
//
// The following important restrictions must be followed:
//
// - A thread must _not_ attempt to release a lock it is not holding,
// - A thread must _not_ attempt to acquire a lock as a reader if it is already
//   holding the lock as a writer, and vice versa.
// - None of the readers-writer lock functions may be called from an ISR.
//
// The /rwlock/ structure defined below implements a readers-writer lock. The
// definition is given here to allow locks to be allocated statically. The
// structure must only be manipulated using the functions declared below.

struct rwlock {
    // NO DIRECT ACCESS!
    struct condition released;
    struct thread * owner;
    unsigned long cnt;
};

// EXPORTED FUNCTION DECLARATIONS
//

extern void rwlock_init(struct rwlock * rwlk);

// Initializes a readers-writer (shared-exclusive) lock in the _unlocked_ state.
// Any thread may acquire it shared (as a reader) or exclusively (as a writer).
//
// On entry rwlock_init() assumes:
// - /rwlk/ is a valid pointer to a region of memory large enough to hold an
//   instance of `struct rwlock`.
//
// On return rwlock_init() guarantees:
// - /rwlk/ points to an initialized instance of a reader-writers lock.
// - No threads are considered to be holding the lock.
//
// Performance guarantees:
// - The number of readers-writer locks in the system is unlimited.

extern void rwlock_acquire_shared(struct rwlock * rwlk);

// Acquires a readers-writer lock as a reader (shared). If the lock is currently
// held by a writer (exclusively), the running thread is put to sleep until the
// lock can be acquired fo reading. This function may not be called if the
// running thread currently holds the lock as a writer (exclusively). This
// function may not be called from an ISR. The lock must have been previously
// initialized by rwlock_init() or its equivalent.
//
// A thread may acquire lock multiple times as a reader (recursive locking), in
// which case it is said to be holding the lock as a reader _with multiplicity_.
// In this case, rwlock_release_shared() must be called an equal number of times
// for the running thread to be considered no longer holding the lock.
//
// Multiple threads may hold a readers-writer lock as a reader simultaneously.
// No thread may acquire the lock as a writer (exclusively) while there is at
// least one thread holding the lock as a reader.
//
// On entry rwlock_acquire_shared() assumes:
// - /rwlock/ is a pointer to properly initialized readers-writer lock (either
//   zero-initialized or initialized using rwlock_init()).
// - The lock is not already held by the running thread as a writer.
//
// On return rwlock_acquire_shared() guarantees:
// - The current thread is considered to be holding the lock as a reader.
//
// This function may context-switch.
//
// Performance guarantees:
// - The number of threads waiting to acquire a shared lock is unlimited.
// - If the lock is not currently held exclusively, rwlock_acquire_shared()
//   returns immediately without a context switch.
//
// See also: rwlock_init(), rwlock_release_shared().

extern void rwlock_release_shared(struct rwlock * rwlk);

// Releases a lock previously acquired as a reader (shared). The running thread
// _must_ be holding the lock as a reader. That is, the number of times the
// running thread has called rwlock_acquire_shared() minus the number of times
// it has called rwlock_relase_shared() on the lock must be greater than zero.
// If the running thread is holding the lock with multiplcity, the number of
// times it is said to be holding the lock is decreased by one.
//
// On entry rwlock_acquire_shared() assumes:
// - The lock is currently held as a reader by the running thread. That is, the
//   the number of times the running thread has called rwlock_acquire_shared()
//   minus the number of times it has called rwlock_relase_shared() on the lock
//   is greater than zero.
//
// On return rwlock_acquire_shared() guarantees:
// - If the running thread is holding the lock with multiplicity greater than
//   one, the multiplity is reduced by one.
// - If the running thread is holding the lock with multiplity one, the thread
//   is no longer considered to be holding the lock.
// - If any threads are waiting to acquire /rwlk/ as a writer (exclusively),
//   exactly one such thread will be allowed to acquire the lock as a writer.
//
// See also: rwlock_acquire_shared().

extern void rwlock_acquire_exclusive(struct rwlock * rwlk);

// TODO

extern void rwlock_release_exclusive(struct rwlock * rwlk);

// TODO

//
// INLINE FUNCTION DEFINITIONS
//

static inline const char * condition_name(const struct condition * cond) {
    return cond->name;
}

#endif // _THREAD_H_