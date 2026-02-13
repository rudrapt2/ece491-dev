// viogpu.c - VirtIO gpu device
//
// Copyright (c) 2025-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "io.h"
#include "ioimpl.h"
#include "intr.h"
#include "heap.h"
#include "conf.h"
#include "misc.h"
#include "error.h"
#include "device.h"
#include "thread.h"
#include "string.h"
#include "console.h"
#include "memory.h"
#include "virtio.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifndef VIOGPU_NAME
#define VIOGPU_NAME "viogpu"
#endif

#ifndef VIOGPU_IRQ_PRIO
#define VIOGPU_IRQ_PRIO 1
#endif

#ifndef VIOGPU_FBUF_WIDTH
#define VIOGPU_FBUF_WIDTH 640
#endif

#ifndef VIOGPU_FBUF_HEIGHT
#define VIOGPU_FBUF_HEIGHT 480
#endif

#define VIOGPU_VIRTQ_LEN 2
#define VIRTIO_GPU_FLAG_FENCE (1u << 0)

#ifndef VIRTIO_GPU_FBUF_VMA
#define VIRTIO_GPU_FBUF_VMA 0x90000000UL
#endif

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

struct video_mode {
    unsigned int width, height;
    unsigned int horiz_stride;
    unsigned int vert_stride;
    unsigned char bytes_per_pixel;
    unsigned char rshift, rdepth;
    unsigned char gshift, gdepth;
    unsigned char bshift, bdepth;
};

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t  ring_idx;
    uint8_t  padding[3];
};

struct virtio_gpu_rect {
    uint32_t x, y, width, height;
};

struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
};

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
};

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    struct virtio_gpu_mem_entry entries[1];
};

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
};

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_resource_detach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
};

struct virtio_gpu_resource_unref {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
};

struct viogpu_device {
    struct io base;
    volatile struct virtio_mmio_regs *regs;
    int irqno;
    int instno;

    struct {
        uint16_t last_used_idx;
        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(VIOGPU_VIRTQ_LEN)];
        };
        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(VIOGPU_VIRTQ_LEN)];
        };
        struct virtq_desc desc[VIOGPU_VIRTQ_LEN];
    } vq;

    struct {
        volatile struct virtio_gpu_resource_flush     flush;
        volatile struct virtio_gpu_transfer_to_host_2d transfer;
        volatile struct virtio_gpu_ctrl_hdr           response;
    } cmd;

    unsigned short color_space;        // virtio format enum value
    void     *fbuf_pma;                // physical backing
    uintptr_t fbuf_vma;                // mapped VA returned to callers
    uint64_t  fbuf_sz;
    struct condition GPUBusy;
    struct video_mode conf;            // width/height/bpp/stride
    uint32_t framebuffer_resource_id;  // stable ID (use instno or irqno)
};


static int  viogpu_open(struct io ** ioptr, void * aux);
static int viogpu_ioctl(struct io * io, int op, void * arg);
static long viogpu_write(struct io * io, const void * buf, long len);
static void viogpu_reclaim(struct io * io);
static void viogpu_isr(int irqno, void * aux);

static int viogpu_create_resource_2d (struct viogpu_device * viogpu);
static int viogpu_attach_backing     (struct viogpu_device * viogpu);
static int viogpu_set_scanout        (struct viogpu_device * viogpu, uint32_t resource_id);
static int viogpu_transfer_to_host   (struct viogpu_device * viogpu);
static int viogpu_cmd_flush          (struct viogpu_device * viogpu);
static int viogpu_detach_backing     (struct viogpu_device * viogpu);
static int viogpu_resource_unref     (struct viogpu_device * viogpu);
static int viogpu_map_buffer         (struct viogpu_device * viogpu);

