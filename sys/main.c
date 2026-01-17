// main.c - main function of the kernel (called from start.s)
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "conf.h"
#include "console.h"
#include "intr.h"
#include "device.h"
#include "thread.h"
#include "memory.h"
#include "process.h"
#include "dev/rtc.h"
#include "dev/uart.h"
#include "dev/virtio.h"
#include "dev/ramdisk.h"
#include "timer.h"
#include "string.h"
#include "filesys.h"
#include "error.h"
#include "cache.h"
#include "misc.h" // for halt()
#include "io.h"

#define INITEXE "shell"

#define CMNTNAME "c"
#define DEVMNTNAME "dev"
#define CDEVNAME "vioblk"
#define CDEVINST 0

#ifndef NUART // number of UARTs
#define NUART 3
#endif

#ifndef NVIODEV // number of VirtIO devices
#define NVIODEV 8
#endif

static void attach_devices(void);

// MP2 stuff
// static void run_mp2(void);
// extern void trek_start(struct io * tio, unsigned long rngseed);
// extern void rule30_start(struct io * tio);

static void mount_cdrive(void); // mount primary storage device ("C drive")
static void run_init(void);

void main(void) {
    console_init();
    timer_init(TIMER_FREQ);
    memory_init();

    intrmgr_init();
    devmgr_init();
    thrmgr_init();
    procmgr_init();
    fsmgr_init();

    attach_devices();

    enable_interrupts();

    mount_cdrive();
//    run_init();
//    run_mp2();
    kprintf("Hello world!\n");
}


void attach_devices(void) {
    int i;

    rtc_attach((void*)RTC_MMIO_BASE);

    for (i = 0; i < NUART; i++)
        attach_uart((void*)UART_MMIO_BASE(i), UART0_INTR_SRCNO+i);
    
    for (i = 0; i < NVIODEV; i++)
        attach_virtio((void*)VIRTIO_MMIO_BASE(i), VIRTIO0_INTR_SRCNO+i);

    attach_filesystem(DEVMNTNAME, &devfs);
}

void mount_cdrive(void) {
    // nothing
}

void run_init(void) {
#if 0
    char * argv[] = { NULL };
    struct uio * initexe;
    int result;
    
    result = open_file(CMNTNAME, INITEXE, &initexe);

    if (result != 0) {
        kprintf(INITEXE ": %s; terminating\n", error_name(result));
        halt();
    }

    // Make descriptor 0 be a null uio object, which the shell will need

    current_process()->uiotab[0] = create_nullio();

    process_exec(initexe, 0, argv);
#endif
}

#if 0
#define TREK_DEVNAME "uart1"
#define RULE30_DEVNAME "uart2"
#define RNG_DEVNAME "viorng0"
#define RTC_DEVNAME "rtc"
#define MP2CP3

void run_mp2(void) {
    struct io * trek_term;
    struct io * rngio;
    unsigned long rngseed;
    int result;
    
    result = open_device(TREK_DEVNAME, &trek_term);

    if (result != 0) {
        kprintf("%s: %s\n", TREK_DEVNAME, error_name(result));
        halt();
    }

    // Open a RNG. If We fail to open viorng0, try to use the RTC. If we don't
    // have that, use rdtime().

    result = open_device(RNG_DEVNAME, &rngio);

    if (result != 0) {
        result = open_device(RTC_DEVNAME, &rngio);
        
        if (result != 0)
            rngio = NULL;
    }

    result = ioread(rngio, &rngseed, 8);

#ifdef MP2CP3

    struct io * rule30_term;

    result = open_device(RULE30_DEVNAME, &rule30_term);

    if (rule30_term == NULL) {
        kprintf("%s: %s\n", RULE30_DEVNAME, error_name(result));
        halt();
    }

    spawn_thread("rule30", (void(*)(void))&rule30_start, rule30_term);

#endif

    trek_start(trek_term, rngseed);
}
#endif