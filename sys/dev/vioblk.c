// vioblk.c - VirtIO block device
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef VIOBLK_TRACE
#define TRACE
#endif

#ifdef VIOBLK_DEBUG
#define DEBUG
#endif

#include "virtio.h"
#include "intr.h"
#include "heap.h"
#include "device.h"
#include "thread.h"
#include "error.h"
#include "string.h"
#include "conf.h"
#include "misc.h"
#include "error.h"
#include "console.h"
#include "ioimpl.h"

#include <limits.h>

// COMPILE-TIME PARAMETERS
//

#ifndef VIOBLK_INTR_PRIO
#define VIOBLK_INTR_PRIO 1
#endif

#ifndef VIOBLK_NAME
#define VIOBLK_NAME "vioblk"
#endif

// INTERNAL CONSTANT DEFINITIONS
//

// VirtIO block device feature bits (number, *not* mask)

#define VIRTIO_BLK_F_SIZE_MAX       1
#define VIRTIO_BLK_F_SEG_MAX        2
#define VIRTIO_BLK_F_GEOMETRY       4
#define VIRTIO_BLK_F_RO             5
#define VIRTIO_BLK_F_BLK_SIZE       6
#define VIRTIO_BLK_F_FLUSH          9
#define VIRTIO_BLK_F_TOPOLOGY       10
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_MQ             12
#define VIRTIO_BLK_F_DISCARD        13
#define VIRTIO_BLK_F_WRITE_ZEROES   14

// INTERNAL TYPE DEFINITIONS
//

// All VirtIO block device requests consist of a request header, defined below,
// followed by data, followed by a status byte. The header is device-read-only,
// the data may be device-read-only or device-written (depending on request
// type), and the status byte is device-written.

struct vioblk_request_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

// Request type (for vioblk_request_header)

#define VIRTIO_BLK_T_IN             0
#define VIRTIO_BLK_T_OUT            1

// Status byte values

#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

/**
 * @brief VirtIO Block Device with virtqueues and condition variables
 */ 

struct vioblk_device {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    volatile struct virtio_mmio_regs * regs;
    int irqno;

    struct io io;

    unsigned long long bytecap;
    unsigned long long blkcnt;

    struct {
        struct condition used_updated;
        struct rwlock lock;

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        struct virtq_desc desc[4];
        struct vioblk_request_header req_header;
        uint8_t req_status;
    } vq;
#endif
};

// INTERNAL FUNCTION DECLARATIONS
//

static int vioblk_open(struct io ** ioptr, void * aux);

static void vioblk_reclaim(struct io * io);

static long vioblk_fetch (
    struct io * io, unsigned long long pos, void * buf, long len);

static long vioblk_store (
    struct io * io, unsigned long long pos, const void * buf, long len);

static int vioblk_ioctl(struct io * io, int op, void * arg);

static void vioblk_isr(int srcno, void * aux);
        
        
// INTERNAL GLOBAL VARIABLES
//

static const struct iointf vioblk_intf = {
    .implname = "vioblk",
    .reclaim = &vioblk_reclaim,
    .fetch = &vioblk_fetch,
    .store = &vioblk_store,
    .ioctl = &vioblk_ioctl
};

// EXPORTED FUNCTION DEFINITIONS
//

// The vioblk_attach function is declared and called from virtio_attach() in
// virtio.c when a VirtIO block device is found.

void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    static unsigned short instcnt = 0; // number of vioblk devices
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct vioblk_device * vb;
    unsigned int blksz;
    int result;
    
	trace("%s(regs=%p,irqno=%d)", __func__, regs, irqno);

    assert (regs->device_id == VIRTIO_ID_BLOCK);

    // Signal device that we found a driver

    regs->status |= VIRTIO_STAT_DRIVER;
    __sync_synchronize(); // fence o,io

    // Negotiate features. We need:
    //  - VIRTIO_F_RING_RESET and
    //  - VIRTIO_F_INDIRECT_DESC
    // We want:
    //  - VIRTIO_BLK_F_BLK_SIZE and
    //  - VIRTIO_BLK_F_TOPOLOGY.

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);
    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    // If the device provides a block size, use it. Otherwise, use 512.

    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    // blksz must be a power of two
    assert (((blksz - 1) & blksz) == 0);

