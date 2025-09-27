/*! @file vioblk.c 
    @brief VirtIO serial port (console)
    @copyright Copyright (c) 2024-2025 University of Illinois

*/


#include "devimpl.h"
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
#include "console.h"
#include "uio.h" // FCNTL

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

struct vioblk_storage {
    struct storage base;
    volatile struct virtio_mmio_regs * regs; ///< VirtIO device registers
    int irqno;
    char opened;

    unsigned int blksz;         ///< The number of bytes in a block

    unsigned long long size;    ///< The size of device in bytes
    unsigned long long blkcnt;  ///< The size of device in blksz blocks

    /**
     * @brief Anonymous struct with the queues to interact with the device
     */
    struct {
        struct condition used_updated; ///< This condition is signalled when used.idx is updated
        struct lock lock; ///< This lock provides exclusive access to virtq

        /**
         * @brief The avail virtqueue for interacting with the VirtIO device
         */

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        /**
         * @brief The used virtqueue for interacting with the VirtIO device
         */

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        struct virtq_desc desc[4]; ///< The first descriptor is an indirect descriptor and is the one used in the avail and used rings. The second descriptor points to the header, the third points to the data, and the fourth to the status byte.
        struct vioblk_request_header req_header; ///< The request header to pass to the VirtIO device
        uint8_t req_status;
    } vq;
};

// INTERNAL FUNCTION DECLARATIONS
//

/**
 * @brief Sets the virtq avail and virtq used queues such that they are available for use. (Hint, read virtio.h) Enables the interupt line for the virtio device and sets necessary flags in vioblk device.
 * @param sto Storage IO struct for the storage device
 * @return Return success or error code
*/
static int vioblk_storage_open (struct storage * sto);

/**
 * @brief Resets the virtq avail and virtq used queues and sets necessary flags in vioblk device.
 * @param sto Storage IO struct for the storage device
 * @return None
 */
static void vioblk_storage_close (struct storage * sto);

/**
 * @brief Reads bytecnt number of bytes from the disk and writes them to buf. Achieves this by repeatedly setting the appropriate registers to request a block from the disk, waiting until the data has been populated in block buffer cache, and then writes that data out to buf. Thread sleeps while waiting for the disk to service the request. Returns the number of bytes successfully read from the disk.
 * @param sto Storage IO struct for the storage device
 * @param pos The starting position for the read within the VirtIO device
 * @param buf A pointer to the buffer to fill with the read data
 * @param bytecnt The number of bytes to read from the VirtIO device into the buffer
 * @return The number of bytes read from the device
 */
static long vioblk_storage_fetch (
    struct storage * sto,
    unsigned long long pos,
    void * buf,
    unsigned long bytecnt);
    
/**
 * @brief Writes n number of bytes from the parameter buf to the disk. The size of the virtio device should not change. You should only overwrite existing data. Write should also not create any new files. Achieves this by filling up the block buffer cache and then setting the appropriate registers to request the disk write the contents of the cache to the specified block location. Thread sleeps while waiting for the disk to service the request. Returns the number of bytes successfully written to the disk.
 * @param sto Storage IO struct for the storage device
 * @param pos The starting position for the write within the VirtIO device
 * @param buf A pointer to the buffer with the data to write
 * @param bytecnt The number of bytes to write to the VirtIO device from the buffer
 * @return The number of bytes written to the device
 */
static long vioblk_storage_store (
    struct storage * sto,
    unsigned long long pos,
    const void * buf,
    unsigned long bytecnt);

static int vioblk_storage_cntl (struct storage * sto, int op, void * arg);

/**
* @brief The interrupt handler for the VirtIO device. When an interrupt occurs, the system will call this function.
* @param irqno The interrupt request number for the VirtIO device
* @param A generic pointer for auxiliary data.
* @return None
*/
static void vioblk_isr(int srcno, void * aux);
        
        

// EXPORTED FUNCTION DEFINITIONS
//

// Attaches a VirtIO block device. Declared and called directly from virtio.c.

