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
#include "dev/uart.h"
#include "dev/virtio.h"
#include "timer.h"
#include "string.h"
#include "fs.h"
#include "error.h"

#define INIT_NAME "intr"

#ifndef NUART
#define NUART 0
#endif

void main(void) {
    struct serial * uarts[NUART]; // UART 0 is console, will be NULL
    char * argv[] = { NULL };
    struct io * diskio;
    struct io * initio;
    int result;
    int i;
    
    console_init();
    intrmgr_init();
    devmgr_init();
    thrmgr_init();
    memory_init();
    procmgr_init();

    // Attach devices

    for (i = 0; i < UARTCNT; i++)
        uarts[0] = uart_attach((void*)UART_MMIO_BASE(i), UART0_INTR_SRCNO+i);
    
    for (i = 0; i < 8; i++)
        virtio_attach((void*)VIRTIO_MMIO_BASE(i), VIRTIO0_INTR_SRCNO+i);
    
    enable_interrupts();

    result = open_device("vioblk", 0, &diskio);

    if (result != 0) {
        kprintf("open_device(\"vioblk\", 0) returned %s\n", error_name(result));
        halt_failure();
    }
    
    result = fsmount(diskio);

    if (result != 0) {
        kprintf("fsmount() returned %s\n", error_name(result));
        halt_failure();
    }
    
    result = fsopen(INIT_NAME, &initio);

    if (result != 0) {
        kprintf(INIT_NAME ": %s; terminating\n", error_name(result));
        halt_failure();
    }

    process_exec(initio, 0, argv);
}