#ifdef STUDENT
    // YOUR CODE HERE
#else

    // Allocate initialize device struct

    vb = kcalloc(1, sizeof(*vb));

    vb->regs = regs;
    vb->irqno = irqno;
    vb->blkcnt = vb->regs->config.blk.capacity;

    assert (vb->blkcnt < ULLONG_MAX / 512);
    vb->bytecap = vb->blkcnt * 512ULL;
    
    // Pre-initialize as much of the request virtq as possible. We use a very
    // simple scheme of one transaction at a time. A higher-performance scheme
    // may allow multiple concurrent requests.
    // Note: entire vb struct is zero-initialized, so we only need to set the
    // non-zero members of the virtq.

    // The first descriptor (index 0 in descriptor array) is the one used in the
    // avail and used rings. It is an indirect descriptor that points to three
    // chained descriptors, vb->vq.desc[1..3].

    vb->vq.desc[0].addr = (uintptr_t)(vb->vq.desc + 1);
    vb->vq.desc[0].len = 3 * sizeof(struct virtq_desc);
    vb->vq.desc[0].flags = VIRTQ_DESC_F_INDIRECT;
    vb->vq.desc[0].next = -1;

    // First descriptor in indirect descriptor chain points to request header.
    vb->vq.desc[1].addr = (uintptr_t)&vb->vq.req_header;
    vb->vq.desc[1].len = sizeof(struct vioblk_request_header);
    vb->vq.desc[1].flags = VIRTQ_DESC_F_NEXT;
    vb->vq.desc[1].next = 1; // relative chain

    // Second descriptor in indirect descriptor chain points to data.
    vb->vq.desc[2].flags = VIRTQ_DESC_F_NEXT;
    vb->vq.desc[2].next = 2;

    // Third descriptor in indirect descriptor chain points to request status.
    vb->vq.desc[3].addr = (uintptr_t)&vb->vq.req_status;
    vb->vq.desc[3].len = 1;
    vb->vq.desc[3].flags = VIRTQ_DESC_F_WRITE;
    vb->vq.desc[3].next = -1;

    condition_init(&vb->vq.used_updated, "vioblk.vq.used_updated");
    rwlock_init(&vb->vq.lock, "vioblk.vq.lock");

    // Attach queues

    virtio_attach_virtq (
        regs, 0, 1,
        (uintptr_t)&vb->vq.desc,
        (uintptr_t)&vb->vq.used,
        (uintptr_t)&vb->vq.avail);

    // Register device

    register_device(VIOBLK_NAME, instcnt++, &vioblk_open, vb);
    ioinit(&vb->io, &vioblk_intf, blksz, 0);
    
    // Signal initialization complete

    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    __sync_synchronize(); // fence o,oi
#endif
}

int vioblk_open(struct io ** ioptr, void * aux) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct vioblk_device * const vb = aux;

	trace("%s(%d,{regs=%p})", __func__, vb->irqno, vb->regs);
    
    vb->vq.avail.idx = 0;
    vb->vq.used.idx = 0;

    virtio_enable_virtq(vb->regs, 0);
    enable_intr_source(vb->irqno, VIOBLK_INTR_PRIO, vioblk_isr, vb);

    *ioptr = ioaddref(&vb->io);

    return 0;
#endif
}

void vioblk_reclaim(struct io * io) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct vioblk_device * const vb = 
        (void*)io - offsetof(struct vioblk_device, io);
    
	trace("%s()", __func__);

    virtio_reset_virtq(vb->regs, 0);
    disable_intr_source(vb->irqno);
#endif
}

