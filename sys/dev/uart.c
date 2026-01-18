// uart.c - NS8550-compatible UART
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef UART_TRACE
#define TRACE
#endif

#ifdef UART_DEBUG
#define DEBUG
#endif

#include "uart.h"
#include "conf.h"
#include "intr.h"
#include "heap.h"
#include "thread.h"
#include "console.h"
#include "device.h"
#include "misc.h"
#include "ioimpl.h"
#include "error.h"

#include <stdint.h>


// COMPILE-TIME CONSTANT DEFINITIONS
//

#ifndef UART_RBUFSZ
#define UART_RBUFSZ 64
#endif

#ifndef UART_INTR_PRIO
#define UART_INTR_PRIO 1
#endif

#ifndef UART_DEVNAME
#define UART_DEVNAME "uart"
#endif


// INTERNAL TYPE DEFINITIONS
// 

struct uart_regs {
    union {
        char rbr; // DLAB=0 read
        char thr; // DLAB=0 write
        uint8_t dll; // DLAB=1
    };
    
    union {
        uint8_t ier; // DLAB=0
        uint8_t dlm; // DLAB=1
    };
    
    union {
        uint8_t iir; // read
        uint8_t fcr; // write
    };

    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr;
    uint8_t msr;
    uint8_t scr;
};

#define LCR_DLAB (1 << 7)
#define LSR_OE (1 << 1)
#define LSR_DR (1 << 0)
#define LSR_THRE (1 << 5)
#define IER_DRIE (1 << 0)
#define IER_THREIE (1 << 1)

// Simple fixed-size ring buffer

struct ringbuf {
    unsigned int hpos; // head of queue (from where elements are removed)
    unsigned int tpos; // tail of queue (where elements are inserted)
    char data[UART_RBUFSZ];
};

// UART device structure

struct uart_device {
    volatile struct uart_regs * regs;
    int irqno;

    struct io io;

    unsigned long rxovrcnt; // number of times OE was set
    
    struct condition rxbnotempty; // signalled when rxbuf becomes not empty
    struct condition txbnotfull;  // signalled when txbuf becomes not full

    struct ringbuf rxbuf;
    struct ringbuf txbuf;
};

// INTERNAL FUNCTION DEFINITIONS
//

static int uart_open(struct io ** ioptr, void * aux);
static long uart_read(struct io * io, void * buf, long bufsz);
static long uart_write(struct io * io, const void * buf, long len);

static void uart_isr(int srcno, void * aux);

// Ring buffer (struct rbuf) functions

static void rbuf_init(struct ringbuf * rbuf);
static int rbuf_empty(const struct ringbuf * rbuf);
static int rbuf_full(const struct ringbuf * rbuf);
static void rbuf_putc(struct ringbuf * rbuf, char c);
static char rbuf_getc(struct ringbuf * rbuf);

// INTERNAL GLOBAL VARIABLES
//

static const struct iointf uart_intf = {
    .implname = "uart",
    .read = &uart_read,
    .write = &uart_write
};

// EXPORTED FUNCTION DEFINITIONS
// 

void attach_uart(void * mmio_base, int irqno) {
    static unsigned short instcnt = 0; // number of UARTs
    struct uart_device * uart;

    uart = kcalloc(1, sizeof(*uart));

    uart->regs = mmio_base;
    uart->irqno = irqno;

    condition_init(&uart->rxbnotempty, "uart.rxnotempty");
    condition_init(&uart->txbnotfull, "uart.txnotfull");


    // Initialize hardware device

    uart->regs->ier = 0;
    uart->regs->lcr = LCR_DLAB;
    // fence o,o ?
    uart->regs->dll = 0x01;
    uart->regs->dlm = 0x00;
    // fence o,o ?
    uart->regs->lcr = 0; // DLAB=0

    register_device(UART_DEVNAME, instcnt++, &uart_open, uart);
    ioinit(&uart->io, &uart_intf, 1, 0);
}

int uart_open(struct io ** ioptr, void * aux) {
    struct uart_device * const uart = aux;

    trace("%s(%d)", __func__, instno);

    if (iorefcnt(&uart->io) != 0)
        return -EBUSY;
    
    // Reset receive and transmit buffers
    
    rbuf_init(&uart->rxbuf);
    rbuf_init(&uart->txbuf);

    // Read receive buffer register to flush any stale data in hardware buffer

    uart->regs->rbr; // forces a read because uart->regs is volatile

    // Enable interrupts when data ready (DR) status asserted

    uart->regs->ier = IER_DRIE;

    enable_intr_source(uart->irqno, UART_INTR_PRIO, uart_isr, uart);

    *ioptr = ioaddref(&uart->io);
    return 0;
}

