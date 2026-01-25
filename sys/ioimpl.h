// ioimpl.h - Generic I/O objects implementer header
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

// This header file is intended for parts of the system that _implement_ I/O
// objects. See io.h for the interface for _users_ of I/O objects.

#ifndef _IOIMPL_H_
#define _IOIMPL_H_

#include "io.h"

// IO and IOINTF
//

// An I/O object is a system object that supports a set of operations, most of
// which are concerned with input and output. The /io/ structure, defined below,
// represents the common elements of all I/O objects. Functions that operate on
// I/O objects pass a pointer to this structure. By including an /io/ structure
// inside a larger structure used to implement an I/O object, an implementation
// can recover a pointer to the larger structure. If the /io/ object is the
// first element of the larger structure, recovering a pointer to the enclosing
// structure from a (struct io*) entails only casting:
//
//     struct myio {
//         struct io base;
//         // ...
//     };
//
//     long myio_read(struct io * io, void * buf, long bufsz) {
//         struct someio * myio = (struct myio*)io;
//         // ...
//     }

//struct iointf; // forward decl.

struct io {
    const struct iointf * intf;
    unsigned int blksz;
    unsigned int refcnt;
};

// The /io/

struct iointf {
    const char * implname;
    void (*reclaim)(struct io * io);
    long (*read)(struct io * io, void * buf, long blkcap);
    long (*write)(struct io * io, const void * buf, long blkcnt);
    long (*store)(struct io * io, unsigned long long blkpos, const void * buf, long blkcnt);
    long (*fetch)(struct io * io, unsigned long long blkpos, void * buf, long blkcnt);
    int (*ioctl)(struct io * io, int op, void * arg);
    int (*ioctl_u)(struct io * io, int op, uintptr_t arg_uma);
};

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
    const struct iointf * intf,
    unsigned long long endpos,
    unsigned int blksz,
    unsigned int refcnt
);

extern long seekio_read(struct io * io, void * buf, long blkcap);
extern long seekio_write(struct io * io, const void * buf, long blkcnt);
extern int seekio_ioctl(struct io * io, int op, void * arg);

#endif // _IOIMPL_H_