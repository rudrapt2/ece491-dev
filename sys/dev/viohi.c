// viohi.c - VirtIO human user interface input
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef VIOHI_TRACE
#define TRACE
#endif

#ifdef VIOHI_TRACE
#define DEBUG
#endif

#include "uio.h"
#include "intr.h"
#include "heap.h"
#include "conf.h"
#include "misc.h"
#include "viohi.h"
#include "error.h"
#include "virtio.h"
#include "device.h"
#include "thread.h"
#include "string.h"
#include "console.h"
#include "devimpl.h"

#include <limits.h>

// COMPILE-TIME PARAMETERS
//

#ifndef VIOHI_NAME
#define VIOHI_NAME "viohi"
#endif

#ifndef VIOHI_INTR_PRIO
#define VIOHI_INTR_PRIO 1
#endif

// INTERNAL CONSTANT DEFINITIONS
//

#define VIRTIO_INPUT_EVENTQ           0

#define VIOHI_QLEN 64

#define VIOHI_EVTSZ (sizeof(struct viohi_event))

// INTERNAL TYPE DEFINITIONS
//

struct virtio_input_event { 
    uint16_t type; 
    uint16_t code; 
    uint32_t value;
}; 

struct viohi_virtq {
    struct condition updated; // signalled when queue updated

    union {
        struct virtq_avail avail;
        char _avail_filler[VIRTQ_AVAIL_SIZE(VIOHI_QLEN)];
    };

    union {
        volatile struct virtq_used used;
        char _used_filler[VIRTQ_USED_SIZE(VIOHI_QLEN)];
    };

    struct virtq_desc desc[VIOHI_QLEN];
};

// Main device structure

struct viohi_device {
    struct serial base;
    volatile struct virtio_mmio_regs * regs;

    int irqno;
    int instno;
    struct viohi_virtq vq;
    struct virtio_input_event evts[VIOHI_QLEN];
};

static int viohi_open(struct serial * ser);
static void viohi_close(struct serial * ser);
static int viohi_read(struct serial * ser, void * buf, unsigned int buflen);

static void viohi_isr(int srcno, void * aux);

// INTERNAL GLOBAL VARIABLES
//

static const struct serial_intf viohi_intf = {
    .open = viohi_open,
    .close = viohi_close,
    .recv = viohi_read,
    .blksz = 1
};

// EXPORTED FUNCTION DEFINITIONS
//

// Attaches a VirtIO Input device. Declared and called directly from virtio.c.

void viohi_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct viohi_device * vhi;
    int result;
    int i;
    
	trace("%s(regs=%p,irqno=%d)", __func__, regs, irqno);

    assert (regs->device_id == VIRTIO_ID_INPUT);

    // Signal device that we found a driver

    regs->status |= VIRTIO_STAT_DRIVER;
    __sync_synchronize(); // fence o,io

    // Negotiate features. We need:
    //  - VIRTIO_F_RING_RESET

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_init(wanted_features);

    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    // Allocate initialize device struct

    vhi = kcalloc(1, sizeof(struct viohi_device));
    vhi->regs = regs;

    // fill io struct with ref count of zero
    serial_init(&vhi->base, &viohi_intf);

    // vbd->instno filled later
    vhi->irqno = irqno;

    condition_init(&vhi->vq.updated, "vioinp.vq.updated");

    for (i = 0; i < VIOHI_QLEN; i++) {
        vhi->vq.desc[i].flags = VIRTQ_DESC_F_WRITE;
        vhi->vq.desc[i].next = -1;
        vhi->vq.desc[i].addr = (uintptr_t)&vhi->evts[i];
        vhi->vq.desc[i].len = sizeof(vhi->evts[i]);
    }
    
    // Attach queues

    virtio_attach_virtq (
        regs, VIRTIO_INPUT_EVENTQ, VIOHI_QLEN,
        (uintptr_t)vhi->vq.desc,
        (uintptr_t)&vhi->vq.used,
        (uintptr_t)&vhi->vq.avail);
    
    vhi->instno = register_device(VIOHI_NAME, DEV_SERIAL, vhi);
    // Signal initialization complete

    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    __sync_synchronize(); // fence o,oi
    trace("%p: Virtio Input device initialized (instance %d)\n", regs, vhi->instno);
}

int viohi_open(struct serial * ser) {
    struct viohi_device * const vhi =
        (void*)ser - offsetof(struct viohi_device, base);

    int i;

    trace("%s()", __func__);

    // Load descriptors into avail ring
    for (i = 0; i < VIOHI_QLEN; i++)
        vhi->vq.avail.ring[i] = i;

    vhi->vq.avail.idx = VIOHI_QLEN;
    vhi->vq.used.idx = 0;

    virtio_enable_virtq(vhi->regs, VIRTIO_INPUT_EVENTQ);
    enable_intr_source(vhi->irqno, VIOHI_INTR_PRIO, viohi_isr, vhi);

    return 0;
}

void viohi_close(struct serial * ser) {
    struct viohi_device * const vhi =
        (void*)ser - offsetof(struct viohi_device, base);

    // assert (ser != NULL);
    // assert (iorefcnt(ser) == 0);

    virtio_reset_virtq(vhi->regs, VIRTIO_INPUT_EVENTQ);
    disable_intr_source(vhi->irqno);
}

int viohi_read(struct serial * ser, void * buf, unsigned int buflen) {
    struct viohi_device * const vhi =
        (void*)ser - offsetof(struct viohi_device, base);
    long cnt = 0;
    int pie;
    int k;

    trace("%s(buf=%p, bufsz=%ld)", __func__, buf, buflen);

    // assert (buf != NULL);
    assert (0 <= buflen);

    if (buflen == 0)
        return 0;
    
    if (buflen < VIOHI_EVTSZ)
        return -EINVAL;
    
    // Round down to multiple of VIOHI_EVTSZ
    buflen &= ~(VIOHI_EVTSZ - 1);

    k = vhi->vq.avail.idx - VIOHI_QLEN;

    debug("vhi->vq.avail.idx = %lu", (unsigned long)vhi->vq.avail.idx);
    debug("vhi->vq.used.idx = %lu", (unsigned long)vhi->vq.used.idx);

    if (vhi->vq.used.idx == k) {
        pie = disable_interrupts();
        while (vhi->vq.used.idx == k)
            condition_wait(&vhi->vq.updated);
        restore_interrupts(pie);
    }
    
    while (cnt < buflen && vhi->vq.used.idx != k) {
        memcpy(buf+cnt, &vhi->evts[k++ % VIOHI_QLEN], VIOHI_EVTSZ);
        cnt += VIOHI_EVTSZ;
    }

    vhi->vq.avail.idx = k + VIOHI_QLEN;
    virtio_notify_avail(vhi->regs, VIRTIO_INPUT_EVENTQ);

    return cnt;
}

void viohi_isr(int srcno, void * aux) {
    struct viohi_device * vhi = (struct viohi_device *)aux;
    uint32_t intr_status;

    trace("%s(srcno=%d)", __func__, srcno);

    intr_status = vhi->regs->interrupt_status;
    vhi->regs->interrupt_ack = intr_status;

    __sync_synchronize(); // fence o,r

    if ((intr_status & 1) == 0)
        return;
    
    condition_broadcast(&vhi->vq.updated);
}
