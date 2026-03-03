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
#include "memory.h"
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
    struct condition rbupd; // ring buffer updated
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
    .implname = LOOP_DEVNAME,
    .read = &loop_read,
    .write = &loop_write,
    .reclaim = &loop_reclaim,
    .ioctl = &loop_ioctl
};


// EXPORTED FUNCTION DEFINITIONS
// 

void attach_loop(void) {
    register_device(LOOP_DEVNAME, -1, &loop_open, NULL);
}

int loop_open(struct io ** ioptr, void * aux) {
    struct loop_device * loop;

    trace("%s()", __func__);

    loop = kcalloc(1, sizeof(*loop));

    condition_init(&loop->rbupd, "loop.rbup");
    rwlock_init(&loop->txlock, "loop.txlock");
    rbuf_init(&loop->rbuf, alloc_phys_page(), PAGE_SIZE);
    *ioptr = ioinit(&loop->io, &loop_intf, 1, 1);
    return 0;
}

void loop_reclaim(struct io * io) {
    struct loop_device * const loop = (struct loop_device*)io;
    
    trace("%s()", __func__);

    free_phys_page(rbuf_bufmem(&loop->rbuf));
    kfree(loop);
}

long loop_read(struct io * io, void * buf, long bufsz) {
    struct loop_device * const loop = (struct loop_device*)io;
    long n;

    trace("%s(%ld)", __func__, bufsz);

    assert (io != NULL); 
    assert (buf != NULL);

    if (bufsz == 0)
        return 0;

    while (rbuf_empty(&loop->rbuf))
        condition_wait(&loop->rbupd);
    
    n = rbuf_getb(&loop->rbuf, buf, bufsz);
    condition_broadcast(&loop->rbupd);
    return n;
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
            condition_wait(&loop->rbupd);
        
        debug("Calling rbuf_putb(buf+%ld,%ld-%ld)", n, buflen, n);
        n += rbuf_putb(&loop->rbuf, buf+n, buflen-n);
        condition_broadcast(&loop->rbupd);
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