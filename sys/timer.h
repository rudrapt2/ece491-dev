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

// TIMERS AND ALARMS
//
// The timer subsystem provides a simple alarm mechanism. An /alarm/ can be used
// to put the calling thread to sleep until a specified future time, expressed
// in timer ticks (rdtime()).
//
// An alarm can be used repeatedly. Each call to alarm_sleep() advances the
// alarm's internal wake-up time relative to the most recent alarm "epoch"
// (init/reset/wake). Call alarm_reset() to reset the epoch to "now".
//
// The timer interrupt handler wakes sleeping threads by broadcasting the alarm
// condition variables associated with expired alarms.
//
// Note: This interface is designed to be used by threads. The timer interrupt
// handler is responsible for signalling alarms but does not perform a context
// switch itself.

// The /alarm/ structure represents an alarm. The definition is provided so
// alarms can be allocated statically. Do not access members of alarm struct
// directly; use only the functions declared below.

struct alarm {
    struct condition cond;
    struct alarm * next;
    unsigned long long twake;
};

// EXPORTED GLOBAL VARIABLES
//

extern char timer_initialized;

// EXPORTED FUNCTION DECLARATIONS
//

extern void timer_init(void);

// Initializes the timer subsystem. This function must be called once at system
// initialization time before any other function declared in timer.h is used.
// On return, timer interrupts are configured and the timer subsystem is ready
// to manage alarms.
//
// On entry timer_init() assumes:
// - TIMER_FREQ (compile-time configuration) correctly reflects the rdtime()
//   tick rate.
//
// On return timer_init() guarantees:
// - timer_initialized is set to 1.
// - The timer compare register is initialized so that alarms may be scheduled.
//
// * This function may _not_ be called from an ISR.

extern void alarm_init(struct alarm * al, const char * name);

// Initializes an alarm. The /al/ argument must point to a region of memory
// large enough to hold an instance of `struct alarm`. This function does not
// allocate memory for the alarm.
//
// The /name/ argument must be a pointer to a null-terminated string or NULL.
// It is used only for diagnostics and debugging.
//
// The alarm epoch is initialized to the current time (rdtime()).
//
// On entry alarm_init() assumes:
// - /al/ is a non-NULL pointer to writable memory large enough to hold an
//   instance of `struct alarm`.
// - /name/ is a pointer to a null-terminated string or NULL.
//
// On return alarm_init() guarantees:
// - /al/ refers to an initialized alarm whose epoch is the time at which
//   alarm_init() executed.
//
// Performance guarantees:
// - The number of alarms in the system is unlimited.
//
// * This function may _not_ be called from an ISR.
//
// See also: alarm_sleep(), alarm_reset().

extern void alarm_sleep(struct alarm * al, unsigned long long tcnt);

// Suspends the calling thread until the alarm triggers. The /tcnt/ argument
// specifies the number of timer ticks relative to the alarm's current epoch.
// The alarm's wake-up time is advanced by /tcnt/ ticks relative to its previous
// wake-up time (or initialization/reset time).
//
// If the computed wake-up time is in the past relative to rdtime(), this
// function returns immediately without suspending the calling thread.
//
// On entry alarm_sleep() assumes:
// - /al/ points to an initialized alarm.
// - /tcnt/ is a tick count relative to the alarm epoch.
// - The calling context is a thread context (not an ISR).
//
// On return alarm_sleep() guarantees:
// - If alarm_sleep() suspended the calling thread, the alarm's condition
//   variable has been signalled at least once after the alarm's wake-up time.
//
// * This function may switch to another thread context.
// * This function may _not_ be called from an ISR.
//
// See also: alarm_reset(), alarm_sleep_ms(), alarm_sleep_us(), alarm_sleep_sec().

extern void alarm_reset(struct alarm * al);

// Resets the alarm epoch to the current time (rdtime()). The next call to
// alarm_sleep() will interpret its tick count relative to the time of this
// alarm_reset() call.
//
// On entry alarm_reset() assumes:
// - /al/ points to an initialized alarm.
//
// On return alarm_reset() guarantees:
// - The alarm epoch is set to the current time (rdtime()).
//
// * This function may be called from an ISR only if the caller guarantees the
//   alarm is not concurrently used by a thread.

extern void alarm_sleep_sec(struct alarm * al, unsigned int sec);
extern void alarm_sleep_ms(struct alarm * al, unsigned long ms);
extern void alarm_sleep_us(struct alarm * al, unsigned long us);

// Convenience wrappers around alarm_sleep() that sleep for a duration expressed
// in seconds, milliseconds, or microseconds.
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

extern void handle_timer_interrupt(void);

// Services a timer interrupt. This function is called from the trap handler
// when a supervisor timer interrupt fires. It wakes all threads whose alarms
// have expired by broadcasting their alarm condition variables, and programs
// the next timer interrupt (if any alarms remain pending).
//
// On entry handle_timer_interrupt() assumes:
// - The interrupt cause is a supervisor timer interrupt.
// - rdtime() is monotonically non-decreasing.
// - The function runs in an interrupt/trap context.
//
// On return handle_timer_interrupt() guarantees:
// - All alarms with twake <= current rdtime() have been signalled.
// - If there are remaining pending alarms, the next interrupt is scheduled at
//   the earliest pending alarm's wake-up time.
// - If there are no pending alarms, the timer interrupt source is disabled.
//
//
// * This function may be called from an ISR/trap context.
// * This function does not itself perform a context switch, but it can modify
//   the list of threads available to run.

#endif // _TIMER_H_