void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct vioblk_storage * vbd;
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

    // Initialize interface after getting blksz
    static struct storage_intf vioblk_intf = {
        .blksz = 512,
        .open = vioblk_storage_open,
        .close = vioblk_storage_close,
        .fetch = vioblk_storage_fetch,
        .store= vioblk_storage_store,
        .cntl = vioblk_storage_cntl
    };

    // Allocate initialize device struct

    vbd = kcalloc(1, sizeof(struct vioblk_storage));

    vbd->blksz = blksz;


    vbd->regs = regs;
    vbd->blksz = blksz;

    // vbd->instno filled later
    vbd->irqno = irqno;

    // capacity in bytes should fit in an unsigned long long
    assert (vbd->regs->config.blk.capacity < ULLONG_MAX / 512);
    vbd->size = vbd->regs->config.blk.capacity * 512ULL;
    debug("%p: virtio block device capacity is %lu", regs, vbd->size);

    vbd->blkcnt = vbd->size / blksz;
    vbd->size = vbd->blkcnt * blksz; // in case size not multiple of blksz
    
    // Pre-initialize as much of the request virtq as possible. We use a very
    // simple scheme of one transaction at a time. A higher-performance scheme
    // may allow multiple concurrent requests.
    // Note: entire vbd struct is zero-initialized, so we only need to set the
    // non-zero members of the virtq.

    // The first descriptor (index 0 in descriptor array) is the one used in the
    // avail and used rings. It is an indirect descriptor that points to three
    // chained descriptors, vbd->vq.desc[1..3].

    vbd->vq.desc[0].addr = (uintptr_t)(vbd->vq.desc + 1);
    vbd->vq.desc[0].len = 3 * sizeof(struct virtq_desc);
    vbd->vq.desc[0].flags = VIRTQ_DESC_F_INDIRECT;
    vbd->vq.desc[0].next = -1;

    // First descriptor in indirect descriptor chain points to request header.
    vbd->vq.desc[1].addr = (uintptr_t)&vbd->vq.req_header;
    vbd->vq.desc[1].len = sizeof(struct vioblk_request_header);
    vbd->vq.desc[1].flags = VIRTQ_DESC_F_NEXT;
    vbd->vq.desc[1].next = 1; // relative chain

    // Second descriptor in indirect descriptor chain points to data.
    vbd->vq.desc[2].flags = VIRTQ_DESC_F_NEXT;
    vbd->vq.desc[2].next = 2;

    // Third descriptor in indirect descriptor chain points to request status.
    vbd->vq.desc[3].addr = (uintptr_t)&vbd->vq.req_status;
    vbd->vq.desc[3].len = 1;
    vbd->vq.desc[3].flags = VIRTQ_DESC_F_WRITE;
    vbd->vq.desc[3].next = -1;

    condition_init(&vbd->vq.used_updated, "vioblk.vq.used_updated");
    lock_init(&vbd->vq.lock);

    // Attach queues

    virtio_attach_virtq (
        regs, 0, 1,
        (uintptr_t)&vbd->vq.desc,
        (uintptr_t)&vbd->vq.used,
        (uintptr_t)&vbd->vq.avail);

    // Register device
    storage_init(&vbd->base, &vioblk_intf, vbd->size);
    register_device(VIOBLK_NAME, DEV_STORAGE, vbd);
    
    // Signal initialization complete

    regs->status |= VIRTIO_STAT_DRIVER_OK;    
    __sync_synchronize(); // fence o,oi
}

int vioblk_storage_open (struct storage * sto){
    struct vioblk_storage * const vbd = 
        (void*)sto - offsetof(struct vioblk_storage, base);

	trace("%s(vbd={regs=%p})", __func__, vbd->regs);

    if (vbd->opened)
        return -EBUSY;
    
    vbd->vq.avail.idx = 0;
    vbd->vq.used.idx = 0;

    virtio_enable_virtq(vbd->regs, 0);
    enable_intr_source(vbd->irqno, VIOBLK_INTR_PRIO, vioblk_isr, vbd);

    vbd->opened = 1;
    return 0;
}

// Must be called with interrupts enabled to ensure there are no pending
// interrupts (ISR will not execute after closing).
void vioblk_storage_close (struct storage * sto) {
    struct vioblk_storage * const vbd = 
        (void*)sto - offsetof(struct vioblk_storage, base);
    
	trace("%s()", __func__);

    assert(vbd->opened);

    virtio_reset_virtq(vbd->regs, 0);
    disable_intr_source(vbd->irqno);

    vbd->opened = 0;
}


