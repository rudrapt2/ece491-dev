/*! @file viorng.c 
    @brief VirtIO rng device
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#include "virtio.h"
#include "intr.h"
#include "heap.h"
#include "error.h"
#include "string.h"
#include "thread.h"
#include "devimpl.h"
#include "conf.h"
#include "intr.h"
#include "misc.h"

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

struct viorng_serial {
    struct serial base;
    volatile struct virtio_mmio_regs * regs;
    int irqno;
    char opened;

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
};

// INTERNAL FUNCTION DECLARATIONS
//

static int viorng_serial_open(struct serial * ser);

static void viorng_serial_close(struct serial * ser);
static int viorng_serial_recv(struct serial * ser, void * buf, unsigned int bufsz);

static void viorng_isr(int irqno, void * aux);

// INTERNAL GLOBAL VARIABLES
//

static const struct serial_intf viorng_serial_intf = {
    .blksz = 1,
    .open = &viorng_serial_open,
    .close = &viorng_serial_close,
    .recv = &viorng_serial_recv
};

// EXPORTED FUNCTION DEFINITIONS
//

// Attaches a VirtIO rng device. Declared and called directly from virtio.c.

void viorng_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct viorng_serial * vrng;
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

    // Allocate and initialize device struct

    vrng = kmalloc(sizeof(struct viorng_serial));
    memset(vrng, 0, sizeof(struct viorng_serial) - VIORNG_BUFSZ);

    vrng->regs = regs;
    vrng->irqno = irqno;

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

    serial_init(&vrng->base, &viorng_serial_intf);
    register_device(VIORNG_NAME, DEV_SERIAL, vrng);
}

int viorng_serial_open(struct serial * ser) {
    struct viorng_serial * vrng = (struct viorng_serial*)ser; 
    vrng->regs->status |= VIRTIO_STAT_ACKNOWLEDGE;

    if (vrng->opened)
		return -EBUSY;

    vrng->vq.avail.idx = 0;
    vrng->vq.used.idx = 0;

    virtio_enable_virtq(vrng->regs, 0);
    enable_intr_source(vrng->irqno, VIORNG_INTR_PRIO, viorng_isr, vrng);

    vrng->opened = 1;

    return 0;
}

void viorng_serial_close(struct serial * ser) {
    struct viorng_serial * vrng = (struct viorng_serial*)ser;

    assert (vrng->opened);

    virtio_reset_virtq(vrng->regs, 0);
    disable_intr_source(vrng->irqno);
    vrng->opened = 0;
}

int viorng_serial_recv(struct serial * ser, void * buf, unsigned int bufsz) {
    struct viorng_serial * vrng = (struct viorng_serial*)ser;
    long rcnt;
    int pie;

    assert (vrng->opened);

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
    struct viorng_serial * vrng = (struct viorng_serial *)aux;
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