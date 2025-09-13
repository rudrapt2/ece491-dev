// viogpu.c - VirtIO GPU
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef VIOGPU_TRACE
#define TRACE
#endif

#ifdef VIOGPU_DEBUG
#define DEBUG
#endif

#include "virtio.h"
#include "intr.h"
#include "assert.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "thread.h"
#include "error.h"
#include "string.h"
#include "assert.h"
#include "ioimpl.h"
#include "io.h"
#include "conf.h"

#include "dev/fbuf.h"

#include <limits.h>

// COMPILE-TIME PARAMETERS
//

#ifndef VIOGPU_NAME
#define VIOGPU_NAME "viogpu"
#endif

#ifndef VIOGPU_INTR_PRIO
#define VIOGPU_INTR_PRIO 1
#endif

#ifndef VIOGPU_WIDTH
#define VIOGPU_WIDTH 640
#endif

#ifndef VIOGPU_HEIGHT
#define VIOGPU_HEIGHT 480
#endif

// INTERNAL CONSTANT DEFINITIONS
//

#define VIOGPU_RESID 1 // main resource id

#define VIOGPU_FBUFSZ ROUND_UP(4UL * VIOGPU_WIDTH * VIOGPU_HEIGHT, PAGE_SIZE)

// VirtIO GPU device feature bits (number, *not* mask)

#define VIRTIO_GPU_F_VIRGL          0
#define VIRTIO_GPU_F_EDID           1
#define VIRTIO_GPU_F_RESOURCE_UUID  2
#define VIRTIO_GPU_F_RESOURCE_BLOB  3
#define VIRTIO_GPU_F_CONTEXT_INIT   4

#define VIRTIO_GPU_CONTROLQ         0

// INTERNAL TYPE DEFINITIONS
//

enum virtio_gpu_ctrl_type { 
    /* 2d commands */ 
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100, 
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D, 
    VIRTIO_GPU_CMD_RESOURCE_UNREF, 
    VIRTIO_GPU_CMD_SET_SCANOUT, 
    VIRTIO_GPU_CMD_RESOURCE_FLUSH, 
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D, 
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING, 
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING, 
    VIRTIO_GPU_CMD_GET_CAPSET_INFO, 
    VIRTIO_GPU_CMD_GET_CAPSET, 
    VIRTIO_GPU_CMD_GET_EDID, 
    VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID, 
    VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB, 
    VIRTIO_GPU_CMD_SET_SCANOUT_BLOB, 

    /* 3d commands */ 
    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200, 
    VIRTIO_GPU_CMD_CTX_DESTROY, 
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE, 
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE, 
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D, 
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D, 
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D, 
    VIRTIO_GPU_CMD_SUBMIT_3D, 
    VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB, 
    VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB, 

    /* cursor commands */ 
    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300, 
    VIRTIO_GPU_CMD_MOVE_CURSOR, 

    /* success responses */ 
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100, 
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO, 
    VIRTIO_GPU_RESP_OK_CAPSET_INFO, 
    VIRTIO_GPU_RESP_OK_CAPSET, 
    VIRTIO_GPU_RESP_OK_EDID, 
    VIRTIO_GPU_RESP_OK_RESOURCE_UUID, 
    VIRTIO_GPU_RESP_OK_MAP_INFO, 

    /* error responses */ 
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200, 
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY, 
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID, 
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID, 
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID, 
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER, 
}; 

#define VIRTIO_GPU_FLAG_FENCE           (1 << 0) 
#define VIRTIO_GPU_FLAG_INFO_RING_IDX   (1 << 1) 

struct virtio_gpu_ctrl_hdr { 
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t ring_idx;
    uint8_t padding[3];
};

// Display info response

#define VIRTIO_GPU_MAX_SCANOUTS 16 
 
struct virtio_gpu_rect { 
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
}; 
 
struct virtio_gpu_resp_display_info { 
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_display_one { 
        struct virtio_gpu_rect r; 
        uint32_t enabled; 
        uint32_t flags; 
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS]; 
};

// Resource commands and responses

enum virtio_gpu_formats { 
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  = 1, 
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM  = 2, 
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM  = 3, 
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM  = 4, 

    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM  = 67, 
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM  = 68, 

    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM  = 121, 
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM  = 134, 
}; 

struct virtio_gpu_resource_create_2d { 
    struct virtio_gpu_ctrl_hdr hdr; 
    uint32_t resource_id; 
    uint32_t format; 
    uint32_t width; 
    uint32_t height; 
};