long vioblk_storage_fetch (
    struct storage * sto,
    unsigned long long pos,
    void * buf,
    unsigned long bytecnt)
{
    struct vioblk_storage * const vbd = 
        (void*)sto - offsetof(struct vioblk_storage, base);
    int pie;

    trace("%s(pos=%ld,buf=%p,bytecnt=%ld)", __func__, pos, buf, bytecnt);

    // check that buf is not in user memory space
    // assert(buf < (void *)UMEM_START_VMA);

    if (vbd->size < pos)
        return -EINVAL;

    // Truncate read to end of device

    if (vbd->size - pos < bytecnt)
        bytecnt = vbd->size - pos;

    // Zero-length reads are allowed but do nothing

    if (bytecnt == 0)
        return 0;

    // Submit virtq request

    lock_acquire(&vbd->vq.lock);

    vbd->vq.req_header.sector = pos / vbd->blksz;
    vbd->vq.req_header.type = VIRTIO_BLK_T_IN;
    vbd->vq.desc[2].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    vbd->vq.desc[2].addr = (uintptr_t)buf;
    vbd->vq.desc[2].len = bytecnt;
    __sync_synchronize(); // fence w,w
    vbd->vq.avail.idx += 1;

    virtio_notify_avail(vbd->regs, 0);

    // Wait for descriptor to return via used ring

    pie = disable_interrupts();
    while (vbd->vq.used.idx != vbd->vq.avail.idx)
        condition_wait(&vbd->vq.used_updated);
    restore_interrupts(pie);

    lock_release(&vbd->vq.lock);

    switch (vbd->vq.req_status) {
    case VIRTIO_BLK_S_OK:
        return bytecnt;
    case VIRTIO_BLK_S_IOERR:
        return -EIO;
    case VIRTIO_BLK_S_UNSUPP:
        return -ENOTSUP;
    default:
        panic(NULL);
    }
}


static long vioblk_storage_store (
    struct storage * sto,
    unsigned long long pos,
    const void * buf,
    unsigned long bytecnt)
{
    struct vioblk_storage * const vbd = 
        (void*)sto - offsetof(struct vioblk_storage, base);
    int pie;

    trace("%s(buf=%p,bytecnt=%ld)", __func__, buf, bytecnt);

    // check that buf is not in user memory space
    // assert(buf < (void *)UMEM_START_VMA);
    
    if (vbd->size < pos)
        return -EINVAL;

    // Truncate read to end of device

    if (vbd->size - pos < bytecnt)
        bytecnt = vbd->size - pos;

    // Zero-length writes are allowed but do nothing

    if (bytecnt == 0)
        return 0;

    // Submit virtq request

    lock_acquire(&vbd->vq.lock);

    vbd->vq.req_header.sector = pos / vbd->blksz;
    vbd->vq.req_header.type = VIRTIO_BLK_T_OUT;
    vbd->vq.desc[2].flags = VIRTQ_DESC_F_NEXT;
    vbd->vq.desc[2].addr = (uintptr_t)buf;
    vbd->vq.desc[2].len = bytecnt;
    __sync_synchronize(); // fence w,w
    vbd->vq.avail.idx += 1;

    virtio_notify_avail(vbd->regs, 0);

    // Wait for descriptor to return via used ring

    pie = disable_interrupts();
    while (vbd->vq.used.idx != vbd->vq.avail.idx)
        condition_wait(&vbd->vq.used_updated);
    restore_interrupts(pie);

    lock_release(&vbd->vq.lock);

    switch (vbd->vq.req_status) {
    case VIRTIO_BLK_S_OK:
        return bytecnt;
    case VIRTIO_BLK_S_IOERR:
        return -EIO;
    case VIRTIO_BLK_S_UNSUPP:
        return -ENOTSUP;
    default:
        panic(NULL);
    }
}

int vioblk_storage_cntl (struct storage * sto, int op, void * arg) {
    struct vioblk_storage * const vbd = 
        (void*)sto - offsetof(struct vioblk_storage, base);
    
    trace("%s(op=%d,arg=%p)", __func__, op, arg);
    
    switch (op) {
    case FCNTL_GETEND:
        *(unsigned long long*)arg = vbd->size;
        return 0;
    default:
        return -ENOTSUP;
    }
}

void vioblk_isr(int irqno, void * aux) {
    struct vioblk_storage * const vbd = aux;
    uint32_t intr_status;

    intr_status = vbd->regs->interrupt_status;
    vbd->regs->interrupt_ack = intr_status;
    __sync_synchronize(); // fence o,r

    trace("%s(irqno=%d)", __func__, irqno);

    if (intr_status & 1)
        condition_broadcast(&vbd->vq.used_updated);
}