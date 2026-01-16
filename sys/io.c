// io.c - Generic I/O objects
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "ioimpl.h"
#include <stddef.h>
#include "error.h"
#include "heap.h"

// IO EXPORTED FUNCTION DEFINITIONS
//

struct io * ioinit (
    struct io * io,
    const struct iointf * intf,
    unsigned int blksz,
    unsigned int refcnt)
{
    io->intf = intf;
    io->blksz = blksz;
    io->refcnt = refcnt;
    return io;
}

unsigned int ioblksz(const struct io * io) {
    return io->blksz;
}

unsigned int iorefcnt(const struct io * io) {
    return io->refcnt;
}

unsigned int ioaddref(struct io * io) {
    io->refcnt += 1;

    if (io->refcnt == 0)
        panic("Too many io refs");
}

void iodropref(struct io * io) {
    assert (io->refcnt != 0);
    io->refcnt -= 1;

    if (io->refcnt == 0 && io->intf->reclaim != NULL)
        io->intf->reclaim(io);
}

long ioread(struct io * io, void * buf, long bufsz) {
    long result;

    assert (io != NULL);
    assert (buf != NULL || bufsz == 0);

    if (io->intf->read == NULL)
        return -ENOTSUP;
    
    if (bufsz < 0)
        return -EINVAL;
    
    if (bufsz != 0 && bufsz < io->blksz)
        return -EINVAL;
    
    result = io->intf->read(io, buf, bufsz);
    assert (result <= bufsz);

    if (0 < result)
        return result * io->blksz;
    else
        return result;
}

long iowrite(struct io * io, const void * buf, long len) {
    long result;

    assert (io != NULL);
    assert (buf != NULL || len == 0);

    if (io->intf->write == NULL)
        return -ENOTSUP;
    
    if (len < 0)
        return -EINVAL;
    
    if (len != 0 && len < io->blksz)
        return -EINVAL;
    
    return io->intf->read(io, buf, len);
}

long iofetch(struct io * io, unsigned long long pos, void * buf, long len) {
    long result;

    assert (io != NULL);
    assert (buf != NULL || len == 0);

    if (io->intf->fetch == NULL)
        return -ENOTSUP;
    
    if (len < 0)
        return -EINVAL;
    
    if (len != 0 && len < io->blksz)
        return -EINVAL;
    
    if (pos % io->blksz != 0 || len % io->blksz != 0)
        return -EINVAL;
    
    return io->intf->fetch(io, pos, buf, len);
}

long iostore(struct io * io, unsigned long long pos, const void * buf, long len) {
    long result;

    assert (io != NULL);
    assert (buf != NULL || len == 0);

    if (io->intf->fetch == NULL)
        return -ENOTSUP;
    
    if (len < 0)
        return -EINVAL;
    
    if (len != 0 && len < io->blksz)
        return -EINVAL;
    
    if (pos % io->blksz != 0 || len % io->blksz != 0)
        return -EINVAL;
    
    return io->intf->fetch(io, pos, buf, len);
}

int ioctl(struct io * io, int op, void * arg) {
    assert (io != NULL);

    if (io->intf->ioctl != NULL)
        return io->intf->ioctl(io, op, arg);
    else
        return -ENOTSUP;
}

int ioctl_u(struct io * io, int u_op, uintptr_t u_arg) {
    assert (io != NULL);

    if (io->intf->ioctl_u != NULL)
        return io->intf->ioctl_u(io, u_op, u_arg);
    else
        return -ENOTSUP;
}

// SEEKIO EXPORTED FUNCTION DEFINITIONS
//

struct io * seekio_init (
    struct seekio * sio,
    struct iointf * intf,
    unsigned long long end,
    unsigned int blksz,
    unsigned int refcnt)
{
    sio->pos = 0;
    sio->end = end;
    return ioinit(&sio->base, intf, blksz, refcnt);
}

long seekio_read(struct io * io, void * buf, long bufsz) {
    struct seekio * const sio = (struct seekio*)io;
    long reqlen, retlen;

    assert (sio->pos % io->blksz == 0);
    assert (sio->end % io->blksz == 0);

    if (io->intf->fetch == NULL)
        return -ENOTSUP;
    
    if (sio->pos == sio->end)
        return 0;
    
    if (sio->end - sio->pos < bufsz)
        reqlen = sio->end - sio->pos;
    else
        reqlen = ROUND_DOWN(bufsz, sio->base.blksz);
    
    retlen = io->intf->fetch(&sio->base, sio->pos, buf, reqlen);

    if (retlen <= 0)
        return retlen;
    
    assert (retlen == reqlen);
    sio->pos += retlen;
    return retlen;
}

