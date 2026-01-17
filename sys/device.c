// device.h - Device manager
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "device.h"

#include <limits.h>  // INT_MAX
#include <stddef.h>

#include "conf.h"
#include "error.h"
#include "fsimpl.h"
#include "heap.h"
#include "misc.h"
#include "string.h"
#include "ioimpl.h"

// INTERNAL TYPE DEFINITIONS
//

// Device list entry

struct device_record {
    struct device_record * next;    // Next device record in linked list
    device_openfn_t openfn;         // Function for opening device
    void * ofaux;                   // Aux. argument for openfn
    unsigned char clen;             // length of name without instance number
    char name[];                    // device name with instance number
};

// devfs listing io object

struct devfs_lsio {
    struct io base;
    const struct device_record * next;
};

// INTERNAL FUNCTION DECLARATIONS
//

static int devfs_open_listing(struct filesystem * fs, struct io ** ioptr);
static long devfs_listing_read(struct io * io, void * buf, long bufsz);

static int devfs_open_file(struct filesystem * fs, const char * name, struct io ** ioptr);

// EXPORTED GLOBAL VARIABLES
//

char devmgr_initialized = 0;

struct filesystem devfs = {
    .open_listing = devfs_open_listing,
    .open_file = &devfs_open_file
};

// DEVMGR INTERNAL GLOBAL VARIABLES
//

static struct device_record * devlist;

// EXPORTED FUNCTION DEFINITIONS
//

void devmgr_init(void) {
    trace("%s()", __func__);
    devmgr_initialized = 1;
}

int register_device (
    const char * name,
    int unique,
    device_openfn_t openfn,
    void * ofaux)
{
    struct device_record ** dptr;
    struct device_record * dev;
    int instno = 0;
    size_t clen;
    size_t namelen;

    trace("%s("\%s", %d)", __func__, name, unique);

    assert(devmgr_initialized);
    assert(name != NULL);
    assert(openfn != NULL);

    clen = strlen(name);
    namelen = clen;
    dptr = &devlist;

    if (255 < clen)
        clen = 255;

    // If the device is unique, place it at the start of the list. Otherwise,
    // scan through list to determine device instance number and add the new
    // device at the end.

    if (!unique) {
        while ((dev = *dptr) != NULL) {
            instno += (strncmp(name, dev->name, dev->clen) == 0);
            dptr = &dev->next;
        }
    }
    
    dev = kmalloc(sizeof(*dev) + namelen + 1);
    memset(dev, 0, sizeof(*dev));

    dev->openfn = openfn;
    dev->ofaux = ofaux;
    dev->next = *dptr;
    *dptr = dev;
    
    return instno;
}

extern int open_device(const char * name, struct io ** ioptr) {
    struct device_record * dev;
    int instno = 0;

    trace("%s(\"%s\")", __func__, name);

    // Find numbered instance of device in devlist

    for (dev = devlist; dev != NULL; dev = dev->next) {
        if (strncmp(name, dev->name, dev->clen) == 0) {
            if (strcmp(name + dev->clen, dev->name + dev->clen) == 0)
                return dev->openfn(instno, ioptr, dev->ofaux);
            instno += 1;
        }
    }

    return -ENOENT;
}

// INTERNAL FUNCTION DEFINITIONS
//


int devfs_open_listing(struct filesystem * fs, struct io ** ioptr) {
    static const struct iointf devfs_lsio_intf = {
        .implname = "devfs_ls",
        .reclaim = (void(*)(struct io*))&kfree,
        .read = &devfs_listing_read
    };

    struct devfs_lsio * lsio;

    assert (fs == &devfs);
    assert (ioptr != NULL);

    lsio = kcalloc(1, sizeof(*lsio));
    lsio->next = devlist;

    *ioptr = ioinit(&lsio->base, &devfs_lsio_intf, 1, 1);
    return 0;
}

long devfs_listing_read(struct io * io, void * buf, long bufsz) {
    struct devfs_lsio * const lsio = (struct devfs_lsio*)io;

    if (lsio->next != NULL) {
        strncpy(buf, lsio->next->name, bufsz);
        lsio->next = lsio->next->next;
        return strlen(buf)+1;
    }
        return 0;
}

int devfs_open_file (
    struct filesystem * fs,
    const char * name,
    struct io ** ioptr)
{
    assert (fs == &devfs);
    assert (name != NULL);
    assert (ioptr != NULL);

    trace("%s("\%s")", __func__, name);

    return open_device(name, ioptr);
}