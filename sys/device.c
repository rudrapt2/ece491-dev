// device.c - Device manager and device operations
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "device.h"
#include "error.h"
#include "uio.h"
#include "conf.h"
#include "string.h"
#include "assert.h"
#include "fsimpl.h"
#include "devimpl.h"

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

struct devfs_listing {
    struct uio base;
    struct device_record * dev;
};

// INTERNAL FUNCTION DECLARATIONS
//

static int devfs_open(struct filesystem * fs, const char * name, struct uio ** uioptr);

static void devfs_listing_close(struct uio * uio);
static long devfs_listing_read(struct uio * uio, void * buf, unsigned long bufsz);

static const char * device_type_label(enum device_type type);

// EXPORTED GLOBAL VARIABLES
//

char devmgr_initialized = 0;

// INTERNAL GLOBAL VARIABLES
//

static const struct filesystem devfs = {
    .open = &devfs_open
};

static const struct uio_intf devfs_lsintf = {
    .close = &devfs_listing_close,
    .read = &devfs_listing_read
};

static struct device_record * devl_head;
static struct device_record * devl_tail;

// EXPORTED FUNCTION DEFINITIONS
//

void devmgr_init(void) {
    trace("%s()", __func__);
    devmgr_initialized = 1;
}

int register_device(const char * name, enum device_type type, void * device_struct) {
    struct device_record * dev;
    int instno = 0;
    int i;

    assert (devmgr_initialized);
    assert (name != NULL);

    dev = kcalloc(1, sizeof(*dev));

    dev->name = name;
    dev->device_struct = device_struct;

    // Insert device at end of device linked list

    if (devl_tail != NULL)
        devl_tail->next = dev;
    else
        devl_head = dev;
    devl_tail = dev;
}

void * find_device(const char * name, enum device_type type, int instno) {
    struct device_record * dev;
    int k = 0;

    trace("%s(\"%s\",\"%s\",%d)", __func__, name, device_type_label(type), instno);

    // Find numbered instance of device in devtab

    for (dev = devl_head; dev != NULL; dev = dev->next) {
        assert (dev->name != NULL);

        if (strcmp(name, dev->name) == 0) {
            if (k++ == instno)
                return dev->device_struct;
        }
    }

    debug("Device %s/%s%d not found", device_type_label(type), name, instno);
    return NULL;
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
    if (ser->intf->recv != NULL) {
        if (0 <= (int)bufsz)
            return ser->intf->recv(ser, buf, bufsz);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

int serial_send(struct serial * ser, const void * buf, unsigned int buflen) {
    if (ser->intf->send != NULL) {
        if (0 <= (int)buflen)
            return ser->intf->send(ser, buf, buflen);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

int serial_cntl(struct serial * ser, int op, void * arg) {
    if (ser->intf->cntl != NULL)
        return serial_cntl(ser, op, arg);
    else
        return -ENOTSUP;
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
    if (sto->intf->fetch != NULL) {
        if (0 <= (long)bufsz)
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
    if (sto->intf->store != NULL) {
        if (0 <= (long)buflen)
            return sto->intf->store(sto, pos, buf, buflen);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

int storage_cntl(struct storage * sto, int op, void * arg) {
    if (sto->intf->cntl != NULL)
        return storage_cntl(sto, op, arg);
    else
        return -ENOTSUP;
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
        return video_cntl(vid, op, arg);
    else
        return -ENOTSUP;
}

// INTERNAL FUNCTION DEFINITIONS
//

int devfs_open (
    struct filesystem * fs __attribute__ ((unused)),
    const char * name,
    struct uio ** uioptr)
{
    struct devfs_listing * ls;
    int instno;
    int k = 0;

    trace("%s(%s)", __func__, name);

    // If name is NULL, then the open call is for a file listing. Return a
    // special listing uio object.

    if (name == NULL) {
        ls = kcalloc(1, sizeof(*ls));
        ls->dev = devl_head;
        return uio_init1(&ls->base, &devfs_lsintf);
    }

    // If name is not NULL, the open call is for a file (device). The name
    // passed to open() consists of the device name followed by an isntance
    // number (starting at zero). Parse the provided file name into the device
    // name length (for use with strncmp()) and the instance number.

    // ... TODO
    

    debug("Device %s%d not found", name, instno);
    return -ENODEV;
}

const char * device_type_label(enum device_type type) {
    switch (type) {
    case DEV_SERIAL: return "ser";
    case DEV_STORAGE: return "sto";
    case DEV_VIDEO: return "vid";
    default: return "UNK";
    }
}

#if 0 // will need this for devfs
/**
 * @brief Parses a device specification, which is an ASCII string identifying a
 * device instance.
 * @details The specification string must consist of one or more non-digit ASCII
 * printable characters representing the device name followed by one or more
 * decimal digits representing the instance number. If the `spec` points to a
 * well-formed device specification, the device_parse_spec function
 * null-terminates the device name in place and returns the instance number as a
 * non-negative integer. If `spec` is not well-formed, device_parse_spec returns
 * -EINVAL.
 * @param spen device specification string
 * @return extracted instance number on success, or negative error code on
 * failure
 */
int parse_devfs_name(char * spec) {
    char * s = spec; // position in string
    char * p = NULL; // start of number
    unsigned long ulval; // for strtoul
    char * end; // for strtoul

    while (*s != '\0') {
        if ('0' <= *s && *s <= '9') {
            if (p == NULL)
                p = s;
        } else if (' ' < *s && *s < '\x7f')
            p = NULL;
        else
            return -EINVAL;
        
        s += 1;
    }

    if (p != NULL && p != s) {
        ulval = strtoul(p, &end, 10);
        if (ulval <= INT_MAX && *end == '\0') {
            *p = '\0';
            return (int)ulval;
        }
    }

    return -EINVAL;
}

#endif