// dw-uart.c - DesignWare APB UART
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
#include "console.h"
#include "device.h"
#include "thread.h"
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

#define OP_UART_USR         0x1F        // UART Status Register
#define OP_UART_DMASA       0xA8        // DMA Software Ack
#define OP_UART_USR_BUSY    (0 << 1)
#define OP_UART_USR_TFNF    (1 << 1)    // Transmit FIFO not full
#define OP_UART_USR_TFE     (1 << 2)    // Transmit FIFO empty
#define OP_UART_USR_RFNE    (1 << 3)    // Receive FIFO not empty
#define OP_UART_USR_RFF     (1 << 4)    // Receive FIFO full

#define UART0_MMIO_BASE 0x10000000UL  // PMA
#define UART1_MMIO_BASE 0x10010000UL  // PMA

struct uart_regs {
    union {
        uint32_t rbr; // DLAB=0 read
        uint32_t thr; // DLAB=0 write
        uint32_t dll; // DLAB=1
    };
    
    union {
        uint32_t ier; // DLAB=0
        uint32_t dlm; // DLAB=1
    };
    
    union {
        uint32_t iir; // read
        uint32_t fcr; // write
    };

    uint32_t lcr;
    uint32_t mcr;
    uint32_t lsr;
    uint32_t msr;
    uint32_t scr;

    uint32_t reserved[23];
    uint32_t usr;   // UART status register
};

#define LCR_DLAB (1 << 7)
#define LSR_OE (1 << 1)
#define LSR_DR (1 << 0)
#define LSR_THRE (1 << 5)
#define IER_DRIE (1 << 0)
#define IER_THREIE (1 << 1)
#define LCR_8N1 0x03            // 8 data bits, no parity, 1 stop bit
#define LSR_TEMT (1 << 6)       // Transmitter empty
#define IER_RLSIE (1 << 2)      // Receiver line status interrupt enable
#define FCR_ENABLE (1 << 0)     // FIFO enable
#define FCR_RCVR_RST (1 << 1)   // Receiver FIFO reset
#define FCR_XMIT_RST (1 << 2)   // Transmitter FIFO reset
#define FCR_TRIGGER_14 (3 << 6) // 14-byte trigger
#define IIR_NO_INT 0x01         // 0x01
#define IIR_BUSY 0x07           // Busy detect


// UART device structure

struct uart_device {
    volatile struct uart_regs * regs;
    int irqno;

    struct io io;

#ifdef STUDENT
    // YOUR CODE HERE
#else
    // struct condition rxbnotempty; // signalled when rxbuf becomes not empty
    // struct condition txbnotfull;  // signalled when txbuf becomes not full
    unsigned long rxovrcnt; // number of times OE was set on entry to ISR
#endif

    struct rbuf rxbuf;
    struct rbuf txbuf;
    unsigned int is_console;
    char rxbufmem[UART_RBUFSZ];
    char txbufmem[UART_RBUFSZ];
};

// INTERNAL FUNCTION DEFINITIONS
//

static int dwuart_open(struct io ** ioptr, void * aux);
static void dwuart_reclaim(struct io * io);
static long dwuart_read(struct io * io, void * buf, long bufsz);
static long dwuart_write(struct io * io, const void * buf, long len);
static int dwuart_ioctl(struct io * io, int op, void * arg);

static void dwuart_isr(int srcno, void * aux);

// INTERNAL GLOBAL VARIABLES
//

static const struct iointf uart_intf = {
    .implname = "dwuart",
    .reclaim = &dwuart_reclaim,
    .read = &dwuart_read,
    .write = &dwuart_write,
    .ioctl = &dwuart_ioctl
};

// EXPORTED FUNCTION DEFINITIONS
// 

void attach_uart(void * mmio_base, int irqno) {
    static unsigned short instcnt = 0; // number of UARTs
    struct uart_device * uart;

    uart = kcalloc(1, sizeof(*uart));

    uart->regs = mmio_base;
    uart->irqno = irqno;
    uart->is_console = (mmio_base == (void*)UART0_MMIO_BASE);

    // Initialize hardware device

    uart->regs->ier = 0;
    uart->regs->lcr = LCR_DLAB;
    // fence o,o ?
    uart->regs->dll = 13;
    uart->regs->dlm = 0x00;
    // fence o,o ?
    uart->regs->lcr = LCR_8N1; // DLAB=0
    if(!uart->is_console){
        uart->regs->fcr = FCR_ENABLE | FCR_RCVR_RST | FCR_XMIT_RST | FCR_TRIGGER_14;
    }

    register_device(UART_DEVNAME, instcnt++, &dwuart_open, uart);
    ioinit(&uart->io, &uart_intf, 1, 0);
}