struct virtio_gpu_resource_attach_backing { 
    struct virtio_gpu_ctrl_hdr hdr; 
    uint32_t resource_id; 
    uint32_t nr_entries; 
}; 

struct virtio_gpu_mem_entry { 
    uint64_t addr; 
    uint32_t length; 
    uint32_t padding; 
};

struct virtio_gpu_set_scanout { 
    struct virtio_gpu_ctrl_hdr hdr; 
    struct virtio_gpu_rect r; 
    uint32_t scanout_id; 
    uint32_t resource_id; 
};

struct virtio_gpu_transfer_to_host_2d { 
    struct virtio_gpu_ctrl_hdr hdr; 
    struct virtio_gpu_rect r; 
    uint64_t offset; 
    uint32_t resource_id; 
    uint32_t padding; 
};

struct virtio_gpu_resource_flush { 
    struct virtio_gpu_ctrl_hdr hdr; 
    struct virtio_gpu_rect r; 
    uint32_t resource_id; 
    uint32_t padding; 
};

// VIOGPU structure
//

// 
//  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18
//  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//  | . | . | U | U | U | U | A | A | A | A | A | A | A | A | A | . | . | . |
//  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//          ^               ^               ^                   ^
//    last_used_idx      used.idx        avail.idx        avail_end_idx
//

struct viogpu_virtq {
    struct condition updated; // signalled when queue updated
    struct lock lock; // exclusive queue access

    union {
        struct virtq_avail avail;
        char _avail_filler[VIRTQ_AVAIL_SIZE(2)];
    };

    union {
        volatile struct virtq_used used;
        char _used_filler[VIRTQ_USED_SIZE(2)];
    };

    struct virtq_desc desc[2];
};

// Main device structure

struct viogpu_device {
    volatile struct virtio_mmio_regs * regs;
    struct io io;

    int irqno;
    int instno;
    void * pfbuf;
    struct viogpu_virtq vq;
};

static int viogpu_open(struct io ** ioptr, void * aux);

static void viogpu_close(struct io * io);
static int viogpu_cntl(struct io * io, int cmd, void * arg);
static long viogpu_write(struct io * io, const void * buf, long len);

static void viogpu_isr(int srcno, void * aux);

int viogpu_resource_create_2d(struct viogpu_device * vgpu);
int viogpu_resource_attach_backing (
    struct viogpu_device * vgpu, void * pbuf, unsigned long bufsz);
int viogpu_set_scanout(struct viogpu_device * vgpu);
int viogpu_transfer_to_host_2d(struct viogpu_device * vgpu);
int viogpu_resource_flush(struct viogpu_device * vgpu);

static int viogpu_issue_command (
    struct viogpu_device * vgpu,
    const struct virtio_gpu_ctrl_hdr * req,
    unsigned long reqlen);

// EXPORTED FUNCTION DEFINITIONS
//

// Attaches a VirtIO GPU device. Declared and called directly from virtio.c.

void viogpu_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct viogpu_device * vgpu;
    int result;
    // int i;

    static const struct iointf viogpu_intf = {
		.close = viogpu_close,
        .cntl = viogpu_cntl,
        .write = viogpu_write
	};

	trace("%s(regs=%p,irqno=%d)", __func__, regs, irqno);

    assert (regs->device_id == VIRTIO_ID_GPU);

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

    vgpu = kcalloc(1, sizeof(struct viogpu_device));
    vgpu->regs = regs;

    // fill io struct with ref count of zero
    ioinit0(&vgpu->io, &viogpu_intf);

    // vbd->instno filled later
    vgpu->irqno = irqno;

    lock_init(&vgpu->vq.lock);
    condition_init(&vgpu->vq.updated, "viogpu.vq.updated");

    // Descriptor 0 is always command
    vgpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    vgpu->vq.desc[0].next = 1;
    vgpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    vgpu->vq.desc[1].next = -1;
    
    // Attach queues

    virtio_attach_virtq (
        regs, VIRTIO_GPU_CONTROLQ, 2,
        (uintptr_t)vgpu->vq.desc,
        (uintptr_t)&vgpu->vq.used,
        (uintptr_t)&vgpu->vq.avail);
    
    vgpu->instno = register_device(VIOGPU_NAME, viogpu_open, vgpu);

    // Signal initialization complete

    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    __sync_synchronize(); // fence o,oi
}