void viogpu_attach(volatile struct virtio_mmio_regs *regs, int irqno) {
    static unsigned short instcnt = 0; // number of vioblk devices
    if (!regs) return;

    static const struct iointf viogpu_intf = {
        .implname = "viogpu",
        .reclaim = &viogpu_reclaim,
        .ioctl = &viogpu_ioctl,
        .ioctl_u = (int(*)(struct io*, int, uintptr_t))&viogpu_ioctl,
        .write = &viogpu_write,
    };

    struct viogpu_device * viogpu = kcalloc(1, sizeof(*viogpu));

    viogpu->regs  = regs;
    viogpu->irqno = irqno;

    viogpu->regs->status |= VIRTIO_STAT_DRIVER;
    __sync_synchronize();

    virtio_featset_t enabled, wanted, needed;
    virtio_featset_init(needed);
    virtio_featset_init(wanted);
    int result = virtio_negotiate_features(regs, enabled, wanted, needed);
    if (result) {
        kprintf("%p: virtio GPU feature negotiation failed\n", regs);
        return;
    }

    virtio_attach_virtq(viogpu->regs, 0, VIOGPU_VIRTQ_LEN,
                        (uint64_t)&viogpu->vq.desc,
                        (uint64_t)&viogpu->vq.used,
                        (uint64_t)&viogpu->vq.avail);

    viogpu->instno = instcnt++;
    register_device(VIOGPU_NAME, viogpu->instno, &viogpu_open, viogpu);
    ioinit(&viogpu->base, &viogpu_intf, 1, 0);

    viogpu->regs->status |= VIRTIO_STAT_DRIVER_OK;
    __sync_synchronize();

    // Queue & ISR plumbing
    viogpu->vq.avail.idx = 0;
    viogpu->vq.used.idx  = 0;
    condition_init(&viogpu->GPUBusy, "GPUBusy");
    trace("%p: Virtio GPU device initialized (instance %d)\n", regs, viogpu->instno);
}

static int viogpu_open(struct io ** ioptr, void * aux) {
    if (!ioptr || !aux) return -EINVAL;

    struct viogpu_device * const viogpu = aux;

    /* QEMU virtio-gpu rejects resource_id 0 (VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID) */
    viogpu->framebuffer_resource_id = (uint32_t)viogpu->instno + 1;

    viogpu->regs->config.gpu.num_scanouts = 1;
    viogpu->regs->config.gpu.events_clear = 0;
    viogpu->regs->config.gpu.events_read  = 0;
    viogpu->regs->config.gpu.num_capsets  = 0;

    viogpu->conf.width           = VIOGPU_FBUF_WIDTH;
    viogpu->conf.height          = VIOGPU_FBUF_HEIGHT;
    viogpu->conf.bytes_per_pixel = 4;
    viogpu->conf.horiz_stride    = viogpu->conf.width * viogpu->conf.bytes_per_pixel;
    viogpu->conf.vert_stride     = viogpu->conf.height;
    viogpu->color_space          = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;

    virtio_enable_virtq(viogpu->regs, 0);
    enable_intr_source(viogpu->irqno, VIOGPU_IRQ_PRIO, viogpu_isr, viogpu);

    int res = viogpu_map_buffer(viogpu);
    if (res) return res;

    *ioptr = ioaddref(&viogpu->base);
    return 0;
}

static void viogpu_reclaim(struct io * io) {
    if (!io) return;
    struct viogpu_device * const viogpu =
        (void*)io - offsetof(struct viogpu_device, base);

    viogpu_detach_backing(viogpu);
    viogpu_resource_unref(viogpu);
    viogpu_set_scanout(viogpu, 0);

    if (viogpu->fbuf_vma && viogpu->fbuf_sz)
        unmap_and_free_range((void*)viogpu->fbuf_vma, viogpu->fbuf_sz);

    viogpu->fbuf_sz  = 0;
    viogpu->fbuf_pma = NULL;
    viogpu->fbuf_vma = 0;
}

