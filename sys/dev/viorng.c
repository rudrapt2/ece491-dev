// viorng.c
// VirtIO rng device
// Copyright (c) 2024-2026 University of Illinois

#include "virtio.h"
#include "intr.h"
#include "heap.h"
#include "error.h"
#include "string.h"
#include "thread.h"
#include "device.h"
#include "board-conf.h"
#include "intr.h"
#include "misc.h"
#include "ioimpl.h"

// INTERNAL CONSTANT DEFINITIONS
//

#ifdef STUDENT

#else
    #ifndef VIORNG_BUFSZ
    #define VIORNG_BUFSZ 256
    #endif
#endif

#ifndef VIORNG_NAME
#define VIORNG_NAME "viorng"
#endif

#ifndef VIORNG_INTR_PRIO
#define VIORNG_INTR_PRIO 1
#endif

// INTERNAL TYPE DEFINITIONS
//

struct viorng_device {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    volatile struct virtio_mmio_regs * regs;
    int irqno;

    struct io io;

    struct {
        uint16_t last_used_idx;

        union {
            struct virtq_avail avail;
            char _avail_fill[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_fill[VIRTQ_USED_SIZE(1)];
        };

        // The first descriptor is a regular descriptor and is the one used in
        // the avail and used rings.

        struct virtq_desc desc[1];
    } vq;

    struct condition bufupd; // buffer has been updated

    // bufcnt is the number of bytes left in buffer. The usable bytes are
    // between buf+0 and buf+bufcnt. (We read from the end of the buffer.)

    unsigned int bufcnt;
    char buf[VIORNG_BUFSZ];
#endif
};

// INTERNAL FUNCTION DECLARATIONS
//

static int viorng_open(struct io ** ioptr, void * aux);

static void viorng_reclaim(struct io * io);

static long viorng_read(struct io * io, void * buf, long bufsz);

static void viorng_isr(int irqno, void * aux);

// INTERNAL GLOBAL VARIABLES
//

static const struct iointf viorng_intf = {
    .implname = "viorng",
    .reclaim = &viorng_reclaim,
    .read = &viorng_read
};

// EXPORTED FUNCTION DEFINITIONS
//

// The vioblk_attach function is declared and called from virtio_attach() in
// virtio.c when a VirtIO RNG device is found.

void viorng_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    static unsigned short instcnt = 0; // number of viorng devices
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct viorng_device * vrng;
    int result;
    
    assert (regs->device_id == VIRTIO_ID_RNG);

    // Signal device that we found a driver

    regs->status |= VIRTIO_STAT_DRIVER;
    // fence o,io
    __sync_synchronize();

    virtio_featset_init(needed_features);
    virtio_featset_init(wanted_features);
    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        debug("%p: virtio feature negotiation failed\n", regs);
        return;
    }

#ifdef STUDENT
    // YOUR CODE HERE
#else

    // Allocate and initialize device struct

    vrng = kmalloc(sizeof(struct viorng_device));
    memset(vrng, 0, sizeof(struct viorng_device) - VIORNG_BUFSZ);

    vrng->regs = regs;
    vrng->irqno = irqno;

    condition_init(&vrng->bufupd, "viorng.bufupd");

#endif

    regs->status |= VIRTIO_STAT_FEATURES_OK;
    assert(regs->status & VIRTIO_STAT_FEATURES_OK);
    
#ifdef STUDENT
    // YOUR CODE HERE
#else
    // Initialize fields of descriptor that will not change.

    vrng->vq.avail.ring[0] = 0;
    vrng->vq.desc[0].flags = VIRTQ_DESC_F_WRITE;
    vrng->vq.desc[0].addr = (uintptr_t)vrng->buf;
    vrng->vq.desc[0].len = VIORNG_BUFSZ;
    vrng->vq.desc[0].next = -1;

    virtio_attach_virtq(regs, 0, 1,
        (uintptr_t)&vrng->vq.desc,
        (uintptr_t)&vrng->vq.used,
        (uintptr_t)&vrng->vq.avail);

#endif
    
    regs->status |= VIRTIO_STAT_DRIVER_OK; //set the driver to OK
    // fence o,oi
    __sync_synchronize();

#ifdef STUDENT
    // YOUR CODE HERE
#else
    register_device(VIORNG_NAME, instcnt++, &viorng_open, vrng);
    ioinit(&vrng->io, &viorng_intf, 1, 0);
#endif
}

int viorng_open(struct io ** ioptr, void * aux) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct viorng_device * vrng = aux;

    vrng->regs->status |= VIRTIO_STAT_ACKNOWLEDGE;

    virtio_enable_virtq(vrng->regs, 0);
    enable_intr_source(vrng->irqno, VIORNG_INTR_PRIO, &viorng_isr, vrng);

    *ioptr = ioaddref(&vrng->io);

    return 0;
#endif
}

void viorng_reclaim(struct io * io) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct viorng_device * const vrng = 
        (void*)io - offsetof(struct viorng_device, io);
    
    virtio_reset_virtq(vrng->regs, 0);
    disable_intr_source(vrng->irqno);
#endif
}

long viorng_read(struct io * io, void * buf, long bufsz) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct viorng_device * const vrng = 
        (void*)io - offsetof(struct viorng_device, io);
    long rcnt;
    int pie;

    if (bufsz == 0)
        return 0;
    
    // If buffer is empty, refill it

    while (vrng->bufcnt == 0) {
        vrng->vq.avail.idx += 1;
        virtio_notify_avail(vrng->regs, 0);

        pie = disable_interrupts();
        while (vrng->bufcnt == 0)
            condition_wait(&vrng->bufupd);
        restore_interrupts(pie);
    }

    rcnt = (vrng->bufcnt < bufsz) ? vrng->bufcnt : bufsz;
    memcpy(buf, vrng->buf + vrng->bufcnt - rcnt, rcnt);
    vrng->bufcnt -= rcnt;
    return rcnt;
#endif
}

void viorng_isr(int irqno, void * aux) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct viorng_device * vrng = aux;
    uint32_t intr_status;

    trace("%s(irqno=%d)", __func__, irqno);

    intr_status = vrng->regs->interrupt_status;
    vrng->regs->interrupt_ack = intr_status;

    __sync_synchronize(); // fence o,r

    if ((intr_status & 1) == 0)
        return;
    
    if (vrng->vq.last_used_idx != vrng->vq.used.idx) {
        vrng->bufcnt = VIORNG_BUFSZ;
        vrng->vq.last_used_idx += 1;
    }

    condition_broadcast(&vrng->bufupd);
#endif
}