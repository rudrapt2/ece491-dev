// mp2-main.c - main function of the kernel (called from start.s) for MP2
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "conf.h"
#include "console.h"
#include "intr.h"
#include "plic.h"
#include "device.h"
#include "thread.h"
#include "timer.h"
#include "string.h"
#include "error.h"
#include "misc.h" // for halt()
#include "heap.h"
#include "io.h"

static void run_mp2(void);

extern void board_init(void); // from board/xxx.c
extern void attach_devices(void); // from board/xxx.c

void main(void) {
    board_init();
    intmgr_init();
    devmgr_init();
    thrmgr_init();

    attach_devices();

    enable_interrupts();

    run_mp2();
}

#define TREK_TERM_NAME "uart1"
#define RULE30_TERM_NAME "uart2"
#define RNG_NAME "viorng0"
#define RTC_NAME "rtc"

void run_mp2(void) {
    extern void trek_start(struct io * tio, unsigned long rngseed);
    extern void rule30_start(struct io * tio);
    struct io * rngio;
    struct io * trek_term;
    struct io * rule30_term;
    unsigned long rngseed;
    int result;
    
    // Open UART for trek.

    result = open_device(TREK_TERM_NAME, &trek_term);

    if (result != 0) {
        kprintf("%s: %s\n", TREK_TERM_NAME, error_desc(result));
        halt();
    }

    // Open UART for rule30.

    result = open_device(RULE30_TERM_NAME, &rule30_term);

    if (rule30_term == NULL) {
        kprintf("%s: %s\n", RULE30_TERM_NAME, error_name(result));
        halt();
    }

    // Open a RNG to get a PRNG seed. If We fail to open viorng0, try to use
    // the RTC. If we can't open that, use rdtime().

    result = open_device(RNG_NAME, &rngio);

    if (result != 0)
        result = open_device(RTC_NAME, &rngio);

    if (result == 0)
        result = ioread(rngio, &rngseed, 8);
    
    if (result != 0)
        rngseed = rdtime();

    spawn_thread("rule30", (void(*)(void))&rule30_start, rule30_term);

    trek_start(trek_term, rngseed);
}

void __attribute__ ((weak)) trek_start(struct io * tio, unsigned long rngseed) {
    char msgbuf[120];
    int n;
    
    for (;;) {
        n = snprintf(msgbuf, sizeof(msgbuf),
            "[%llu] This is trek (rngseed = %lu)\r\n", rdtime(), rngseed);
        
        iowrite(tio, msgbuf, n);
        sleep_ms(500);
    }
}

void __attribute__ ((weak)) rule30_start(struct io * tio) {
    char msgbuf[120];
    int n;
    
    for (;;) {
        n = snprintf(msgbuf, sizeof(msgbuf),
            "[%llu] This is rule30\r\n", rdtime());
        iowrite(tio, msgbuf, n);
        sleep_ms(500);
    }
}