static int viogpu_ioctl(struct io * io, int op, void *arg) {
    if (!io) return -EINVAL;
    struct viogpu_device * const viogpu =
        (void*)io - offsetof(struct viogpu_device, base);

    switch (op) {
    case IOC_MAPBUF:
        if (!arg) return -EINVAL;
        *(void **)arg = (void*)viogpu->fbuf_vma;
        return 0;

    default:
        return -ENOTSUP;
    }
}

static long viogpu_write(struct io * io, const void * buf, long len) {
    struct viogpu_device * const d =
        (void*)io - offsetof(struct viogpu_device, base);
    if (buf != NULL && len != 0) return -EINVAL;

    if (viogpu_transfer_to_host(d) == 0)
        (void)viogpu_cmd_flush(d);
    return 0;
}


static void viogpu_isr(int irqno, void *aux) {
    (void)irqno;
    if (!aux) return;
    struct viogpu_device * viogpu = (struct viogpu_device *)aux;

    uint32_t st = viogpu->regs->interrupt_status;
    if (st) viogpu->regs->interrupt_ack = st;

    if (viogpu->cmd.response.type != 0) {
        condition_broadcast(&viogpu->GPUBusy);
    }
}

static inline void wait(struct viogpu_device * viogpu) {
    int pie;
    viogpu->cmd.response.type = 0;
    virtio_notify_avail(viogpu->regs, 0);
    pie = disable_interrupts();
    while (viogpu->cmd.response.type == 0)
        condition_wait(&viogpu->GPUBusy);
    restore_interrupts(pie);
}

static int viogpu_create_resource_2d(struct viogpu_device * viogpu) {
    struct virtio_gpu_resource_create_2d req = {0};

    req.hdr.type     = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    req.hdr.flags    = 0;
    req.hdr.ring_idx = 0;
    req.hdr.ctx_id   = 0;
    req.resource_id  = viogpu->framebuffer_resource_id;
    req.format       = (uint32_t)viogpu->color_space;
    req.width        = (uint32_t)viogpu->conf.width;
    req.height       = (uint32_t)viogpu->conf.height;

    viogpu->vq.desc[0].addr  = (uint64_t)&req;
    viogpu->vq.desc[0].len   = sizeof(req);
    viogpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    viogpu->vq.desc[0].next  = 1;

    viogpu->vq.desc[1].addr  = (uint64_t)&viogpu->cmd.response;
    viogpu->vq.desc[1].len   = sizeof(viogpu->cmd.response);
    viogpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    viogpu->vq.desc[1].next  = 0;

    viogpu->vq.avail.ring[viogpu->vq.avail.idx % VIOGPU_VIRTQ_LEN] = 0;
    viogpu->vq.avail.idx++;

    wait(viogpu);

    if (viogpu->cmd.response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("viogpu: create_2d failed (0x%x)\n", viogpu->cmd.response.type);
        return -EIO;
    }
    viogpu->cmd.response.type = 0;
    return 0;
}

static int viogpu_attach_backing(struct viogpu_device * viogpu) {
    struct virtio_gpu_resource_attach_backing req = {0};

    req.hdr.type      = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    req.resource_id   = viogpu->framebuffer_resource_id;
    req.nr_entries    = 1;
    req.entries[0].addr   = (uint64_t)viogpu->fbuf_pma;
    req.entries[0].length = (uint32_t)viogpu->fbuf_sz;

    viogpu->vq.desc[0].addr  = (uint64_t)&req;
    viogpu->vq.desc[0].len   = sizeof(req);
    viogpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    viogpu->vq.desc[0].next  = 1;

    viogpu->vq.desc[1].addr  = (uint64_t)&viogpu->cmd.response;
    viogpu->vq.desc[1].len   = sizeof(viogpu->cmd.response);
    viogpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    viogpu->vq.desc[1].next  = 0;

    viogpu->vq.avail.ring[viogpu->vq.avail.idx % VIOGPU_VIRTQ_LEN] = 0;
    viogpu->vq.avail.idx++;

    wait(viogpu);

    if (viogpu->cmd.response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("viogpu: attach_backing failed (0x%x)\n", viogpu->cmd.response.type);
        return -EIO;
    }
    viogpu->cmd.response.type = 0;
    return 0;
}