int viogpu_open(struct io ** ioptr, void * aux) {
    struct viogpu_device * const vgpu = aux;
    // int result;

    vgpu->pfbuf = alloc_phys_pages(VIOGPU_FBUFSZ / PAGE_SIZE);

    virtio_enable_virtq(vgpu->regs, VIRTIO_GPU_CONTROLQ);
    enable_intr_source(vgpu->irqno, VIOGPU_INTR_PRIO, viogpu_isr, vgpu);

    viogpu_resource_create_2d(vgpu);
    viogpu_resource_attach_backing(vgpu, vgpu->pfbuf, VIOGPU_FBUFSZ);
    viogpu_set_scanout(vgpu);

    *ioptr = ioaddref(&vgpu->io);

    return 0;
}

long viogpu_write(struct io * io, const void * buf, long len) {
    struct viogpu_device * const vgpu =
        (void*)io - offsetof(struct viogpu_device, io);
    viogpu_transfer_to_host_2d(vgpu);
    viogpu_resource_flush(vgpu);
    return 0;
}

void viogpu_close(struct io * io) {
    struct viogpu_device * const vgpu =
        (void*)io - offsetof(struct viogpu_device, io);

    assert(io != NULL);
    assert (iorefcnt(io) == 0);

    virtio_reset_virtq(vgpu->regs, VIRTIO_GPU_CONTROLQ);
    disable_intr_source(vgpu->irqno);

    free_phys_pages(vgpu->pfbuf, VIOGPU_FBUFSZ / PAGE_SIZE);
}

int viogpu_cntl(struct io * io, int cmd, void * arg) {
    struct viogpu_device * const vgpu =
        (void*)io - offsetof(struct viogpu_device, io);
    void ** ppptr = arg; // pointer to physical pointer
    void ** vpptr = arg; // pointer to virtual pointer
    struct fbuf_conf * fbc = arg;
    
    switch (cmd) {
    case IOCTL_GETFBUF:
        *ppptr = vgpu->pfbuf;
        return 0;
    case IOCTL_MAPFBUF:
        // TODO: Map frame buffer into user space
        return -ENOTSUP;
    case IOCTL_GETFBCONF:
        fbc->width = VIOGPU_WIDTH;
        fbc->height = VIOGPU_HEIGHT;
        fbc->pxtype = FBUF_B8G8R8X8;
        return 0;
    default:
        return -ENOTSUP;
    }
}

void viogpu_isr(int srcno, void * aux) {
    struct viogpu_device * vgpu = (struct viogpu_device *)aux;
    uint32_t intr_status;

    trace("%s(srcno=%d)", __func__, srcno);

    intr_status = vgpu->regs->interrupt_status;
    vgpu->regs->interrupt_ack = intr_status;

    __sync_synchronize(); // fence o,r

    if ((intr_status & 1) == 0)
        return;
    
    condition_broadcast(&vgpu->vq.updated);
}

int viogpu_resource_create_2d(struct viogpu_device * vgpu) {
    struct virtio_gpu_resource_create_2d req;
    
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    req.resource_id = VIOGPU_RESID;
    req.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    req.width = VIOGPU_WIDTH;
    req.height = VIOGPU_HEIGHT;

    return viogpu_issue_command(vgpu, &req.hdr, sizeof(req));
}

int viogpu_resource_attach_backing (
    struct viogpu_device * vgpu, void * pbuf, unsigned long bufsz)
{
    struct {
        struct virtio_gpu_resource_attach_backing attach;
        struct virtio_gpu_mem_entry entry;
    } req;

    memset(&req, 0, sizeof(req));
    req.attach.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    req.attach.resource_id = 1;
    req.attach.nr_entries = 1;
    req.entry.addr = (uintptr_t)pbuf;
    req.entry.length = bufsz;

    return viogpu_issue_command(vgpu, &req.attach.hdr, sizeof(req));
}

int viogpu_set_scanout(struct viogpu_device * vgpu) {
    struct virtio_gpu_set_scanout req;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    req.r.width = VIOGPU_WIDTH;
    req.r.height = VIOGPU_HEIGHT;
    req.resource_id = VIOGPU_RESID;

    return viogpu_issue_command(vgpu, &req.hdr, sizeof(req));
}

int viogpu_transfer_to_host_2d(struct viogpu_device * vgpu) {
    struct virtio_gpu_transfer_to_host_2d req;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    req.r.width = VIOGPU_WIDTH;
    req.r.height = VIOGPU_HEIGHT;
    req.resource_id = VIOGPU_RESID;

    return viogpu_issue_command(vgpu, &req.hdr, sizeof(req));
}

int viogpu_resource_flush(struct viogpu_device * vgpu) {
    struct virtio_gpu_resource_flush req;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    req.r.width = VIOGPU_WIDTH;
    req.r.height = VIOGPU_HEIGHT;
    req.resource_id = VIOGPU_RESID;

    return viogpu_issue_command(vgpu, &req.hdr, sizeof(req));
}

