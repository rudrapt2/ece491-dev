// ns8250.c - NS8250-compatible UART
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

#include "conf.h"
#include "intr.h"
#include "heap.h"
#include "thread.h"
#include "console.h"
#include "device.h"
#include "misc.h"
#include "ioimpl.h"
#include "error.h"
#include "rbuf.h"

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

struct ns8250_regs {
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

// UART device structure

struct ns8250_intr_device {
    volatile struct ns8250_regs * regs;
    int irqno; // interrupt source number for this UART
    char rxoe; // OE was set on entry to ISR

    struct io io;

    struct condition rxbnotempty; // signalled when rxbuf becomes not empty
    struct condition txbnotfull;  // signalled when txbuf becomes not full
    struct rwlock txlock; // transmit exclusive access

    struct rbuf rxbuf;
    struct rbuf txbuf;
    char rxbufmem[UART_RBUFSZ];
    char txbufmem[UART_RBUFSZ];
};

struct ns8250_poll_device {
    volatile struct ns8250_regs * regs;
    struct io io;
 };

// INTERNAL FUNCTION DEFINITIONS
//

static void attach_ns8250_intr(void * mmio_base, int irqno);
static void attach_ns8250_poll(void * mmio_base);

static int ns8250_intr_open(struct io ** ioptr, void * aux);
static void ns8250_intr_reclaim(struct io * io);
static long ns8250_intr_read(struct io * io, void * buf, long bufsz);
static long ns8250_intr_write(struct io * io, const void * buf, long len);
static int ns8250_intr_ioctl(struct io * io, int op, void * arg);

static int ns8250_poll_open(struct io ** ioptr, void * aux);
static long ns8250_poll_read(struct io * io, void * buf, long bufsz);
static long ns8250_poll_write(struct io * io, const void * buf, long len);

static void ns8250_isr(int srcno, void * aux);
static void ns8250_hwinit(volatile struct ns8250_regs * regs);

// INTERNAL GLOBAL VARIABLES
//

static const struct iointf ns8250_intr_intf = {
    .implname = "ns8250",
    .reclaim = &ns8250_intr_reclaim,
    .read = &ns8250_intr_read,
    .write = &ns8250_intr_write,
    .ioctl = &ns8250_intr_ioctl
};

static const struct iointf ns8250_poll_intf = {
    .implname = "ns8250",
    .read = &ns8250_poll_read,
    .write = &ns8250_poll_write
};

unsigned short uart_instcnt = 0; // number of UARTs


void attach_ns8250(void * mmio_base, int irqno) {
    if (irqno < 0)
        attach_ns8250_poll(mmio_base);
    else
        attach_ns8250_intr(mmio_base, irqno);
}

// EXPORTED FUNCTION DEFINITIONS
// 

void attach_ns8250_intr(void * mmio_base, int irqno) {
    struct ns8250_intr_device * uart;

    uart = kcalloc(1, sizeof(*uart));

    uart->regs = mmio_base;
    uart->irqno = irqno;

    condition_init(&uart->rxbnotempty, "uart.rxnotempty");
    condition_init(&uart->txbnotfull, "uart.txnotfull");
    rwlock_init(&uart->txlock, "uart.txlock");

    assert (ISPOW2(sizeof(uart->rxbufmem)));
    assert (ISPOW2(sizeof(uart->txbufmem)));
    
    rbuf_init(&uart->rxbuf, uart->rxbufmem, sizeof(uart->rxbufmem));
    rbuf_init(&uart->txbuf, uart->txbufmem, sizeof(uart->txbufmem));

    register_device(UART_DEVNAME, uart_instcnt++, &ns8250_intr_open, uart);
    ioinit(&uart->io, &ns8250_intr_intf, 1, 0);
}

void attach_poll_ns8250(void * mmio_base) {
    struct ns8250_poll_device * uart;

    uart = kcalloc(1, sizeof(*uart));

    uart->regs = mmio_base;
    ns8250_hwinit(uart->regs);

    register_device(UART_DEVNAME, uart_instcnt++, &ns8250_poll_open, uart);
    ioinit(&uart->io, &ns8250_poll_intf, 1, 0);
}

int ns8250_intr_open(struct io ** ioptr, void * aux) {
    struct ns8250_intr_device * const uart = aux;

    trace("%s()", __func__);

    if (iorefcnt(&uart->io) != 0)
        return -EBUSY;
    
    // Reset receive and transmit buffers
    
    rbuf_reset(&uart->rxbuf);
    rbuf_reset(&uart->txbuf);
    uart->rxoe = 0;

    // Read RBR to flush any stale data in hardware buffer

    uart->regs->rbr; // forces a read because uart->regs is volatile

    uart->regs->ier = IER_DRIE;
    enable_intr_source(uart->irqno, UART_INTR_PRIO, ns8250_isr, uart);

    *ioptr = ioaddref(&uart->io);
    return 0;
}

void ns8250_intr_reclaim(struct io * io) {
    struct ns8250_intr_device * const uart =
        (void*)io - offsetof(struct ns8250_intr_device, io);

    trace("%s()", __func__);

    // Disable all interrupts from device

    uart->regs->ier = 0;
    disable_intr_source(uart->irqno);
}

long ns8250_intr_read(struct io * io, void * buf, long bufsz) {
    struct ns8250_intr_device * const uart =
        (void*)io - offsetof(struct ns8250_intr_device, io);
    int sie;

    trace("%s(%ld)", __func__, bufsz);

    assert (io != NULL);

    // If we missed any data, return -EIO (I/O error) for all reads

    if (uart->rxoe != 0)
        return -EIO;

    if (bufsz == 0)
        return 0;
    
    assert (buf != 0);

    // Sleep until receive buffer is not empty, then copy as much data as we can
    // from ring buffer into /buf/. We can return without filling the buffer.
    // Note that we disable interrupts _before_ doing the copy. This is safe to
    // do (from a program correctness point of view) in our specific case
    // because our ring buffer does not require mutual exclusion between
    // rbuf_getc() and rbuf_putc().

    sie = disable_interrupts();

    while (rbuf_empty(&uart->rxbuf))
        condition_wait(&uart->rxbnotempty);

    restore_interrupts(sie);

    uart->regs->ier |= IER_DRIE; // enable receive interrupts

    return rbuf_getb(&uart->rxbuf, buf, bufsz);
}

long ns8250_intr_write(struct io * io, const void * buf, long buflen) {
    struct ns8250_intr_device * const uart =
        (void*)io - offsetof(struct ns8250_intr_device, io);
    long n = 0; // number of bytes written
    int sie;
   
    trace("%s(%ld)", __func__, buflen);

    assert (io != NULL);

    if (buflen == 0)
        return 0;

    assert (buf != NULL);

    // Acquire txlock to ensure that writes are not interleaved

    rwlock_acquire(&uart->txlock, /* exclusive */ 1);

    // Sleep until transmit ring buffer is not full, then copy bytes from _buf_
    // to ring buffer. Unlike the read case, we write all bytes before
    // returning.

    while (n < buflen) {
        sie = disable_interrupts();

        while (rbuf_full(&uart->txbuf))
            condition_wait(&uart->txbnotfull);

        restore_interrupts(sie);

        n += rbuf_putb(&uart->txbuf, buf+n, buflen-n);
        
        uart->regs->ier |= IER_THREIE;
    }

    rwlock_release(&uart->txlock);

    return n;
}

int ns8250_intr_ioctl(struct io * io, int op, void * arg) {
    struct ns8250_intr_device * const uart =
        (void*)io - offsetof(struct ns8250_intr_device, io);
    int sie;
    
    switch (op) {
    case IOC_RESET:
        sie = disable_interrupts();
        rbuf_reset(&uart->rxbuf);
        rbuf_reset(&uart->txbuf);
        uart->regs->rbr;
        uart->rxoe = 0;
        restore_interrupts(sie);
        return 0;
    default:
        return -ENOTSUP;
    }
}

int ns8250_poll_open(struct io ** ioptr, void * aux) {
    struct ns8250_poll_device * const uart = aux;

    trace("%s()", __func__);

    uart->regs->rbr; // flush RBR

    *ioptr = ioaddref(&uart->io);
    return 0;
}

long ns8250_poll_read(struct io * io, void * buf, long bufsz) {
    struct ns8250_poll_device * const uart =
        (void*)io - offsetof(struct ns8250_poll_device, io);
    unsigned char * const cbuf = buf;
    long n = 0;

    trace("%s(%ld)", __func__, bufsz);

    assert (io != NULL);

    if (bufsz == 0)
        return 0;
    
    assert (buf != 0);

    while ((uart->regs->lsr & LSR_DR) == 0)
        continue;
    
    while (n < bufsz && (uart->regs->lsr & LSR_DR) != 0)
        cbuf[n++] = uart->regs->rbr;
    
    return n;
}

long ns8250_poll_write(struct io * io, const void * buf, long buflen) {
    struct ns8250_poll_device * const uart =
        (void*)io - offsetof(struct ns8250_poll_device, io);
    unsigned char * const cbuf = buf;
    long n = 0; // number of bytes written
   
    trace("%s(%ld)", __func__, buflen);

    assert (io != NULL);

    if (buflen == 0)
        return 0;

    assert (buf != NULL);

    while (n < buflen) {
        while ((uart->regs->lsr & LSR_THRE) == 0)
            continue;
        
        uart->regs->thr = cbuf[n++];
    }

    return n;
}

void ns8250_isr(int srcno, void * aux) {
    struct ns8250_intr_device * const uart = aux;
    unsigned int const line_status = uart->regs->lsr;

    if (line_status & LSR_OE)
        uart->rxoe = 1;
    
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

void ns8250_hwinit(volatile struct ns8250_regs * regs) {
    // Initialize hardware device

    regs->ier = 0;
    regs->lcr = LCR_DLAB;
    // fence o,o ?
    regs->dll = 0x01;
    regs->dlm = 0x00;
    // fence o,o ?
    regs->lcr = 0; // DLAB=0
}