// uart.c -  NS8250-compatible serial port
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef UART_TRACE
#define TRACE
#endif

#ifdef UART_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "assert.h"
#include "uart.h"
#include "devimpl.h"
#include "intr.h"
#include "heap.h"
#include "thread.h"
#include "console.h"

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

struct uart_serial {
    struct serial base;
    volatile struct uart_regs * regs;
    int irqno;
    char opened;

    unsigned long rxovrcnt; ///< number of times OE was set
    
    struct condition rxbnotempty; ///< signalled when rxbuf becomes not empty
    struct condition txbnotfull;  ///< signalled when txbuf becomes not full

    struct ringbuf rxbuf;
    struct ringbuf txbuf;
};

// INTERNAL FUNCTION DEFINITIONS
//

static int uart_serial_open(struct serial * ser);
static void uart_serial_close(struct serial * ser);
static int uart_serial_recv(struct serial * ser, void * buf, unsigned int buflen);
static int uart_serial_send(struct serial * ser, const void * buf, unsigned int buflen);

static void uart_isr(int srcno, void * aux);

// Ring buffer (struct rbuf) functions

static void rbuf_init(struct ringbuf * rbuf);
static int rbuf_empty(const struct ringbuf * rbuf);
static int rbuf_full(const struct ringbuf * rbuf);
static void rbuf_putc(struct ringbuf * rbuf, char c);
static char rbuf_getc(struct ringbuf * rbuf);

// INTERNAL GLOBAL VARIABLES
//

static const struct serial_intf uart_serial_intf = {
    .blksz = 1,
    .open = &uart_serial_open,
    .close = &uart_serial_close,
    .recv = &uart_serial_recv,
    .send = &uart_serial_send
};

// EXPORTED FUNCTION DEFINITIONS
// 

/**
 * @brief This function attaches an NS8250/16550 compatible UART device. It
 * returns an instance of the serial device class.
 * @param mmio_base The address at which the device registers are mapped
 * @param irqno The interrupt source number of the device
 * @return a pointer to a struct serial corresponding to the device
 */

void attach_uart(void * mmio_base, int irqno) {
    struct uart_serial * uart;

    // UART0 is used for the console and should not be attached as a normal
    // device. It should already be initialized by console_init().

    if (mmio_base == (void*)UART0_MMIO_BASE)
        return;
    
    uart = kcalloc(1, sizeof(*uart));

    uart->regs = mmio_base;
    uart->irqno = irqno;
    uart->opened = 0;

    // Initialize condition variables. The ISR is registered when our interrupt
    // source is enabled in uart_serial_open().

    condition_init(&uart->rxbnotempty, "uart.rxnotempty");
    condition_init(&uart->txbnotfull, "uart.txnotfull");


    // Initialize hardware

    uart->regs->ier = 0;
    uart->regs->lcr = LCR_DLAB;
    // fence o,o ?
    uart->regs->dll = 0x01;
    uart->regs->dlm = 0x00;
    // fence o,o ?
    uart->regs->lcr = 0; // DLAB=0

    serial_init(&uart->base, &uart_serial_intf);
    register_device(UART_DEVNAME, DEV_SERIAL, uart);
}

int uart_serial_open(struct serial * ser) {
    struct uart_serial * const uart =
        (void*)ser - offsetof(struct uart_serial, base);

    trace("%s()", __func__);

    if (uart->opened)
        return -EBUSY;
    
    // Reset receive and transmit buffers
    
    rbuf_init(&uart->rxbuf);
    rbuf_init(&uart->txbuf);

    // Read receive buffer register to flush any stale data in hardware buffer

    uart->regs->rbr; // forces a read because uart->regs is volatile

    // Enable interrupts when data ready (DR) status asserted

    uart->regs->ier = IER_DRIE;

    enable_intr_source(uart->irqno, UART_INTR_PRIO, uart_isr, uart);

    uart->opened = 1;
    return 0;
}

void uart_serial_close(struct serial * ser) {
    struct uart_serial * const uart =
        (void*)ser - offsetof(struct uart_serial, base);

    trace("%s()", __func__);
    assert (uart->opened);

    // Disable all interrupts from device

    uart->regs->ier = 0;
    disable_intr_source(uart->irqno);

    uart->opened = 0;
}

/**
 * @brief Implements the recv() function of the serial device class.
 * @details This function is called via the function pointer in the
 * uart_serial_intf struct by serial_recv() to receive data to a buffer. The
 * bufsz parameter gives the buffer capacity, i.e., the maximum number of bytes
 * to receive. The function may return after receiving fewer than bufsz bytes,
 * but always receives at least one byte.
 * @param ser a pointer to a struct uart_serial
 * @param buf pointer to where the first received character will be written
 * @param bufsz size of the buffer (capacity)
 * @return the number of characters actually received, which may be fewer than
 * bufsz
 */

