// opirv.c - Orange Pi RV system initialization
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include <stddef.h>

#include "console.h" // console_init();
#include "memory.h" // memory_init();

// Run-time configuration
//

#define RAM_SIZE_MB (2*1024)

// Board constants
//

#define RAM_START_PMA 0x40000000UL

#ifndef TIMER_FREQ
#define TIMER_FREQ 10000000UL  // FIXME!
#endif

// MMIO addresses (FIXME!)
//

#ifndef PLIC_MMIO_BASE
#define PLIC_MMIO_BASE 0x0C000000L
#endif

#define NUART 3
#define UART0_MMIO_BASE 0x10000000UL  // PMA
#define UART1_MMIO_BASE 0x10010000UL  // PMA
#define UART_MMIO_BASE(i) (UART0_MMIO_BASE + (i) * (UART1_MMIO_BASE - UART0_MMIO_BASE))
#define UART0_INTR_SRCNO 20

// Additional derived constants
//

#define RAM_SIZE ((size_t)RAM_SIZE_MB * 1024 * 1024)
#define RAM_START ((void*)RAM_START_PMA)
#define RAM_END_PMA (RAM_START_PMA + RAM_SIZE)
#define RAM_END (RAM_START + RAM_SIZE)

#define RSV_START_PMA RAM_START
#define RSV_END_PMA (RAM_START_PMA + 0x80000)

// Device attach function declarations
//

static struct mregion opi_ram[] = {
	{0x40000000, 2UL * 1024 * 1024 * 1024}, {0x0, 0}
};

static struct mregion opi_mmio[] = {
	{0x0, 0x40000000}, {0x0, 0}
};

static struct mregion opi_resv[] = {
	{0x40000000, 0x80000}, {0x0, 0}
};

static struct matlas opi_matlas = {
	.ram = opi_ram,
	.mmio = opi_mmio,
	.resv = opi_resv,

	.ram_size = 1,
	.mmio_size = 1,
	.resv_size = 1
};

extern void plic_init(void * mmio_base); // plic.c
extern void timer_init(unsigned int freq); // timer.c

extern void attach_dwuart(void * mmio_base, int irqno); // dev/dw-uart.c
extern void attach_virtio(void * mmio_base, int irqno); // dev/virtio.c

void board_init(unsigned int hartid, void * dtb) {    
    console_init();
    plic_init((void*)PLIC_MMIO_BASE);
    timer_init(TIMER_FREQ);

    memory_init(&opi_matlas);
}

void attach_board_devices(void) {
    int i;

    for (i = 0; i < NUART; i++)
        attach_dwuart((void*)UART_MMIO_BASE(i), UART0_INTR_SRCNO+i);
}