long vioblk_fetch (
    struct io * io,
    unsigned long long bytepos,
    void * buf,
    long bytecnt)
{
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct vioblk_device * const vb = 
        (void*)io - offsetof(struct vioblk_device, io);
    unsigned long long blkpos;
    int pie;

    trace("%s(%lld,%ld)", __func__, bytepos, bytecnt);

    if (vb->bytecap < bytepos || vb->bytecap - bytepos < bytecnt)
        return -EINVAL;

    // Zero-length reads are allowed but do nothing

    if (bytecnt == 0)
        return 0;
    
    blkpos = bytepos / io->blksz;

    // Submit virtq request

    rwlock_acquire(&vb->vq.lock, /* excl*/ 1);

    vb->vq.req_header.sector = blkpos;
    vb->vq.req_header.type = VIRTIO_BLK_T_IN;
    vb->vq.desc[2].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    // Note: /buf/ must be a valid pma, so must not be in user space.
    vb->vq.desc[2].addr = (uintptr_t)buf;
    vb->vq.desc[2].len = bytecnt;

    __sync_synchronize(); // fence w,w

    vb->vq.avail.idx += 1;

    __sync_synchronize(); // fence w,w

    virtio_notify_avail(vb->regs, 0);

    // Wait for descriptor to return via used ring

    pie = disable_interrupts();
    while (vb->vq.used.idx != vb->vq.avail.idx)
        condition_wait(&vb->vq.used_updated);
    restore_interrupts(pie);

    rwlock_release(&vb->vq.lock);

    switch (vb->vq.req_status) {
    case VIRTIO_BLK_S_OK:
        return bytecnt;
    case VIRTIO_BLK_S_IOERR:
        return -EIO;
    case VIRTIO_BLK_S_UNSUPP:
        return -ENOTSUP;
    default:
        panic(NULL);
    }
#endif
}


long vioblk_store (
    struct io * io,
    unsigned long long bytepos,
    const void * buf,
    long bytecnt)
{
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct vioblk_device * const vb = 
        (void*)io - offsetof(struct vioblk_device, io);
    unsigned long long blkpos;
    int pie;

    trace("%s(%lld,%ld)", __func__, bytepos, bytecnt);

    if (vb->bytecap < bytepos || vb->bytecap - bytepos < bytecnt)
        return -EINVAL;

    // Zero-length reads are allowed but do nothing

    if (bytecnt == 0)
        return 0;
    
    blkpos = bytepos / io->blksz;

    // Submit virtq request

    rwlock_acquire(&vb->vq.lock, /* exclusive */ 1);

    vb->vq.req_header.sector = blkpos;
    vb->vq.req_header.type = VIRTIO_BLK_T_OUT;
    vb->vq.desc[2].flags = VIRTQ_DESC_F_NEXT;
    // Note: /buf/ must be a valid pma, so must not be in user space.
    vb->vq.desc[2].addr = (uintptr_t)buf;
    vb->vq.desc[2].len = bytecnt;
    __sync_synchronize(); // fence w,w
    vb->vq.avail.idx += 1;

    virtio_notify_avail(vb->regs, 0);

    // Wait for descriptor to return via used ring. A more efficient
    // implementation would not need to wait here. This would require some
    // additional bookkeeping to keep track of the fact that there is an
    // outstanding buffer. We also need a way to ensure that all data has been
    // written, so we would need to add a /flush/ function to /iointf/.

    pie = disable_interrupts();
    while (vb->vq.used.idx != vb->vq.avail.idx)
        condition_wait(&vb->vq.used_updated);
    restore_interrupts(pie);

    rwlock_release(&vb->vq.lock);

    switch (vb->vq.req_status) {
    case VIRTIO_BLK_S_OK:
        return bytecnt;
    case VIRTIO_BLK_S_IOERR:
        return -EIO;
    case VIRTIO_BLK_S_UNSUPP:
        return -ENOTSUP;
    default:
        panic(NULL);
    }
#endif
}

int vioblk_ioctl(struct io * io, int op, void * arg) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct vioblk_device * const vb = 
        (void*)io - offsetof(struct vioblk_device, io);
    
    trace("%s(op=%d)", __func__, op);
    
    switch (op) {
    case IOC_GETEND:
        *(unsigned long long*)arg = vb->bytecap;
        return 0;
    default:
        return -ENOTSUP;
    }
#endif
}

void vioblk_isr(int irqno, void * aux) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct vioblk_device * const vb = aux;
    uint32_t intr_status;

    intr_status = vb->regs->interrupt_status;
    vb->regs->interrupt_ack = intr_status;
    __sync_synchronize(); // fence o,r

    trace("%s(irqno=%d)", __func__, irqno);

    if (intr_status & 1)
        condition_broadcast(&vb->vq.used_updated);
#endif
}