long seekio_write(struct io * io, const void * buf, long len) {
    struct seekio * const sio = (struct seekio*)io;
    long reqlen, retlen;

    if (io->intf->store == NULL)
        return -ENOTSUP;
    
    if (sio->pos == sio->end)
        return 0;
    
    if (sio->end - sio->pos < len)
        reqlen = sio->end - sio->pos;
    else
        reqlen = ROUND_DOWN(len, sio->base.blksz);
    
    retlen = io->intf->store(&sio->base, sio->pos, buf, reqlen);

    if (retlen <= 0)
        return retlen;
    
    assert (retlen == reqlen);
    sio->pos += retlen;
    return retlen;
}

int seekio_ioctl(struct io * io, int op, void * arg) {
    struct seekio * const sio = (struct seekio*)io;
    unsigned long long * const ullarg = arg;

    switch (op) {
    case IOC_GETEND:
        *ullarg = sio->end;
        return 0;
    
    case IOC_SETEND:
        if (*ullarg % io->blksz != 0)
            return -EINVAL;
        sio->end = *ullarg;
        return 0;
    
    case IOC_GETPOS:
        *ullarg = sio->pos;
        return 0;

    case IOC_SETPOS:
        if (*ullarg % io->blksz != 0)
            return -EINVAL;
        if (sio->end < *ullarg)
            return -EINVAL;
        sio->pos = *ullarg;
        return 0;
    
    default:
        return -ENOTSUP;
    }
}


// NULLIO INTERNAL FUNCTION DECLARATIONS
//

static long nullio_read(struct io * io, void * buf, long bufsz);
static long nullio_write(struct io * io, const void * buf, long len);
static long nullio_fetch(struct io * io, unsigned long long pos, void * buf, long len);
static long nullio_store(struct io * io, unsigned long long pos, const void * buf, long len);


// NULLIO INTERNAL CONSTANT DEFINITIONS
//

static const struct iointf nullio_intf = {
    .implname = "nullio",
    .read = &nullio_read,
    .write = &nullio_write,
    .fetch = &nullio_fetch,
    .store = &nullio_store
};

static const struct io nullio = {
    .intf = &nullio_intf,
    .blksz = 1,
    .refcnt = 0
};

// NULLIO INTERNAL FUNCTION DEFINITIONS
//

struct io * create_nullio(void) {
    return (struct io*)&nullio;
}

long nullio_read(struct io * io, void * buf, long bufsz) {
    (void)io;
    (void)buf;
    (void)bufsz;
    return 0;
}

long nullio_write(struct io * io, const void * buf, long len) {
    (void)io;
    (void)buf;
    (void)len;
    return 0;
}

long nullio_fetch(struct io * io, unsigned long long pos, void * buf, long len) {
    assert (io == &nullio);
    (void)buf;

    if (pos != 0 || len != 0)
        return -EINVAL;
    
    return 0;
}

long nullio_store(struct io * io, unsigned long long pos, const void * buf, long len) {
    assert (io == &nullio);
    (void)buf;

    if (pos != 0 || len != 0)
        return -EINVAL;
    
    return 0;
}

// MEMIO INTERNAL TYPE DEFINITIONS
//

struct memio {
    struct seekio base;
    void * buf;
};

// MEMIO INTERNAL FUNCTION DECLARATIONS
//

static void memio_reclaim(struct io * io);
static long memio_fetch(struct io * io, unsigned long long pos, void * buf, long len);
static long memio_store(struct io * io, unsigned long long pos, const void * buf, long len);
static int memio_ioctl(struct io * io, int op, void * arg);
static int memio_ioctl_u(struct io * io, int u_op, uintptr_t u_arg);

// MEMIO INTERNAL CONSTANT DEFINITIONS
//

static const struct iointf memio_intf = {
    .implname = "memio",
    .reclaim = &memio_reclaim,
    .read = &seekio_read,
    .write = &seekio_write,
    .fetch = &memio_fetch,
    .store = &memio_store,
    .ioctl = &memio_ioctl,
    .ioctl_u = &memio_ioctl_u
};

// MEMIO EXPORTED FUNCTION DEFINITIONS
//

extern struct io * create_memio (
    void * buf,
    size_t buflen,
    unsigned int blksz,
    int rdonly)
{
    struct memio * mio;

    mio = kcalloc(1, sizeof(*mio));

    return ioinit(&mio->base, &memio_intf, /* blksz */ 1, /* refcnt */ 0);
}

// MEMIO INTERNAL FUNCTION DEFINITIONS
//

void memio_reclaim(struct io * io) {
    struct memio * const mio = (struct memio*)io;
}

long memio_fetch(struct io * io, unsigned long long pos, void * buf, long len) {
    struct memio * const mio = (struct memio*)io;
}

long memio_store(struct io * io, unsigned long long pos, const void * buf, long len) {
    struct memio * const mio = (struct memio*)io;
}

int memio_ioctl(struct io * io, int op, void * arg) {
    struct memio * const mio = (struct memio*)io;
}

int memio_ioctl_u(struct io * io, int u_op, uintptr_t u_arg) {
    struct memio * const mio = (struct memio*)io;
}