int dwuart_open(struct io ** ioptr, void * aux) {
    struct uart_device * const uart = aux;

    trace("%s()", __func__);

    if (iorefcnt(&uart->io) != 0)
        return -EBUSY;
    
    // Reset receive and transmit buffers
    
    rbuf_init(&uart->rxbuf, uart->rxbufmem, sizeof(uart->rxbufmem));
    rbuf_init(&uart->txbuf, uart->txbufmem, sizeof(uart->txbufmem));

    // Read RBR to flush any stale data in hardware buffer
    uart->regs->iir;
    uart->regs->lsr;
    uart->regs->msr;
    uart->regs->rbr; // forces a read because uart->regs is volatile
    uart->regs->ier = 0;
    
    // Enable interrupts when data ready (DR) status asserted
    if(!uart->is_console){
        uart->regs->fcr = FCR_ENABLE | FCR_RCVR_RST | FCR_XMIT_RST | FCR_TRIGGER_14;
        uart->regs->ier = IER_DRIE;
        enable_intr_source(uart->irqno, UART_INTR_PRIO, dwuart_isr, uart);
    } else {
        uart->regs->fcr = FCR_ENABLE | FCR_RCVR_RST | FCR_XMIT_RST | FCR_TRIGGER_14;
        uart->regs->ier = 0;
    }

    *ioptr = ioaddref(&uart->io);
    return 0;
}

void dwuart_reclaim(struct io * io) {
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);

    trace("%s()", __func__);

    // Disable all interrupts from device

    uart->regs->ier = 0;
    if(!uart->is_console){
        disable_intr_source(uart->irqno);
    }
}

long dwuart_read(struct io * io, void * buf, long bufsz) {

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

    if(uart->is_console){
        unsigned char * buf_ptr = buf;
        for(long i = 0; i < bufsz; i++){
            while(!(uart->regs->lsr & LSR_DR)){
                continue;
            }
            buf_ptr[i] = uart->regs->rbr;
        }
        return bufsz;
    }
    // pie = disable_interrupts();

    while (rbuf_empty(&uart->rxbuf))
        // condition_wait(&uart->rxbnotempty);
        continue;

    // restore_interrupts(pie);

    while (n < bufsz && !rbuf_empty(&uart->rxbuf))
        ((char*)buf)[n++] = rbuf_getc(&uart->rxbuf);
    
    uart->regs->ier |= IER_DRIE; // enable receive interrupts
    
    return n;
}

long dwuart_write(struct io * io, const void * buf, long buflen) {
    struct uart_device * const uart =
        (void*)io - offsetof(struct uart_device, io);
    long n = 0; // number of bytes written so far
    int pie;
   
    trace("%s(%ld)", __func__, buflen);

    if (buflen == 0)
        return 0;

    assert (buf != NULL);

    // Sleep until transmit ring buffer is not full, then copy characters from
    // _buf_ to ring buffer. Unlike the read case, we write all characters
    // before returning.

    if(uart->is_console){
        unsigned char * buf_ptr = buf;
        for(long i = 0; i < buflen; i++){
            while(!(uart->regs->lsr & LSR_THRE)){
                continue;
            }
            uart->regs->thr = buf_ptr[i];
        }
        return buflen;
    }

    while (n < buflen) {
        // pie = disable_interrupts();

        while (rbuf_full(&uart->txbuf))
            // condition_wait(&uart->txbnotfull);
            continue;

        // restore_interrupts(pie);
        if ((uart->regs->lsr & LSR_THRE) && !rbuf_empty(&uart->txbuf)) {
            uart->regs->thr = rbuf_getc(&uart->txbuf) & 0xFF;
        }
        while (!rbuf_full(&uart->txbuf) && n < buflen)
            rbuf_putc(&uart->txbuf, ((const char*)buf)[n++]);
        
        uart->regs->ier |= IER_THREIE;
    }

    return n;
}

int dwuart_ioctl(struct io * io, int op, void * arg) {
    return -ENOTSUP;
}

void dwuart_isr(int srcno, void * aux) {
    struct uart_device * const uart = aux;
    const uint32_t line_status = uart->regs->lsr;
    uint32_t iir = uart->regs->iir & 0x0F;

    if(iir == IIR_BUSY){
        uart->regs->usr;        // Clear by reading USR register
        return;
    }

    if (line_status & LSR_OE)
        uart->rxovrcnt += 1;
    
    if (line_status & LSR_DR) {
        while(uart->regs->lsr & LSR_DR){
            if (!rbuf_full(&uart->rxbuf)) {
                rbuf_putc(&uart->rxbuf, uart->regs->rbr);
                // condition_broadcast(&uart->rxbnotempty);
            } else {
                uart->regs->ier &= ~IER_DRIE;
                break;
            }
        }
    }

    if (line_status & LSR_THRE) {
        if (!rbuf_empty(&uart->txbuf)) {
            while((uart->regs->lsr & LSR_THRE) && !rbuf_empty(&uart->txbuf)){
                uart->regs->thr = rbuf_getc(&uart->txbuf);
            }
            // condition_broadcast(&uart->txbnotfull);
        } else {
            uart->regs->ier &= ~IER_THREIE;
        }
    }
}