static int viogpu_set_scanout(struct viogpu_device * viogpu, uint32_t resource_id) {
    struct virtio_gpu_set_scanout req = {0};

    req.hdr.type     = VIRTIO_GPU_CMD_SET_SCANOUT;
    req.scanout_id   = 0;
    req.resource_id  = resource_id;
    req.r.x = 0; req.r.y = 0;
    req.r.width  = viogpu->conf.width;
    req.r.height = viogpu->conf.height;

    viogpu->vq.desc[0].addr  = (uint64_t)&req;
    viogpu->vq.desc[0].len   = sizeof(req);
    viogpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    viogpu->vq.desc[0].next  = 1;

    viogpu->vq.desc[1].addr  = (uint64_t)&viogpu->cmd.response;
    viogpu->vq.desc[1].len   = sizeof(viogpu->cmd.response);
    viogpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    viogpu->vq.desc[1].next  = 0;

    viogpu->vq.avail.ring[viogpu->vq.avail.idx % VIOGPU_VIRTQ_LEN] = 0;
    viogpu->vq.avail.idx++;

    wait(viogpu);

    if (viogpu->cmd.response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("viogpu: set_scanout failed (0x%x)\n", viogpu->cmd.response.type);
        return -EIO;
    }
    viogpu->cmd.response.type = 0;
    return 0;
}

static int viogpu_transfer_to_host(struct viogpu_device * viogpu) {
    viogpu->cmd.transfer.hdr.type   = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    viogpu->cmd.transfer.resource_id = viogpu->framebuffer_resource_id;
    viogpu->cmd.transfer.r.x = 0;
    viogpu->cmd.transfer.r.y = 0;
    viogpu->cmd.transfer.r.width  = viogpu->conf.width;
    viogpu->cmd.transfer.r.height = viogpu->conf.height;
    viogpu->cmd.transfer.offset = 0;

    viogpu->vq.desc[0].addr  = (uint64_t)&viogpu->cmd.transfer;
    viogpu->vq.desc[0].len   = sizeof(viogpu->cmd.transfer);
    viogpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    viogpu->vq.desc[0].next  = 1;

    viogpu->vq.desc[1].addr  = (uint64_t)&viogpu->cmd.response;
    viogpu->vq.desc[1].len   = sizeof(viogpu->cmd.response);
    viogpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    viogpu->vq.desc[1].next  = 0;

    viogpu->vq.avail.ring[viogpu->vq.avail.idx % VIOGPU_VIRTQ_LEN] = 0;
    viogpu->vq.avail.idx++;

    wait(viogpu);

    if (viogpu->cmd.response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("viogpu: xfer_to_host failed (0x%x)\n", viogpu->cmd.response.type);
        return -EIO;
    }
    viogpu->cmd.response.type = 0;
    return 0;
}

static int viogpu_cmd_flush(struct viogpu_device * viogpu) {
    viogpu->cmd.flush.hdr.type   = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    viogpu->cmd.flush.resource_id = viogpu->framebuffer_resource_id;
    viogpu->cmd.flush.r.x = 0;
    viogpu->cmd.flush.r.y = 0;
    viogpu->cmd.flush.r.width  = viogpu->conf.width;
    viogpu->cmd.flush.r.height = viogpu->conf.height;

    viogpu->vq.desc[0].addr  = (uint64_t)&viogpu->cmd.flush;
    viogpu->vq.desc[0].len   = sizeof(viogpu->cmd.flush);
    viogpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    viogpu->vq.desc[0].next  = 1;

    viogpu->vq.desc[1].addr  = (uint64_t)&viogpu->cmd.response;
    viogpu->vq.desc[1].len   = sizeof(viogpu->cmd.response);
    viogpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    viogpu->vq.desc[1].next  = 0;

    viogpu->vq.avail.ring[viogpu->vq.avail.idx % VIOGPU_VIRTQ_LEN] = 0;
    viogpu->vq.avail.idx++;

    wait(viogpu);

    if (viogpu->cmd.response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("viogpu: flush failed (0x%x)\n", viogpu->cmd.response.type);
        return -EIO;
    }
    viogpu->cmd.response.type = 0;
    return 0;
}

