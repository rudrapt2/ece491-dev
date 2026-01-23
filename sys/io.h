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

// The /io/ structure represents an objects capable of input and output
// operations, which includes files and devices. The structure definition is
// opaque to _users_ of I/O objects. Parts of the system that _implement_ an I/O
// object should include ioimpl.h, which contains the definition of the /io/
// structure.


// IO OBJECT OPERATIONS
//

extern unsigned int ioblksz(const struct io * io);

// Returns the block size of the I/O object. All operations must in multiples of
// this size.
//
// On entry ioblksz() assumes:
// - /io/ points to a valid I/O object.
//
// On return ioblksz() guarantees:
// - The returned block size is greater than or equal to 1.
//
// See also: ioread(), iowrite(), iofill(), iofetch(), iostore().


extern unsigned int iorefcnt(const struct io * io);

// Returns the number of independent references to the I/O object. A reference
// is _independent_ if it has a distinct lifetime from any other reference to
// the same object. An object may have a reference count of 0, however, the only
// permitted operations on such an object are: ioblksz(), iorefcnt(), and
// ioaddref().
//
// On entry iorefcnt() assumes:
// - /io/ points to a valid I/O object.
//
// On return iorefcnt() gurantees:
// - The return value is the number of independent references to the object.
//
// See also: ioaddref(), iodropref().


extern struct io * ioaddref(struct io * io);
extern void iodropref(struct io * io);

extern long ioread(struct io * io, void * buf, long bufsz);
extern long iofill(struct io * io, void * buf, long bufsz);
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