int viogpu_issue_command (
    struct viogpu_device * vgpu,
    const struct virtio_gpu_ctrl_hdr * req,
    unsigned long reqlen)
{
    struct virtio_gpu_ctrl_hdr resp;
    unsigned int avail_idx;
    int pie;

    lock_acquire(&vgpu->vq.lock);

    vgpu->vq.desc[0].addr = (uintptr_t)req;
    vgpu->vq.desc[0].len = reqlen;
    vgpu->vq.desc[1].addr = (uintptr_t)&resp;
    vgpu->vq.desc[1].len = sizeof(resp);
    
    avail_idx = vgpu->vq.avail.idx;
    vgpu->vq.avail.idx = avail_idx + 1;

    virtio_notify_avail(vgpu->regs, VIRTIO_GPU_CONTROLQ);
    
    pie = disable_interrupts();
    while (vgpu->vq.avail.idx != vgpu->vq.used.idx)
        condition_wait(&vgpu->vq.updated);
    restore_interrupts(pie);

    assert (vgpu->vq.used.ring[avail_idx % 2].len == sizeof(resp));

    lock_release(&vgpu->vq.lock);

    switch (resp.type) {
    case VIRTIO_GPU_RESP_OK_NODATA:
        return 0;
    case VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY:
        return -ENOMEM;
    case VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID:
    case VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID:
    case VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID:
    case VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER:
        return -EINVAL;
    default:
        return -EIO;
    }
}

#if 0
int viogpu_get_display_info(struct viogpu_device * vgpu) {
    struct virtio_gpu_resp_display_info resp;
    struct virtio_gpu_ctrl_hdr req;
    unsigned int avail_idx;
    unsigned int used_idx;
    unsigned int didx;
    int i;

    avail_idx = wait_avail_idx(&vgpu->vq);
    didx = vgpu->vq.avail.ring[avail_idx % VIOGPU_VQLEN];
    vgpu->vq.desc[didx+0].addr = (uintptr_t)&req;
    vgpu->vq.desc[didx+0].len = sizeof(req);
    vgpu->vq.desc[didx+1].addr = (uintptr_t)&resp;
    vgpu->vq.desc[didx+1].len = sizeof(resp);

    memset(&req, 0, sizeof(req));
    req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    vgpu->vq.avail.ring[avail_idx % VIOGPU_VQLEN] = didx;
    vgpu->vq.avail.idx = avail_idx + 1;

    virtio_notify_avail(vgpu->regs, VIRTIO_GPU_CONTROLQ);

    debug("Enqueued VIRTIO_GPU_CMD_GET_DISPLAY_INFO command");
    
    do {
        used_idx = wait_used_idx(&vgpu->vq);
    } while (vgpu->vq.used.ring[used_idx % VIOGPU_VQLEN].id != didx);

    vgpu->vq.last_used_idx = used_idx + 1;

    assert (vgpu->vq.used.ring[used_idx].len == sizeof(resp));
    assert (resp.hdr.type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO);

    for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
        if (resp.pmodes[i].enabled) {
            debug("pmodes[%d].r.x = %lu", i, resp.pmodes[i].r.x);
            debug("pmodes[%d].r.y = %lu", i, resp.pmodes[i].r.y);
            debug("pmodes[%d].r.width = %lu", i, resp.pmodes[i].r.width);
            debug("pmodes[%d].r.height = %lu", i, resp.pmodes[i].r.height);
            debug("pmodes[%d].enabled = %lu", i, resp.pmodes[i].enabled);
            debug("pmodes[%d].flags = %lx", i, resp.pmodes[i].flags);
        }
    }

    return 0;
}

unsigned int wait_avail_idx(struct viogpu_virtq * vq) {
    int pie;
    
    // Fast path: there is an index available
    if (vq->avail.idx != vq->avail_end_idx)
        return vq->avail.idx;
    
    // Slow path: disable interrupts and wait

    pie = disable_interrupts();
    while (vq->avail.idx == vq->avail_end_idx)
        condition_wait(&vq->updated);
    restore_interrupts(pie);

    return vq->avail.idx;
}

unsigned int wait_used_idx(struct viogpu_virtq * vq) {
    int pie;
    
    // Fast path: there is an index available
    if (vq->used.idx != vq->last_used_idx)
        return vq->last_used_idx;
    
    // Slow path: disable interrupts and wait

    pie = disable_interrupts();
    while (vq->used.idx == vq->last_used_idx)
        condition_wait(&vq->updated);
    restore_interrupts(pie);

    return vq->last_used_idx;
}
#endif