// loop.c - Loopback serial device
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef LOOP_TRACE
#define TRACE
#endif

#ifdef LOOP_TRACE
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

// COMPILE-TIME CONSTANT DEFINITIONS
//

#ifndef LOOP_RBUFSZ
#define LOOP_RBUFSZ 64
#endif

#ifndef LOOP_DEVNAME
#define LOOP_DEVNAME "loop"
#endif


// INTERNAL TYPE DEFINITIONS
// 

struct loop_device {
    struct io io;
    struct condition rbup; // ring buffer updated
    struct rwlock txlock; // transmit exclusive access
    struct rbuf rbuf;
};


// INTERNAL FUNCTION DEFINITIONS
//

static int loop_open(struct io ** ioptr, void * aux);
static void loop_reclaim(struct io * io);
static long loop_read(struct io * io, void * buf, long bufsz);
static long loop_write(struct io * io, const void * buf, long len);
static int loop_ioctl(struct io * io, int op, void * arg);


// INTERNAL GLOBAL VARIABLES
//

static const struct iointf loop_intf = {
    .implname = "loop",
    .read = &loop_read,
    .write = &loop_write,
    .reclaim = &loop_reclaim,
    .ioctl = &loop_ioctl
};


// EXPORTED FUNCTION DEFINITIONS
// 

void attach_loop(void) {
    static unsigned short instcnt = 0; // device instance counter
    struct loop_device * loop;

    loop = kcalloc(1, sizeof(*loop));
    condition_init(&loop->rbup, "loop.rbup");
    rwlock_init(&loop->txlock, "loop.txlock");

    register_device(LOOP_DEVNAME, instcnt++, &loop_open, loop);
    ioinit(&loop->io, &loop_intf, 1, 0);
}

int loop_open(struct io ** ioptr, void * aux) {
    struct loop_device * const loop = aux;

    trace("%s()", __func__);

    if (iorefcnt(&loop->io) != 0)
        return -EBUSY;
    
    rbuf_reset(&loop->rbuf);

    *ioptr = ioaddref(&loop->io);
    return 0;
}

void loop_reclaim(struct io * io __attribute__ ((unused))) {
    trace("%s()", __func__);
}

long loop_read(struct io * io, void * buf, long bufsz) {
    struct loop_device * const loop = (struct loop_device*)io;

    trace("%s(%ld)", __func__, bufsz);

    assert (io != NULL); 
    assert (buf != NULL);

    if (bufsz == 0)
        return 0;

    while (rbuf_empty(&loop->rbuf))
        condition_wait(&loop->rbup);
    
    return rbuf_getb(&loop->rbuf, buf, bufsz);
}

long loop_write(struct io * io, const void * buf, long buflen) {
    struct loop_device * const loop = (struct loop_device*)io;
    long n = 0;
   
    trace("%s(%ld)", __func__, buflen);

    assert (io != NULL); 
    assert (buf != NULL);

    if (buflen == 0)
        return 0;

    rwlock_acquire(&loop->txlock, /* exclusive */ 1);

    do {
        while (rbuf_full(&loop->rbuf))
            condition_wait(&loop->rbup);
        
        n += rbuf_putb(&loop->rbuf, buf+n, buflen-n);
    } while (n < buflen);

    rwlock_release(&loop->txlock);

    return n;
}

int loop_ioctl(struct io * io, int op, void * arg) {
    struct loop_device * const loop = (struct loop_device*)io;
    
    switch (op) {
    case IOC_RESET:
        rbuf_reset(&loop->rbuf);
        return 0;
    default:
        return -ENOTSUP;
    }
}