// timer.h - Timer and Alarms
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

#include "conf.h"   // TIMER_FREQ
#include "thread.h" // struct condition

// SYSTEM TIMER
//
// The timer subsystem provides two services: putting a thread to sleep until
// some future time and periodic routines.

extern char timer_initialized;
extern unsigned int timer_frequency;
extern void timer_init(unsigned int freq);

// Initializes the timer subsystem. The /freq/ argument is the system timer
// frequency is ticks per second. This function must be called once at system
// initialization time before any other function declared in timer.h is used;
// timer_init() is normally called by the board initialization function.

// After successful initialization, timer_init() set the global variable
// /timer_initialized/, which is statically initialized to 0, to 1, indicating
// that the timer subsystem is now ready to provide alarm and periodic routine
// services. The /timer_frequency/ global variable, also initially 0, is set to
// /freq/. Global variables /timer_initialized/ and /timer_frequency/ must not
// be modified externally.
//
// Interrupts must be enabled for the timer system to function, however, they
// need not be enabled when timer_init() is called.
//
// Both one-time alarms and periodic routines are based on the system timer. The
// system timer can be read using the rdtime() function (riscv.h).
//
// On return timer_init() guarantees:
// - /timer_initialized/ is set to 1.
//
// * This function must be called once at system initialization time before any
//   other functions declared in timer.h.
//
// See also: alarm_init(), start_period_rountine().


extern void handle_timer_interrupt(void);

// Services a timer interrupt. This function must be called by the system
// interrupt handler in response to a supervisor timer interrupt.
//
// On return handle_timer_interrupt() guarantees:
// - All suspended threads that were waiting for an alarm not later than when
//   handle_timer_interrupt() was called become RUNNABLE.
// - If there are suspended threads still waiting for an alarm, the next timer
//   interrupt is set to occur no later than the earliest alarm time of any
//   threads still waiting for future alarms.
// - If there are any periodic routines, the next timer interrupt is set to
//   occur no later than the earliest next activation time of all such routines.


// ONE-TIME ALARMS
//

// An /alarm/ can be used to put the calling thread to sleep until a specified
// future time, expressed in timer ticks, which increment at the same rate as
// the CPU timer returned by rdtime().
//
// An alarm can be used repeatedly, but only by one thread at a time.
//
// The timer interrupt handler wakes sleeping threads by broadcasting the alarm
// condition variables associated with expired alarms.
//
// The timer subsystem uses the RISC-V hardware timer associated with the
// supervisor timer interrupt, and is intended to be the sole subsystem using
// that timer. The timer subsystem manages the STIE (supervisor timer interrupt
// enable) bit of the /sie/ register. This bit must _not_ be modified outside
// timer.c after timer_init() is called.
//
// The timer system does _not_ modify the global interrupt enable state except
// during short critical sections.
//
// Interrupts must be enabled for the timer subsystem to provide alarm service.
// The system interrupt handler _must_ call handle_timer_interrupt() declared
// below to servie timer interrupts.

// The /alarm/ structure represents an alarm. The definition is provided so
// alarms can be allocated statically. Do not access members of alarm struct
// directly; use only the functions declared below.

struct alarm {
    // NO DIRECT ACCESS!
    struct condition cond;
    struct alarm * next;
    unsigned long long twake;
};

extern void alarm_init(struct alarm * al, const char * name);

// Initializes an alarm. The /al/ argument must point to a region of memory
// large enough to hold an instance of `struct alarm`. This function does _not_
// allocate space for this structure.
//
// The /name/ argument must be a pointer to a null-terminated string or NULL,
// which will serve as the name of the alarm. If /name/ is NULL, the alarm is
// given an implementation-defined name. The value of the /name/ argument may be
// retrived later using alarm_name().
//
// When an alarm is no longer needed, the memory associated with the /alarm/
// structure may be reclaimed. After that point, a pointer to this structure is
// no longer considered to be a valid alarm and must not be used. It is safe,
// however, to initialize and use a stack-allocated /alarm/ structure, provided
// that the alarm is only used during the lifetime of the structure.
//
// On entry alarm_init() assumes:
// - /al/ is a pointer to a region of memory large enough to hold an instance of
//   an /alarm/ structure.
// - /name/ is a pointer to a null-terminated string or NULL.
//
// On return alarm_init() guarantees:
// - /al/ is a pointer to an initialized instance of an alarm.
//
// Performance guarantees:
// - The number of alarms in the system is unlimited.
//
// * This function may _not_ be called from an ISR.
//
// See also: alarm_sleep_until().

extern void alarm_sleep_until(struct alarm * al, unsigned long long twake);

// Suspends the calling thread until the specified time. The /twake/ argument
// specifies the time at which the calling thread should be resumed. The /twake/
// argument is an absolute time referenced to the same reference clock as the
// rdtime() function (riscv.h).
//
// If /twake/ is at or after the current time, as obtained by rdtime(),
// alarm_sleep_until() returns immediately. Otherwise, the calling thread is
// suspended until /twake/.
//
// On entry alarm_sleep_until() assumes:
// - /al/ is a pointer to properly initialized alarm structure.
//
// On return alarm_sleep_until() guarantees:
// - The current time, as returned by rdtime(), it not before /twake/.
//
// Performance guarantees:
// - If alarm_sleep_until() is called at or after /twake/, it returns
//   immediately without suspending the calling thread.
// - The calling thread becomes RUNNABLE no later than after the next call to
//   handle_timer_interrupt() after /twake/ returns.

// * This function may switch to another thread context.
// * This function may _not_ be called from an ISR.
//
// See also: alarm_sleep_ms(), alarm_sleep_us(), alarm_sleep_sec().

extern void alarm_sleep_sec(struct alarm * al, unsigned int sec);
extern void alarm_sleep_ms(struct alarm * al, unsigned long ms);
extern void alarm_sleep_us(struct alarm * al, unsigned long us);

// Convenience wrappers around alarm_sleep_until() that sleep for a duration
// expressed in seconds, milliseconds, or microseconds. The /sec/, /ms/, and
// /us/ arguments express a _duration_. The calling thread is suspended for the
// specified duration relative to the current time as returned by rdtime().
//
// These functions are equivalent to calling alarm_sleep_until() with /twake/
// set to the current time plus a number of clock ticks equal to the expressed
// duration.
//
// * These functions may switch to another thread context.
// * These functions may _not_ be called from an ISR.

extern void sleep_sec(unsigned int sec);
extern void sleep_ms(unsigned long ms);
extern void sleep_us(unsigned long us);

// Convenience sleep functions that create a temporary alarm and sleep for the
// specified duration.
//
// * These functions may switch to another thread context.
// * These functions may _not_ be called from an ISR.

// PERIODIC ROUTINES
//

typedef unsigned int (*timer_rfun_t) (
    unsigned int period,
    unsigned long long tnow,
    void * aux
);

extern int start_routine(timer_rfun_t rfun, unsigned int period, void * rfaux);
extern void stop_routine(int tag);

#endif // _TIMER_H_
