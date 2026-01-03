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

#include <stddef.h>

#include "conf.h"
#include "intr.h"
#include "misc.h"
#include "riscv.h"
#include "see.h"   // set_stcmp()
#include "thread.h"

// COMPILE-TIME PARAMETERS
//

// PREEMPT_FREQ is the preemption tick interval in ms.

#define PREEMPT_FREQ 20

// EXPORTED GLOBAL VARIABLES
//

char timer_initialized = 0;

// INTERNAL GLOBAL VARIABLES
//

// Alarm used to periodically trigger preemption. This alarm is inserted into
// the same pending-alarm list as user alarms, but it will never block in
// alarm_sleep().

static struct alarm preempt_alarm;

// Head of the pending-alarm list. The list is sorted by twake (ascending).
// The list is manipulated with interrupts disabled.

static struct alarm * sleep_list;

// EXPORTED FUNCTION DEFINITIONS
//

void timer_init(void)
{
    set_stcmp(UINT64_MAX);
    timer_initialized = 1;

    alarm_init(&preempt_alarm, "preempt");
    
    //Doesn't block for special preempt alarm
    alarm_sleep_ms(&preempt_alarm, PREEMPT_FREQ);
}

void alarm_init(struct alarm * al, const char * name)
{
    condition_init(&al->cond, name ? name : "alarm");
    al->twake = rdtime();
    al->next = NULL;
}

void alarm_sleep(struct alarm * al, unsigned long long tcnt)
{
    unsigned long long now;
    struct alarm * prev;
    int pie;

    now = rdtime();

    // Advance the alarm wake-up time relative to its current epoch. If the
    // addition would wrap, saturate to UINT64_MAX.

    if (UINT64_MAX - al->twake < tcnt)
        al->twake = UINT64_MAX;
    else
        al->twake += tcnt;

    // If the wake-up time has already passed, return without sleeping.

    if (al->twake < now)
        return;

    pie = disable_interrupts();

    if (sleep_list == NULL || al->twake <= sleep_list->twake) {
        // Insert at head of list. This requires updating the compare register.

        debug("[%lu] Inserting alarm %s at head of list",
            now, al->cond.name);

        al->next = sleep_list;
        sleep_list = al;

        set_stcmp(al->twake);
        csrs_sie(RISCV_SIE_STIE);
    } else {
        // Insert into list in wake-up order. Interrupts remain disabled during
        // the walk.

        for (prev = sleep_list; prev->next != NULL; prev = prev->next) {
            if (al->twake <= prev->next->twake) {
                debug("[%lu] Inserting alarm %s after %s",
                    now, al->cond.name, prev->cond.name);

                al->next = prev->next;
                prev->next = al;
                break;
            }
        }

        // End of list: insert at tail.

        if (prev->next == NULL) {
            debug("[%lu] Inserting alarm %s at tail",
                now, al->cond.name);

            al->next = NULL;
            prev->next = al;
        }
    }

    debug("[%lu] Next timer interrupt set for %llu ticks",
        now, sleep_list->twake);

    // condition_wait() must be inside the interrupt-disabled region to avoid a
    // race where the alarm is signalled before we begin waiting.
    //
    // The preemption alarm is a special internal alarm: it is used only to
    // schedule periodic timer interrupts and must never block.

    if (al != &preempt_alarm)
        condition_wait(&al->cond);

    restore_interrupts(pie);
}

void alarm_reset(struct alarm * al)
{
    al->twake = rdtime();
}

void alarm_sleep_sec(struct alarm * al, unsigned int sec)
{
    alarm_sleep(al, (unsigned long long)sec * (unsigned long long)TIMER_FREQ);
}

void alarm_sleep_ms(struct alarm * al, unsigned long ms)
{
    alarm_sleep(al, (unsigned long long)ms *
        (unsigned long long)(TIMER_FREQ / 1000));
}

void alarm_sleep_us(struct alarm * al, unsigned long us)
{
    alarm_sleep(al, (unsigned long long)us *
        (unsigned long long)(TIMER_FREQ / 1000 / 1000));
}

void sleep_sec(unsigned int sec)
{
    sleep_ms(1000UL * (unsigned long)sec);
}

void sleep_ms(unsigned long ms)
{
    sleep_us(1000UL * ms);
}

void sleep_us(unsigned long us)
{
    struct alarm al;

    alarm_init(&al, "sleep");
    alarm_sleep_us(&al, us);
}

void handle_timer_interrupt(void)
{
    struct alarm * head;
    struct alarm * next;
    uint64_t now;
    char preempt_seen;

    head = sleep_list;
    preempt_seen = 0;

    now = rdtime();

    trace("[%lu] %s()", now, __func__);
    debug("[%lu] stcmp programmed for %lu", now, (unsigned long)rdtime());

    while (head != NULL && head->twake <= now) {
        debug("[%lu] Broadcasting alarm for %s", now, head->cond.name);

        condition_broadcast(&head->cond);

        if (head == &preempt_alarm)
            preempt_seen = 1;

        next = head->next;
        head->next = NULL;
        head = next;
    }

    sleep_list = head;

    // If preempt alarm was removed re-insert into sleep list.
    // alarm_sleep_ms() will not block on special preempt alarm.
    if (preempt_seen)
        alarm_sleep_ms(&preempt_alarm, PREEMPT_FREQ);

    head = sleep_list;
 
    // /head/ will always point to a valid alarm since the preempt alarm
    // will always be present in /sleep_list/.
    debug("[%lu] Setting next alarm for %lu", now, head->twake);
    set_stcmp(head->twake);

}
