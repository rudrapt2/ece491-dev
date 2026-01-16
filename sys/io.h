// io.h - Generic I/O objects
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _IO_H_
#define _IO_H_

#include <stdint.h>

struct io; // opaque

// IO OBJECT OPERATIONS
//

extern unsigned int ioblksz(const struct io * io);
extern unsigned int iorefcnt(const struct io * io);
extern unsigned int ioaddref(struct io * io);
extern void iodropref(struct io * io);

extern long read(struct io * io, void * buf, long bufsz);
extern long write(struct io * io, const void * buf, long len);
extern long fetch(struct io * io, unsigned long long pos, void * buf, long len);
extern long store(struct io * io, unsigned long long pos, const void * buf, long len);

#define IOC_GETEND 1
#define IOC_SETEND 2
#define IOC_GETPOS 3
#define IOC_SETPOS 4

int ioctl(struct io * io, int op, void * arg);

int ioctl_u(struct io * io, int u_op, uintptr_t u_arg);

// SPECIAL IO OBJECTS
//

extern struct io * create_nullio(void);

extern struct io * create_memio (
    void * buf,
    unsigned int size,
    unsigned int blksz,
    int rdonly);

#endif // _IO_H_