int uart_serial_recv(struct serial * ser, void * buf, unsigned int bufsz) {
    struct uart_serial* const uart = (struct uart_serial*)ser;
    long n = 0; // number of bytes copied from ring buffer
    int pie;

    trace("%s(bufsz=%ld)", __func__, bufsz);
    assert (uart->opened);
    
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


/**
 * @brief Implements the send() function of the serial device class.
 * @details This function is called via the function pointer in the
 * uart_serial_intf struct by serial_send() to send data from a buffer. The len
 * parameter gives the number of bytes to send. The function always sends all
 * len bytes before returning.
 * @param ser a pointer to a struct uart_serial
 * @param buf pointer to the first character to send
 * @param len number of bytes to send
 * @return the number of bytes actually send, always equal to len
 */

int uart_serial_send(struct serial * ser, const void * buf, unsigned int buflen) {
    struct uart_serial * const uart =
        (void*)ser - offsetof(struct uart_serial, base);
    long n = 0; // number of bytes written so far
    int pie;
   
    trace("%s(len=%ld)", __func__, len);
    assert (uart->opened);

    if (buflen == 0)
        return 0;

    assert (buf != NULL);

    // Sleep until transmit ring buffer is not full, then copy characters from
    // _buf_ to ring buffer. Unlike the read case, we write all characters
    // before returning.

    while (n < buflen) {
        pie = disable_interrupts();

        while (rbuf_full(&uart->txbuf))
            condition_wait(&uart->txbnotfull);

        restore_interrupts(pie);

        while (!rbuf_full(&uart->txbuf) && n < buflen)
            rbuf_putc(&uart->txbuf, ((const char*)buf)[n++]);
        
        uart->regs->ier |= IER_THREIE;
    }

    return n;
}

void uart_isr(int srcno, void * aux) {
    struct uart_serial * const uart = aux;
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

/**
 * @brief Initializes the ring buffer structure. The ring buffer is initially empty.
 * @param rbuf a pointer to a pre-allocated ringbuf structure
 */

void rbuf_init(struct ringbuf * rbuf) {
    rbuf->hpos = 0;
    rbuf->tpos = 0;
}


/**
 * @brief Tests if a ring buffer is empty
 * @param rbuf pointer to a ring buffer structure
 * @return 1 if the ring buffer is empty, and 0 otherwise
 */

int rbuf_empty(const struct ringbuf * rbuf) {
    return (rbuf->hpos == rbuf->tpos);
}


/**
 * @brief Tests if a ring buffer is full
 * @param rbuf pointer to a ring buffer structure
 * @return 1 if the ring buffer is full, and 0 otherwise
 */

int rbuf_full(const struct ringbuf * rbuf) {
    return (rbuf->tpos - rbuf->hpos == UART_RBUFSZ);
}

/**
 * @brief Adds a character to the tail of the ring buffer. The ring buffer
 * must not be full before calling this function.
 * @param rbuf pointer to a ring buffer structure
 * @param c character to insert into the ring buffer
 */

void rbuf_putc(struct ringbuf * rbuf, char c) {
    uint_fast16_t tpos;

    tpos = rbuf->tpos;
    rbuf->data[tpos % UART_RBUFSZ] = c;
    asm volatile ("" ::: "memory");
    rbuf->tpos = tpos + 1;
}

/**
 * @brief Removes a character from the head of the ring buffer. The ring buffer
 * must not be empty before calling this function.
 * @param rbuf pointer to a ring buffer structure
 * @return character removed from the head of the ring buffer
 */

char rbuf_getc(struct ringbuf * rbuf) {
    uint_fast16_t hpos;
    char c;

    hpos = rbuf->hpos;
    c = rbuf->data[hpos % UART_RBUFSZ];
    asm volatile ("" ::: "memory");
    rbuf->hpos = hpos + 1;
    return c;
}

// The functions below provide polled uart input and output for the console.

#define UART0 (*(volatile struct uart_regs*)UART0_MMIO_BASE)

void console_device_init(void) {
    UART0.ier = 0x00;

    // Configure UART0. We set the baud rate divisor to 1, the lowest value,
    // for the fastest baud rate. In a physical system, the actual baud rate
    // depends on the attached oscillator frequency. In a virtualized system,
    // it doesn't matter.
    
    UART0.lcr = LCR_DLAB;
    UART0.dll = 0x01;
    UART0.dlm = 0x00;

    // The com0_putc and com0_getc functions assume DLAB=0.

    UART0.lcr = 0;
}

void console_device_putc(char c) {
    // Spin until THR is empty
    while (!(UART0.lsr & LSR_THRE))
        continue;

    UART0.thr = c;
}

char console_device_getc(void) {
    // Spin until RBR contains a byte
    while (!(UART0.lsr & LSR_DR))
        continue;
    
    return UART0.rbr;
}