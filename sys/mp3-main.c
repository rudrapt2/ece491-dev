// main.c - main function of the kernel (called from start.s)
//
// Copyright (c) 2024-2026 University of Illinois
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
    board_init();

    intrmgr_init();
    devmgr_init();
    thrmgr_init();
    procmgr_init();
    fsmgr_init();

    attach_devices();

    enable_interrupts();

    mount_cdrive();
    kprintf("Hello world!\n");
}

// The following definition of shutdown overrides the default definition in
// misc.c, which is just an alias for halt().

void shutdown(void) {
    flush_all_filesys();
    halt();
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
