/*! @file timer.c
    @brief A timer system
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA

*/

#ifdef TIMER_TRACE
#define TRACE
#endif

#ifdef TIMER_DEBUG
#define DEBUG
#endif

#include "timer.h"
#include "thread.h"
#include "riscv.h"
#include "intr.h"
#include "conf.h"
#include "see.h" // for set_stcmp
#include "misc.h"


#include <stddef.h>

// EXPORTED GLOBAL VARIABLE DEFINITIONS
// 

char timer_initialized = 0;

// INTERNVAL GLOBAL VARIABLE DEFINITIONS
//

static struct alarm * sleep_list;

// INTERNAL FUNCTION DECLARATIONS
//

// EXPORTED FUNCTION DEFINITIONS
//
/**
 * @brief Initializes the timer
 * @param None
 * @return None
 */

void timer_init(void) {
    set_stcmp(UINT64_MAX);
    timer_initialized = 1;
}

/**
 * @brief Initialize alarm fields
 * @param al The alarm object pointer
 * @param name The condition variable
 * @return None
 */
void alarm_init(struct alarm * al, const char * name) {
    condition_init(&al->cond, name ? name : "alarm");
    al->twake = rdtime();
    al->next = NULL;
}

/**
 * @brief Allows the current thread to sleep
 * @param al The alarm object pointer
 * @param tcnt The number of ticks to put the thread to sleep for relative to the most recent initialization, reset, or wake-up event for the alarm object
 * @return None
 */
void alarm_sleep(struct alarm * al, unsigned long long tcnt) {
    unsigned long long now;
    struct alarm * prev;
    int pie;

    now = rdtime();

    // If the tcnt is so large it wraps around, set it to UINT64_MAX

    if (UINT64_MAX - al->twake < tcnt)
        al->twake = UINT64_MAX;
    else
        al->twake += tcnt;
    
    // If the wake-up time has already passed, return

    if (al->twake < now)
        return;
    
    pie = disable_interrupts();

    if (sleep_list == NULL || al->twake <= sleep_list->twake) {
        // Insert alarm at head of alarm list (will require updating stcmp)
        debug("[%lu] Inserting alarm %s at head of list", now, al->cond.name);
        al->next = sleep_list;
        sleep_list = al;
        set_stcmp(al->twake);
        csrs_sie(RISCV_SIE_STIE);
    } else {
        // Insert current alarm in list in order of wake-up time. Ideally, we
        // should not keep interrupts disabled while we iterate through the
        // list. The right way would be to restore interrupt state, find the
        // place on the list where we want to insert, the disable interrupts and
        // re-check the insert point.

        for (prev = sleep_list; prev->next != NULL; prev = prev->next) {
            if (al->twake <= prev->next->twake) {
                debug("[%lu] Inserting alarm %s after %s",
                    now, al->cond.name, prev->cond.name);
                al->next = prev->next;
                prev->next = al;
                break;
            }
        }

        // End of list, insert at tail
        if (prev->next == NULL) {
            debug("%[lu] Inserting alarm %s at tail", al->cond.name);
            al->next = NULL;
            prev->next = al;
        }
    }

    debug("[%lu] Next timer interrupt set for %llu ticks", now, sleep_list->twake);

    // Note: condition_wait must be *inside* intr_disable/restore_interrupts block to
    // prevent a race condition where an alarm is signalled before we call
    // condition_wait.

    condition_wait(&al->cond);

    restore_interrupts(pie);
}

// Resets the alarm so that the next sleep increment is relative to the time
// alarm_reset is called.

/**
 * @brief Resets the alarm against each epoch (now)
 * @param al The alarm object pointer
 * @return None
 */
void alarm_reset(struct alarm * al) {
    al->twake = rdtime();
}

/**
 * @brief Allows an alarm to trigger after a specified number of seconds
 * @param al The alarm object pointer
 * @param sec The number of seconds to sleep
 * @return None
 */
void alarm_sleep_sec(struct alarm * al, unsigned int sec) {
    alarm_sleep(al, sec * TIMER_FREQ);
}

/**
 * @brief Allows an alarm to trigger after a specified number of milliseconds 
 * @param al The alarm object pointer
 * @param ms The number of milliseconds to sleep
 * @return None
 */
void alarm_sleep_ms(struct alarm * al, unsigned long ms) {
    alarm_sleep(al, ms * (TIMER_FREQ / 1000));
}

/**
 * @brief Allows an alarm to trigger after a specified number of microseconds
 * @param al The alarm object pointer
 * @param us The number of microseconds to sleep
 * @return None
 */
void alarm_sleep_us(struct alarm * al, unsigned long us) {
    alarm_sleep(al, us * (TIMER_FREQ / 1000 / 1000));
}

/**
 * @brief Pauses program execution for a specified number of seconds
 * @param sec An unsigned integer which is the number of seconds to sleep
 * @return None
 */
void sleep_sec(unsigned int sec) {
    sleep_ms(1000UL * sec);
}

/**
 * @brief Pauses program execution for a specified number of milliseconds
 * @param ms A unsigned integer which is the number of milliseconds to sleep
 * @return None
 */
void sleep_ms(unsigned long ms) {
    sleep_us(1000UL * ms);
}

/**
 * @brief Pauses program execution for a specified number of microseconds
 * @param us A unsigned integer which is the number of microseconds to sleep
 * @return None
 */
void sleep_us(unsigned long us) {
    struct alarm al;

    alarm_init(&al, "sleep");
    alarm_sleep_us(&al, us);
}

/**
 * @brief Service timer interrupts
 * @param None
 * @return None
 */

void handle_timer_interrupt(void) {
    struct alarm * head = sleep_list;
    struct alarm * next;
    uint64_t now;

    now = rdtime();

    trace("[%lu] %s()", now, __func__);
    debug("[%lu] mtcmp = %lu", now, rdtime());

    while (head != NULL && head->twake <= now) {
        debug("[%lu] Broadcasting alarm for %s", now, head->cond.name);
        condition_broadcast(&head->cond);
        next = head->next;
        head->next = NULL;
        head = next;
    }

    sleep_list = head;

    if (head == NULL) {
        debug("[%lu] No alarms pending: timer interrupt disabled ", now);
        csrc_sie(RISCV_SIE_STIE);
    } else {
        debug("[%lu] Setting next alarm for %lu ", now, head->twake);
        set_stcmp(head->twake);
    }
}