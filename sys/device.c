// device.c - Device manager and device operations
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "device.h"
#include "devimpl.h"
#include "error.h"
#include "conf.h"
#include "string.h"
#include "assert.h"
#include "heap.h"

#include <stddef.h>
#include <limits.h> // INT_MAX

// INTERNAL MACRO DEFINITIONS
//

// The ISPOW2 macro evaluates to 1 if its argument is either zero or a power of
// two. The argument must be an integer type. Cast pointers to `uintptr_t` to
// test pointer alignment.

#define ISPOW2(n) (((n)&((n)-1)) == 0)

// INTERNAL TYPE DEFINITIONS
//

// Device list entry

struct device_record {
    struct device_record * next;
    const char * name;
    enum device_type type;
    void * device_struct;
};

// INTERNAL FUNCTION DECLARATIONS
//

extern const char * device_type_short_name(enum device_type type);

// EXPORTED GLOBAL VARIABLES
//

char devmgr_initialized = 0;

// INTERNAL GLOBAL VARIABLES
//

static struct device_record * devl_head;
static struct device_record * devl_tail;

// EXPORTED FUNCTION DEFINITIONS
//

void devmgr_init(void) {
    trace("%s()", __func__);
    devmgr_initialized = 1;
}

void register_device(const char * name, enum device_type type, void * device_struct) {
    struct device_record * dev;

    assert (devmgr_initialized);
    assert (name != NULL);

    dev = kcalloc(1, sizeof(*dev));

    dev->name = name;
    dev->device_struct = device_struct;

    // Insert device at end of device linked list. We go through the list to
    // determine the instance number of the device (which we return).

    if (devl_tail != NULL)
        devl_tail->next = dev;
    else
        devl_head = dev;
    devl_tail = dev;
}

void * find_device(const char * name, enum device_type type, int instno) {
    struct device_record * dev;
    int k = 0;

    trace("%s(%s/%s%d)", __func__, device_type_short_name(type), name, instno);

    // Find numbered instance of device in devtab

    for (dev = devl_head; dev != NULL; dev = dev->next) {
        assert (dev->name != NULL);

        if (strcmp(name, dev->name) == 0) {
            if (k++ == instno)
                return dev->device_struct;
        }
    }

    debug("Device %s/%s%d not found", device_type_short_name(type), name, instno);
    return NULL;
}

const char * device_type_short_name(enum device_type type) {
    switch (type) {
    case DEV_SERIAL: return "ser";
    case DEV_STORAGE: return "sto";
    case DEV_VIDEO: return "vid";
    default: return "UNK";
    }
}

int serial_open(struct serial * ser) {
    if (ser->intf->open != NULL)
        return ser->intf->open(ser);
    else
        return -ENOTSUP;
}

void serial_close(struct serial * ser) {
    if (ser->intf->close != NULL)
        return ser->intf->close(ser);
}

int serial_recv(struct serial * ser, void * buf, unsigned int bufsz) {
    unsigned int const blksz = ser->intf->blksz;

    if (ser->intf->recv != NULL) {
        // bufsz may be 0, but if non-zero, must be at least blksz; round bufsz
        // to a multiple of blksz.
        if (0 <= (int)bufsz && (bufsz == 0 || blksz <= bufsz))
            return ser->intf->recv(ser, buf, bufsz / blksz * blksz);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

int serial_send(struct serial * ser, const void * buf, unsigned int buflen) {
    unsigned int const blksz = ser->intf->blksz;

    if (ser->intf->send != NULL) {
        // buflen may be 0, but if non-zero, must be at least blksz; round
        // buflen to a multiple of blksz.
        if (0 <= (int)buflen && (buflen == 0 || blksz <= buflen))
            return ser->intf->send(ser, buf, buflen / blksz * blksz);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

int serial_cntl(struct serial * ser, int op, void * arg) {
    if (ser->intf->cntl != NULL)
        return ser->intf->cntl(ser, op, arg);
    else
        return -ENOTSUP;
}

unsigned int serial_blksz(const struct serial * ser) {
    return ser->intf->blksz;
}

// STORAGE DEVICE
// 

int storage_open(struct storage * sto) {
    if (sto->intf->open != NULL)
        return sto->intf->open(sto);
    else
        return -ENOTSUP;
}

void storage_close(struct storage * sto) {
    if (sto->intf->close != NULL)
        return sto->intf->close(sto);
}

long storage_fetch (
    struct storage * sto,
    unsigned long long pos,
    void * buf,
    unsigned long bufsz)
{
    unsigned int const blksz = sto->intf->blksz;

    if (sto->intf->fetch != NULL) {
        // bufsz may be 0, but if non-zero, must be at least blksz; round
        // bufsz to a multiple of blksz.
        if (0 <= (long)bufsz && (bufsz == 0 || blksz <= bufsz) && (pos % blksz == 0))
            return sto->intf->fetch(sto, pos, buf, bufsz);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

long storage_store (
    struct storage * sto,
    unsigned long long pos,
    const void * buf,
    unsigned long buflen)
{
    unsigned int const blksz = sto->intf->blksz;

    if (sto->intf->store != NULL) {
        // buflen may be 0, but if non-zero, must be at least blksz; round
        // buflen to a multiple of blksz.
        if (0 <= (long)buflen && (buflen == 0 || blksz <= buflen) && (pos % blksz == 0))
            return sto->intf->store(sto, pos, buf, buflen);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

int storage_cntl(struct storage * sto, int op, void * arg) {
    if (sto->intf->cntl != NULL)
        return sto->intf->cntl(sto, op, arg);
    else
        return -ENOTSUP;
}

unsigned int storage_blksz(const struct storage * sto) {
    return sto->intf->blksz;
}

unsigned long long storage_capacity(const struct storage * sto) {
    return sto->capacity;
}

// VIDEO DEVICE
//

int video_open(struct video * vid, int mode, void ** fbptr) {
    if (vid->intf->open != NULL)
        return vid->intf->open(vid, mode, fbptr);
    else
        return -ENOTSUP;
}

void video_close(struct video * vid) {
    if (vid->intf->close != NULL)
        return vid->intf->close(vid);
}

void video_flush(struct video * vid) {
    if (vid->intf->flush != NULL)
        return vid->intf->flush(vid);
}

int video_cntl(struct video * vid, int op, void * arg) {
    if (vid->intf->cntl != NULL)
        return vid->intf->cntl(vid, op, arg);
    else
        return -ENOTSUP;
}