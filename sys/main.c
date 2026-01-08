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
static void run_mp2(void);
void trek_start(struct serial* term, unsigned long rngseed);
void rule30_start(struct serial* term);

// static void mount_cdrive(void); // mount primary storage device ("C drive")
// static void run_init(void);

void main(void) {
    console_init();
    intrmgr_init();
    devmgr_init();
    thrmgr_init();
    memory_init();
    procmgr_init();
    fsmgr_init();
    timer_init();

    attach_devices();

    enable_interrupts();

//    mount_cdrive();
//    run_init();
    run_mp2();
}


void attach_devices(void) {
    int i;
    int result;

    rtc_attach((void*)RTC_MMIO_BASE);
    ramdisk_attach();

    for (i = 0; i < NUART; i++)
        attach_uart((void*)UART_MMIO_BASE(i), UART0_INTR_SRCNO+i);
    
    for (i = 0; i < NVIODEV; i++)
        attach_virtio((void*)VIRTIO_MMIO_BASE(i), VIRTIO0_INTR_SRCNO+i);

    result = mount_devfs(DEVMNTNAME);

    if (result != 0) {
        kprintf("mount_devfs(%s) failed: %s\n",
            CDEVNAME, error_name(result));
        halt();
    }
}

void mount_cdrive(void) {
    // nothing
}

void run_init(void) {
#if 1
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
#endif
}

void run_mp2(void) {
    #define MP2CP3
    struct serial* trek_term;
    struct serial* seedsrc;
    unsigned long rngseed;
    int result;
    
    trek_term = find_serial("uart", 1);

    if (trek_term == NULL) {
        kprintf("Serial device uart1 not found\n");
        halt();
    }

    result = serial_open(trek_term);
    if (result != 0) {
        kprintf("failed to open uart 1\n");
        halt();
    }

    seedsrc = find_serial("viorng", 0);
    if (seedsrc == NULL) {
        kprintf("viorng 0 is NULL\n");
    } else {
        kprintf("using viorng as seedsrc\n");
    }

    if (seedsrc == NULL) {
        seedsrc = find_serial("rtc0", 0);
    }


    if (seedsrc != NULL) {
        result = serial_open(seedsrc);
        if (result != 0) {
            kprintf("failed to open seed source device\n");
        }
        result = serial_recv(seedsrc, &rngseed, sizeof(rngseed));
        if (result != sizeof(rngseed)) {
            kprintf("failed to receive from seed source device\n");
        }
        serial_close(seedsrc);
    } else {
        rngseed = 0xECE391;
    }

    rngseed = 0xECE391;
    kprintf("rngseed = %lu\n", rngseed);

#ifdef MP2CP3

    struct serial * rule30_term;

    rule30_term = find_serial("uart", 2);

    if (rule30_term == NULL) {
        kprintf("Serial device uart2 not found\n");
        halt();
    }

    serial_open(rule30_term);

    spawn_thread("rule30", (void(*)(void))&rule30_start, rule30_term);

#endif

    trek_start(trek_term, rngseed);
}
