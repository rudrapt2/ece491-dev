// timer.c - Timer and Alarms
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef TIMER_TRACE
#define TRACE
#endif

#ifdef TIMER_DEBUG
#define DEBUG
#endif

#include "timer.h"
#include "thread.h"
#include "riscv.h"
#include "sbi.h" // for sbi_set_timer
#include "intr.h"
#include "misc.h"
#include "string.h"

#include <stddef.h>
#include <limits.h> // for ULLONG_MAX

// COMPILE-TIME OPTIONS
//

#ifndef BOLT_FREQ
#define BOLT_FREQ 50 // Hz
#endif

// INTERNAL TYPE DEFINITIONS
//

struct timer_alarm {
    struct timer_alarm * next;
    struct condition woken;
    unsigned long long twake;
};

// EXPORTED GLOBAL VARIABLES
//

char timer_initialized = 0;
unsigned int timer_frequency = 0;


// INTERNAL GLOBAL VARIABLES
//

static struct timer_alarm * sleep_list; // list of pending alarms

static unsigned long long tbolt; // next system periodic interrupt time
static unsigned int bolt_period; // ticks between system periodic interrupts


// INTERNAL FUNCTION DECLARATIONS
//

static void add_alarm(struct timer_alarm * al);

// Adds an alarm to the sleep_list and adjusts the next timer interrupt time if
// necessary. Must be called with timer interrupts DISABLED.

static void enable_timer_interrupts(void);
static void disable_timer_interrupts(void);

// Enabled and disables timer interrupts (sie.STIE). Does not touch sstatus.SIE.

// EXPORTED FUNCTION DEFINITIONS
//

void timer_init(unsigned int freq) {
    assert (freq > 0);
    timer_frequency = freq;
    bolt_period = freq / BOLT_FREQ;
    timer_initialized = 1;

    sbi_set_timer(0); // interrupt immediately
    enable_timer_interrupts();
}

void alarm_sleep_until(unsigned long long twake) {
    struct timer_alarm alarm;
    unsigned long long tnow;
    int pie;

    tnow = rdtime();

    trace("[%llu] %s(%llu)", tnow, __func__, twake);

    if (twake < tnow)
        return;

    memset(&alarm, 0, sizeof(alarm));
    alarm.twake = twake;

    // We need to have timer interrupts disabled while modifying the sleep list
    // and changing the S mode timer compare register. We could disable all
    // interrupts, but that is not necessary. Since add_alarm() may traverse the
    // list of current alarms, it is desirable to not keep all interrupts
    // disabled at that time.

    disable_timer_interrupts();
    add_alarm(&alarm);

    // condition_wait() must be inside an interrupt-disabled region to avoid a
    // race where the alarm is signalled before we begin waiting. Now we need to
    // disable all interrupts, but need to re-enable timer interrupts now.
    
    pie = disable_interrupts();
    enable_timer_interrupts();

    while (rdtime() < twake)
        condition_wait(&alarm.woken);
    restore_interrupts(pie);
}

void sleep_sec(unsigned int sec) {
    alarm_sleep_until(1ULL * sec * timer_frequency);
}

void sleep_ms(unsigned int ms) {
    alarm_sleep_until(1ULL * ms * timer_frequency / 1000);
}

void sleep_us(unsigned int us) {
    alarm_sleep_until(1ULL * us * timer_frequency / 1000 / 1000);
}

void handle_timer_interrupt(void) {
    unsigned long long talarm;   // next alarm interrupt time
    unsigned long long tnow;
    struct timer_alarm * head;
    struct timer_alarm * next;

    // This function is guaranteed by assumption not to be called while we are
    // executing any other function in timer.c, either by assumption or by
    // disabling timer interrupts in critical sections elsewhere.

    head = sleep_list;

    tnow = rdtime();

    trace("[%lu] %s()", tnow, __func__);

    while (head != NULL && head->twake <= tnow) {
        debug("[%lu] Waking threads sleeping on <%p>", tnow, head);
        condition_broadcast(&head->woken);

        next = head->next;
        head->next = NULL;
        head = next;
    }

    sleep_list = head;

    // Calculate next system periodic interrupt time and next alarm wake time
    // and set timer interrupt for the earlier of the two

    tbolt = ROUND_UP(tnow+1, bolt_period);
    talarm = (head != NULL) ? head->twake : ULLONG_MAX;
    debug("[%llu] %s(): tbolt = %llu", tnow, __func__, tbolt);
    debug("[%llu] %s(): talarm = %llu", tnow, __func__, talarm);
    debug("[%llu] %s(): Calling sbi_set_timer(%llu)", tnow, __func__, MIN(tbolt, talarm));
    sbi_set_timer(MIN(tbolt, talarm));
}

// INTERNAL FUNCTION DEFINITIONS
//

static void add_alarm(struct timer_alarm * al) {
    struct timer_alarm * prev;

    trace("[%llu] %s({%llu})", rdtime(), __func__, al->twake);
    debug("[%llu] tbolt = %llu", rdtime(), tbolt);

    // Timer interrupts MUST be disabled by caller while we are manipulating the
    // alarm list.

    al->next = NULL;

    if (sleep_list == NULL || al->twake <= sleep_list->twake) {
        // Insert at head of list. If alarm time is before next system periodic
        // interrupt, update the S mode timer compare register.

        al->next = sleep_list;
        sleep_list = al;

        debug("[%llu] %s(): tbolt = %llu", rdtime(), __func__, tbolt);
        debug("[%llu] %s(): al->twake = %llu", rdtime(), __func__, al->twake);

        if (al->twake < tbolt) {
            debug("[%llu] %s(): Calling sbi_set_timer(%llu)", rdtime(), __func__, al->twake);
            sbi_set_timer(al->twake);
        }
        
        return;
    }

    // Insert into list after the first element. We don't need to update the
    // timer compare register.

    for (prev = sleep_list; prev->next != NULL; prev = prev->next) {
        if (al->twake <= prev->next->twake) {
            al->next = prev->next;
            prev->next = al;
            return;
        }
    }

    // End of list reached, insert at tail.

    prev->next = al;
}

void enable_timer_interrupts(void) {
    csrs_sie(RISCV_SIE_STIE);
}

void disable_timer_interrupts(void) {
    csrc_sie(RISCV_SIE_STIE);
}