// devimpl.h - Header for device implementations
//

#ifndef _DEVIMPL_H_
#define _DEVIMPL_H_

#include "device.h"

extern int register_device(const char * name, enum device_type type, void * device_struct);

// SERIAL DEVICES
//

// The /serial_intf/ struct defines the interface to the device: the block size
// and function pointers for operations on the device. Use the functions
// /serial_open/, /serial_close/, etc. defined below to operate on the device.

struct serial_intf {
    unsigned int blksz;
    int (*open)(struct serial * ser);
    void (*close)(struct serial * ser);
    int (*recv)(struct serial * ser, void * buf, unsigned int bufsz);
    int (*send)(struct serial * ser, const void * buf, unsigned int buflen);
    int (*cntl)(struct serial * ser, int op, void * arg);
};

// Each serial device is represented by a /serial/ struct, and all references to
// the device in the system are a pointer to this struct. The struct may be
// embedded in a larger struct, containing additional device-specific members.

struct serial {
    const struct serial_intf * intf;
};

static inline void serial_init (
struct serial * ser, const struct serial_intf * intf)
{
    ser->intf = intf;
}

// STORAGE DEVICES
//

struct storage_intf {
    unsigned int blksz;
    int (*open)(struct storage * sto);
    void (*close)(struct storage * sto);
    
    long (*fetch) (
        struct storage * sto,
        unsigned long long pos,
        void * buf,
        unsigned long bytecnt);
    
    long (*store) (
        struct storage * sto,
        unsigned long long pos,
        const void * buf,
        unsigned long bytecnt);
        
    int (*cntl)(struct storage * sto, int op, void * arg);
};

struct storage {
    const struct storage_intf * intf;
    unsigned long long capacity;
};

static inline void storage_init (
    struct storage * sto, const struct storage_intf * intf, unsigned long long cap)
{
    sto->intf = intf;
    sto->capacity = cap;
}

// VIDEO DEVICE
//

struct video_mode {
    unsigned int width, height;
    unsigned int horiz_stride;
    unsigned int vert_stride;
    unsigned char bytes_per_pixel;
    unsigned char rshift, rdepth;
    unsigned char gshift, gdepth;
    unsigned char bshift, bdepth;
};

struct video_intf {
    unsigned short modecnt;
    const struct video_mode * modes;
    int (*open)(struct video * vid, int mode, void ** fbufptr);
    void (*close)(struct video * vid);
    void (*flush)(struct video * vid);
    int (*cntl)(struct video * vid, int op, void * arg);
};

struct video {
    const struct video_intf * intf;
};

static inline void video_init (
    struct video * vid, const struct video_intf * intf)
{
    vid->intf = intf;
}

#endif // _DEVIMPL_H_