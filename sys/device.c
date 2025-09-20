// device.c - Device manager and device operations
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "device.h"
#include "devimpl.h"
#include "fsimpl.h"
#include "uioimpl.h"
#include "error.h"
#include "conf.h"
#include "string.h"
#include "assert.h"
#include "heap.h"
#include "misc.h" // ISPOW2

#include <stddef.h>
#include <limits.h> // INT_MAX

// INTERNAL MACRO DEFINITIONS
//

// INTERNAL TYPE DEFINITIONS
//

// Device list entry

struct device_record {
    struct device_record * next;
    const char * name;
    int instno;
    enum device_type type;
    void * device_struct;
};

struct devfs_listing_uio {
    struct uio base;
    const struct device_record * dev;
};

struct devfs_serial_uio {
    struct uio base;
    struct serial * ser;
};

// INTERNAL FUNCTION DECLARATIONS
//

static int devfs_open (
    struct filesystem * fs, const char * name, struct uio ** uioptr);

static int devfs_open_listing(struct uio ** uioptr);

static void devfs_listing_close(struct uio * uio);

static void devfs_listing_read (
    struct uio * uio, void * buf, unsigned long bufsz);

static int devfs_open_device(const char * name, struct uio ** uioptr);
static int devfs_open_serial(struct serial * ser, struct uio ** uioptr) {
static int devfs_open_storage(struct storage * ser, struct uio ** uioptr) {
static int devfs_open_video(struct video * ser, struct uio ** uioptr) {

static void devfs_serial_close(struct uio * uio);
static long devfs_serial_read(struct uio * uio, void * buf, unsigned long bufsz);
static long devfs_serial_write(struct uio * uio, const void * buf, unsigned long buflen);
static int devfs_serial_cntl(struct uio * uio, int op, void * arg);

static void devfs_storage_close(struct uio * uio);
static long devfs_storage_read(struct uio * uio, void * buf, unsigned long bufsz);
static long devfs_storage_write(struct uio * uio, const void * buf, unsigned long buflen);
static int devfs_storage_cntl(struct uio * uio, int op, void * arg);

static void devfs_video_close(struct uio * uio);
static long devfs_video_read(struct uio * uio, void * buf, unsigned long bufsz);
static long devfs_video_write(struct uio * uio, const void * buf, unsigned long buflen);
static int devfs_video_cntl(struct uio * uio, int op, void * arg);

// EXPORTED GLOBAL VARIABLES
//

char devmgr_initialized = 0;

// INTERNAL GLOBAL VARIABLES
//

static struct device_record * devlist;

static const struct filesystem devfs = {
    .open = &devfs_open
};

// Array of /uio_intf/ structures indexed by device type. The first entry,
// corresponding to DEV_UNDEF is for /devfs_listing_uio/.

static const struct uio_intf devfs_uio_intfs[] = {
    [DEV_UNDEF] = {
        .close = &devfs_listing_close,
        .read = &devfs_listing_read
    },
    [DEV_SERIAL] = {
        .close = &devfs_serial_close,
        .read = &devfs_serial_read,
        .write = &devfs_serial_write,
        .cntl = &devfs_serial_cntl
    },
    [DEV_STORAGE] = {
        .close = &devfs_storage_close,
        .read = &devfs_storage_read,
        .write = &devfs_storage_write,
        .cntl = &devfs_storage_cntl
    },
    [DEV_VIDEO] = {
        .close = &devfs_video_close,
        .read = &devfs_video_read,
        .write = &devfs_video_write,
        .cntl = &devfs_video_cntl
    }
};

// EXPORTED FUNCTION DEFINITIONS
//

void devmgr_init(void) {
    trace("%s()", __func__);
    devmgr_initialized = 1;
}

int register_device(const char * name, enum device_type type, void * device_struct) {
    struct device_record ** dptr;
    struct device_record * dev;
    size_t namesz;
    int instno = 0;

    assert (devmgr_initialized);
    assert (name != NULL);

    // Check that /type/ is a valid device type.

    switch (type) {
    case DEV_SERIAL:
    case DEV_STORAGE:
    case DEV_VIDEO:
        break;
    default:
        return -EINVAL;
    }

    // Walk through device list to determine the instance number for the device.
    // When we finish, /dptr/ points to the /next/ member of the last device in
    // the list (if not list is not empty) or to /devlist/ if the list is empty
    // (devlist == NULL).

    while (*dptr != NULL) {
        instno += (strcmp(name, (*dptr)->name) == 0);
        dptr = &(*dptr)->next;
    }

    // Allocate device_record struct and fill it in.

    dev = kcalloc(1, sizeof(*dev));

    dev->name = name;
    dev->instno = instno;
    dev->type = type;
    dev->device_struct = device_struct;

    // Insert device at end of device linked list using /dptr/.

    *dptr = dev;
    return instno;
}

void * find_device(const char * name, enum device_type type, int instno) {
struct device_record * find_device_record (
    const char * name, enum device_type type, int instno)
{
    struct device_record * dev;

    trace("%s(%s/%s%d)", __func__, device_type_short_name(type), name, instno);

    // Find numbered instance of device in devlist

    for (dev = devlist; dev != NULL; dev = dev->next) {
        if (dev->type == type && dev->instno == instno &&
            strcmp(name, dev->name) == 0)
        {
            return dev->device_struct;
        }
    }

    debug("Device %s/%s%d not found", device_type_short_name(type), name, instno);
    return NULL;
}

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

int mount_devfs(const char * name) {
    return mount_fs(name, (struct filesystem*)&devfs);
}

// INTERNAL FUNCTION DEFINITIONS
//


int devfs_open (
    struct filesystem * fs, const char * name, struct uio ** uioptr)
{
    if (name == NULL || *name == '\0')
        return devfs_open_listing(uioptr);
    else
        return devfs_open_device(name, uioptr);
}

int devfs_open_listing(struct uio ** uioptr) {
    struct devfs_listing_uio * ls;

    ls = kcalloc(1, sizeof(*ls));
    ls->dev = devlist;

    // Note: The listing uio_intf is at index DEV_UNDEF in /devfs_uio_intfs/.
    *uioptr = uio_init1(&ls->base, devfs_uio_intfs + DEV_UNDEF);

    return 0;
}

void devfs_listing_close(struct uio * uio) {
    struct devfs_listing_uio * const ls = (struct devfs_listing_uio*)uio;
    kfree(ls);
}

void devfs_listing_read (
    struct uio * uio, void * buf, unsigned long bufsz)
{
    struct devfs_listing_uio * const ls = (struct devfs_listing_uio*)uio;
    size_t len;

    if (ls->dev != NULL) {
        len = snprintf(buf, bufsz, "%s%d", ls->dev->name, ls->dev->instno);
        ls->dev = ls->dev->next;
        return (len < bufsz) ? len : bufsz;
    } else
        return 0;
}

int devfs_open_device(const char * name, struct uio ** uioptr) {
    const char * dp = NULL; // position of trailing sequence of digits
    const char * s; // position in string
    struct device_record * dev;
    unsigned long instno;

    // Parse name as device name and instance number

    for (s = name; *s != '\0'; s++) {
        if ('0' <= *s && *s <= '9') {
            if (dp == NULL)
                dp = s;
        } else
            dp = NULL;
    }

    if (dp == NULL)
        return -ENOENT;
    
    instno = strtoul(dp, NULL, 10);

    // Find the device record for the device.

    for (dev = devlist; dev != NULL; dev = dev->next) {
        if (strncmp(name, dev->name, dp - name) == 0 &&
            dev->name[dp - name] == '\0' &&
            dev->instno == instno)
        {
            switch (dev->type) {
            case DEV_SERIAL:
                return devfs_open_serial(dev->device_struct, uioptr);
            case DEV_STORAGE:
                return devfs_open_storage(dev->device_struct, uioptr);
            case DEV_VIDEO:
                return devfs_open_serial(dev->device_struct, uioptr);
            default:
                panic("Bad device type");
            }
        }
    }

    return -ENOENT;
}

static int devfs_open_serial(struct serial * ser, struct uio ** uioptr) {
    struct devfs_serial_uio *ser_uio = kcalloc(1, sizeof(struct devfs_serial_uio));
    ser_uio->base = **uioptr;
    ser_uio->ser = ser;
    return ser->intf->open(ser);
    // uio_addref(*uioptr); - fs_open should do this
}

void devfs_serial_close(struct uio * uio) {
    struct serial * ser = (struct serial *)(((void*)uio) + offsetof(struct devfs_serial_uio, ser));
    ser->intf->close(ser);
}

long devfs_serial_read(struct uio * uio, void * buf, unsigned long bufsz) {
    // ...
}

long devfs_serial_write(struct uio * uio, const void * buf, unsigned long buflen) {
    // ...
}

int devfs_serial_cntl(struct uio * uio, int op, void * arg) {
    // ...
}

void devfs_storage_close(struct uio * uio) {
    // ...
}

long devfs_storage_read(struct uio * uio, void * buf, unsigned long bufsz) {
    // ...
}

long devfs_storage_write(struct uio * uio, const void * buf, unsigned long buflen) {
    // ...
}

int devfs_storage_cntl(struct uio * uio, int op, void * arg) {
    // ...
}

void devfs_video_close(struct uio * uio) {
    // ...
}

long devfs_video_read(struct uio * uio, void * buf, unsigned long bufsz) {
    // ...
}

long devfs_video_write(struct uio * uio, const void * buf, unsigned long buflen) {
    // ...
}

int devfs_video_cntl(struct uio * uio, int op, void * arg) {
    // ...
}