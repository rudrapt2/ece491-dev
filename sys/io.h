// io.h - Generic I/O objects
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _IO_H_
#define _IO_H_

#include <stddef.h> // size_t
#include <stdint.h> // uintptr_t

struct io; // opaque

// IO OBJECT OPERATIONS
//

extern unsigned int ioblksz(const struct io * io);
extern unsigned int iorefcnt(const struct io * io);
extern struct io * ioaddref(struct io * io);
extern void iodropref(struct io * io);

extern long ioread(struct io * io, void * buf, long bufsz);
extern long iowrite(struct io * io, const void * buf, long len);
extern long iofetch(struct io * io, unsigned long long pos, void * buf, long len);
extern long iostore(struct io * io, unsigned long long pos, const void * buf, long len);

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
    void * buf, size_t size,
    void(*reclfn)(void*,size_t));

#ifndef MP2
extern void create_iopipe(struct io ** wioptr, struct io ** rioptr);
#endif // MP2

#endif // _IO_H_