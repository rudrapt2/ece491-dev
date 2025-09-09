// viorng.c - VirtIO rng device
// 
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "virtio.h"
#include "intr.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "error.h"
#include "string.h"
#include "thread.h"
#include "ioimpl.h"
#include "assert.h"
#include "conf.h"
#include "intr.h"
#include "console.h"

// INTERNAL CONSTANT DEFINITIONS
//

#ifndef VIORNG_BUFSZ
#define VIORNG_BUFSZ 256
#endif

#ifndef VIORNG_NAME
#define VIORNG_NAME "viorng"
#endif

#ifndef VIORNG_IRQ_PRIO
#define VIORNG_IRQ_PRIO 1
#endif

// INTERNAL TYPE DEFINITIONS
//

struct viorng_device {
    volatile struct virtio_mmio_regs * regs;
    int irqno;
    int instno;

    struct io io;

    struct {
        uint16_t last_used_idx;

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
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
};

// INTERNAL FUNCTION DECLARATIONS
//

static int viorng_open(struct io ** ioptr, void * aux);

static void viorng_close(struct io * io);
static long viorng_read(struct io * io, void * buf, long bufsz);

static void viorng_isr(int irqno, void * aux);

// EXPORTED FUNCTION DEFINITIONS
//

// Attaches a VirtIO rng device. Declared and called directly from virtio.c.

void viorng_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    static const struct iointf viorng_iointf = {
        .close = viorng_close,
        .read = viorng_read
    };

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
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    // Allocate and initialize device struct

    vrng = kmalloc(sizeof(struct viorng_device));
    memset(vrng, 0, sizeof(struct viorng_device) - VIORNG_BUFSZ);

    vrng->regs = regs;
    vrng->irqno = irqno;

    ioinit0(&vrng->io, &viorng_iointf);

    condition_init(&vrng->bufupd, "viorng.bufupd");

    regs->status |= VIRTIO_STAT_FEATURES_OK;
    assert(regs->status & VIRTIO_STAT_FEATURES_OK);
    
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
    
    regs->status |= VIRTIO_STAT_DRIVER_OK; //set the driver to OK
    // fence o,oi
    __sync_synchronize();

    vrng->instno = register_device(VIORNG_NAME, &viorng_open, vrng);
}

int viorng_open(struct io ** ioptr, void * aux) {
    struct viorng_device *vrng = (struct viorng_device*) aux; 
    vrng->regs->status |= VIRTIO_STAT_ACKNOWLEDGE;

    assert(ioptr != NULL);

    if (iorefcnt(&vrng->io) > 0)
		return -EBUSY;

    vrng->vq.avail.idx = 0;
    vrng->vq.used.idx = 0;

    virtio_enable_virtq(vrng->regs, 0);
    enable_intr_source(vrng->irqno, VIORNG_INTR_PRIO, viorng_isr, vrng);

    *ioptr = ioaddref(&vrng->io);
    return 0;
}

void viorng_close(struct io * io) {
    struct viorng_device *const vrng =
            (void *)io - offsetof(struct viorng_device, io);

    assert(io != NULL);
    assert (iorefcnt(io) == 0);

    virtio_reset_virtq(vrng->regs, 0);
    disable_intr_source(vrng->irqno);
}

long viorng_read(struct io * io, void * buf, long bufsz) {
    struct viorng_device * vrng =
        (void *)io - offsetof(struct viorng_device, io);
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
}

void viorng_isr(int irqno, void * aux) {
    struct viorng_device * vrng = (struct viorng_device *)aux;
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
}