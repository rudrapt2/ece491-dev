// ioimpl.h - Generic I/O objects implementer header
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _IOIMPL_H_
#define _IOIMPL_H_

#include "io.h"

// IO and IOINTF
//

struct iointf {
    const char * implname;
    void (*reclaim)(struct io * io);
    long (*read)(struct io * io, void * buf, long blkcap);
    long (*write)(struct io * io, const void * buf, long blkcnt);
    long (*store)(struct io * io, unsigned long long blkpos, void * buf, long blkcnt);
    long (*fetch)(struct io * io, unsigned long long blkpos, void * buf, long blkcnt);
    int (*ioctl)(struct io * io, int op, void * arg);
    int (*ioctl_u)(struct io * io, int u_op, void * u_arg);
};

struct io {
    struct iointf * intf;
    unsigned int blksz;
    unsigned int refcnt;
};

#define IO(p) ((struct io*)(p));

extern struct io * ioinit (
    struct io * io,
    const struct iointf * intf,
    unsigned int blksz,
    unsigned int refcnt
);


// SEEKIO
//

struct seekio {
    struct io base;
    unsigned long long pos;
    unsigned long long end;
};

extern struct io * seekio_init (
    struct seekio * sio,
    struct iointf * intf,
    unsigned long long end,
    unsigned int blksz,
    unsigned int refcnt
);

extern long seekio_read(struct io * io, void * buf, long blkcap);
extern long seekio_write(struct io * io, const void * buf, long blkcnt);
extern int seekio_ioctl(struct io * io, int op, void * arg);

#endif // _IOIMPL_H_