static int viogpu_detach_backing(struct viogpu_device * viogpu) {
    struct virtio_gpu_resource_detach_backing req = {0};

    req.hdr.type     = VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING;
    req.resource_id  = viogpu->framebuffer_resource_id;

    viogpu->vq.desc[0].addr  = (uint64_t)&req;
    viogpu->vq.desc[0].len   = sizeof(req);
    viogpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    viogpu->vq.desc[0].next  = 1;

    viogpu->vq.desc[1].addr  = (uint64_t)&viogpu->cmd.response;
    viogpu->vq.desc[1].len   = sizeof(viogpu->cmd.response);
    viogpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    viogpu->vq.desc[1].next  = 0;

    viogpu->vq.avail.ring[viogpu->vq.avail.idx % VIOGPU_VIRTQ_LEN] = 0;
    viogpu->vq.avail.idx++;

    wait(viogpu);

    if (viogpu->cmd.response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("viogpu: detach_backing failed (0x%x)\n", viogpu->cmd.response.type);
        return -EIO;
    }
    viogpu->cmd.response.type = 0;
    return 0;
}

static int viogpu_resource_unref(struct viogpu_device * viogpu) {
    struct virtio_gpu_resource_unref req = {0};

    req.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    req.resource_id = viogpu->framebuffer_resource_id;

    viogpu->vq.desc[0].addr  = (uint64_t)&req;
    viogpu->vq.desc[0].len   = sizeof(req);
    viogpu->vq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    viogpu->vq.desc[0].next  = 1;

    viogpu->vq.desc[1].addr  = (uint64_t)&viogpu->cmd.response;
    viogpu->vq.desc[1].len   = sizeof(viogpu->cmd.response);
    viogpu->vq.desc[1].flags = VIRTQ_DESC_F_WRITE;
    viogpu->vq.desc[1].next  = 0;

    viogpu->vq.avail.ring[viogpu->vq.avail.idx % VIOGPU_VIRTQ_LEN] = 0;
    viogpu->vq.avail.idx++;

    wait(viogpu);

    if (viogpu->cmd.response.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("viogpu: resource_unref failed (0x%x)\n", viogpu->cmd.response.type);
        return -EIO;
    }
    viogpu->cmd.response.type = 0;
    return 0;
}

static int viogpu_map_buffer(struct viogpu_device * viogpu) {
    viogpu->fbuf_sz = (uint64_t)viogpu->conf.width *
                 (uint64_t)viogpu->conf.height *
                 (uint64_t)viogpu->conf.bytes_per_pixel;


    viogpu->fbuf_vma = VIRTIO_GPU_FBUF_VMA;
    viogpu->fbuf_pma = alloc_phys_pages(viogpu->fbuf_sz / PAGE_SIZE);
    viogpu->fbuf_vma = (uintptr_t) map_range(viogpu->fbuf_vma, viogpu->fbuf_sz, viogpu->fbuf_pma, PTE_U | PTE_R | PTE_W);
    if (!viogpu->fbuf_pma) return -ENOMEM;

    // Create resource, attach backing, set scanout
    int res;
    res = viogpu_create_resource_2d(viogpu);                 
    if (res) return res;
    res = viogpu_attach_backing(viogpu);                     
    if (res) return res;
    res = viogpu_set_scanout(viogpu, viogpu->framebuffer_resource_id); 
    if (res) return res;

    return 0;
}