void uart_reclaim(struct io * io) {
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);

    trace("%s()", __func__);

    // Disable all interrupts from device

    uart->regs->ier = 0;
    disable_intr_source(uart->irqno);
}

long uart_read(struct io * io, void * buf, long bufsz) {
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);
    long n = 0; // number of bytes copied from ring buffer
    int pie;

    trace("%s(%ld)", __func__, bufsz);
    
    if (bufsz == 0)
        return 0;

    assert (buf != NULL);

    // Sleep until receive buffer is not empty, then copy as much data as we can
    // from ring buffer into /buf/. We can return without filling the buffer.
    // Note that we disable interrupts _before_ doing the copy. This is safe to
    // do (from a program correctness point of view) in our specific case
    // because our ring buffer does not require mutual exclusion between
    // rbuf_getc() and rbuf_putc().

    pie = disable_interrupts();

    while (rbuf_empty(&uart->rxbuf))
        condition_wait(&uart->rxbnotempty);

    restore_interrupts(pie);

    while (n < bufsz && !rbuf_empty(&uart->rxbuf))
        ((char*)buf)[n++] = rbuf_getc(&uart->rxbuf);
    
    uart->regs->ier |= IER_DRIE; // enable receive interrupts
    
    return n;
}

long uart_write(struct io * io, const void * buf, long len) {
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);
    long n = 0; // number of bytes written so far
    int pie;
   
    trace("%s(%ld)", __func__, len);

    if (len == 0)
        return 0;

    assert (buf != NULL);

    // Sleep until transmit ring buffer is not full, then copy characters from
    // _buf_ to ring buffer. Unlike the read case, we write all characters
    // before returning.

    while (n < len) {
        pie = disable_interrupts();

        while (rbuf_full(&uart->txbuf))
            condition_wait(&uart->txbnotfull);

        restore_interrupts(pie);

        while (!rbuf_full(&uart->txbuf) && n < len)
            rbuf_putc(&uart->txbuf, ((const char*)buf)[n++]);
        
        uart->regs->ier |= IER_THREIE;
    }

    return n;
}

void uart_isr(int srcno, void * aux) {
    struct uart_device * const uart = aux;
    const uint_fast8_t line_status = uart->regs->lsr;

    if (line_status & LSR_OE)
        uart->rxovrcnt += 1;
    
    if (line_status & LSR_DR) {
        if (!rbuf_full(&uart->rxbuf)) {
            rbuf_putc(&uart->rxbuf, uart->regs->rbr);
            condition_broadcast(&uart->rxbnotempty);
        } else
            uart->regs->ier &= ~IER_DRIE;
    }

    if (line_status & LSR_THRE) {
        if (!rbuf_empty(&uart->txbuf)) {
            uart->regs->thr = rbuf_getc(&uart->txbuf);
            condition_broadcast(&uart->txbnotfull);
        } else
            uart->regs->ier &= ~IER_THREIE;
    }
}

void rbuf_init(struct ringbuf * rbuf) {
    rbuf->hpos = 0;
    rbuf->tpos = 0;
}

int rbuf_empty(const struct ringbuf * rbuf) {
    return (rbuf->hpos == rbuf->tpos);
}

int rbuf_full(const struct ringbuf * rbuf) {
    return (rbuf->tpos - rbuf->hpos == UART_RBUFSZ);
}

void rbuf_putc(struct ringbuf * rbuf, char c) {
    uint_fast16_t tpos;

    tpos = rbuf->tpos;
    rbuf->data[tpos % UART_RBUFSZ] = c;
    asm volatile ("" ::: "memory");
    rbuf->tpos = tpos + 1;
}

char rbuf_getc(struct ringbuf * rbuf) {
    uint_fast16_t hpos;
    char c;

    hpos = rbuf->hpos;
    c = rbuf->data[hpos % UART_RBUFSZ];
    asm volatile ("" ::: "memory");
    rbuf->hpos = hpos + 1;
    return c;
}