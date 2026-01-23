// main.c - main function of the kernel (called from start.s)
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

#ifndef MP2
#define INITEXE "shell"

#define CMNTNAME "c" // lffs
#define DMNTNAME "d" // ktfs
#define DEVMNTNAME "dev"
#define CDEVNAME "vioblk"
#define CDEVINST 1
#define DDEVNAME "vioblk"
#define DDEVINST 0

#ifndef NUART // number of UARTs
#define NUART 3
#endif
#endif

#ifndef MP2
static void exec_init();
static void mount_cdrive(void); // mount primary storage device ("C drive")
static void mount_ddrive(void); // mount secondary storage device ("D drive")
#else
static void run_games(void);
#endif // MP2


extern void board_init(void); // from board/xxx.c
extern void attach_devices(void); // from board/xxx.c

void main(void) {
    board_init();
    intrmgr_init();
    devmgr_init();
    thrmgr_init();

#ifndef MP2
    // MP3 stuff
    mount_cdrive();
    mount_ddrive();
#endif

    attach_devices();
    enable_interrupts();

#ifndef MP2
    exec_init();
#else
    run_games();
#endif
}

void mount_cdrive(void) {
    struct storage * hd;
    struct cache * cache;
    int result;

    hd = find_storage(CDEVNAME, CDEVINST);

    if (hd == NULL) {
        kprintf("Storage device %s%d not found\n", CDEVNAME, CDEVINST);
        halt();
    }

    result = storage_open(hd);

    if (result != 0) {
        kprintf("storage_open failed on %s%d: %s\n",
            CDEVNAME, CDEVINST, error_name(result));
        halt();
    }

    result = create_cache(hd, &cache, LFFS_BLKSZ);

    if (result != 0) {
        kprintf("create_cache(%s%d) failed: %s\n",
            CDEVNAME, CDEVINST, error_name(result));
        halt();
    }

    result = mount_lffs(CMNTNAME, cache, storage_capacity(hd));

    if (result != 0) {
        kprintf("mount_lffs(%s, cache(%s%d)) failed: %s\n",
            CMNTNAME, CDEVNAME, CDEVINST, error_name(result));
        halt();
    }
}

void mount_ddrive(void) {
    struct storage * hd;
    struct cache * cache;
    int result;

    hd = find_storage(DDEVNAME, DDEVINST);

    if (hd == NULL) {
        kprintf("Storage device %s%d not found\n", DDEVNAME, DDEVINST);
        halt();
    }

    result = storage_open(hd);

    if (result != 0) {
        kprintf("storage_open failed on %s%d: %s\n",
            DDEVNAME, DDEVINST, error_name(result));
        halt();
    }

    result = create_cache(hd, &cache, KTFS_BLKSZ);

    if (result != 0) {
        kprintf("create_cache(%s%d) failed: %s\n",
            DDEVNAME, DDEVINST, error_name(result));
        halt();
    }

    result = mount_ktfs(DMNTNAME, cache);

    if (result != 0) {
        kprintf("mount_ktfs(%s, cache(%s%d)) failed: %s\n",
            DMNTNAME, DDEVNAME, DDEVINST, error_name(result));
        halt();
    }
}

void exec_init() {
    char * argv[] = { NULL };
    struct uio * initexe;
    int result;
    
    result = open_file(CMNTNAME, INITEXE, &initexe);

    if (result != 0) {
        kprintf(INITEXE ": %s; terminating\n", error_name(result));
        halt();
    }

    // Make descriptor 0 be a null uio object, which the shell will need

    current_process()->uiotab[0] = create_null_uio();

    process_exec(initexe, 0, argv);
}

#ifdef MP2
#define TREK_TERM_NAME "uart1"
#define RULE30_TERM_NAME "uart2"
#define RNG_NAME "viorng0"
#define RTC_NAME "rtc"

void run_games(void) {
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

    // Open a RNG to get a PRNG seed. Try, in decreasing order of preference:
    // viorng0 device, rtc device, and rdtime().

    result = open_device(RNG_NAME, &rngio);

    if (result != 0)
        result = open_device(RTC_NAME, &rngio);

    if (result == 0)
        result = ioread(rngio, &rngseed, 8);
    
    if (result != 0)
        rngseed = rdtime();

#if 1 // Set to 1 for MP3cp3

    // Open UART for rule30.

    result = open_device(RULE30_TERM_NAME, &rule30_term);

    if (rule30_term == NULL) {
        kprintf("%s: %s\n", RULE30_TERM_NAME, error_name(result));
        halt();
    }

    spawn_thread("rule30", (void(*)(void))&rule30_start, rule30_term);
#else
    (void)rule30_term; // suppress unused variable warning
#endif

    trek_start(trek_term, rngseed);
}

// Get rid of these once we have trek and rule30 working.

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

#endif