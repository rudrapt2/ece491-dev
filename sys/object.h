// object.h - Kernel objects
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _OBJECT_H_
#define _OBJECT_H_

enum object_type {
    OBJ_NULL,
    OBJ_STREAM,
    OBJ_STORAGE
};

struct object;

enum object_type object_type(const struct object * obj);
void object_addref(struct object * obj);
void object_dropref(struct object * obj);

struct stream;

unsigned int stream_blksz(const struct stream * stm);
long stream_read(struct stream * stm, void * buf, long blkcnt);
long stream_write(struct stream * stm, const void * buf, long blkcnt);
int stream_ioctl(struct stream * stm, int op, void * arg);


static inline void stream_close(struct stream * stm) {
    object_dropref((struct object*)stm);
}

struct storage;

unsigned int storage_blksz(const struct storage * sto);
long storage_read(struct storage * sto, unsigned long long blkno, void * buf, long blkcnt);
long storage_write(struct storage * sto, unsigned long long blkno, const void * buf, long blkcnt);
int storage_ioctl(struct storage * sto, int op, void * arg);

static inline void storage_close(struct storage * sto) {
    object_dropref((struct object*)sto);
}

#endif // _OBJECT_H_

// 
// objimpl.h
//

struct object {
    enum object_type type;
    unsigned int refcnt;
    void (*reclaim)(struct object *);
};

struct object * object_init (
    struct object * obj,
    enum object_type * type,
    unsigned int refcnt,
    void (*reclaim)(struct object*)
);

struct stream_intf {
    long (*read)(struct stream * stm, void * buf, long blkcnt);
    long (*write)(struct stream * stm, const void * buf, long blkcnt);
    int (*ioctl)(struct stream * stm, int op, void * arg);
};

struct stream {
    struct object obj; // must be first
    struct stream_intf * intf;
    unsigned int blksz;
};

// 
// object.c
//


#include <stddef.h>
#include "assert.h"
#include "error.h"

enum object_type object_type(const struct object * obj) {
    return obj->type;
}

void object_addref(struct object * obj) {
    obj->refcnt += 1;
    assert (obj->refcnt != 0);
}

void object_dropref(struct object * obj) {
    assert (obj->refcnt != 0);
    obj->refcnt -= 1;

    if (obj->refcnt == 0 && obj->reclaim != NULL)
        obj->reclaim(obj);
}

unsigned int stream_blksz(const struct stream * stm) {
    return stm->blksz;
}

long stream_read(struct stream * stm, void * buf, long blkcnt) {
    assert (stm != NULL);

    if (stm->intf->read == NULL)
        return -ENOTSUP;
    
    if (blkcnt < 0)
        return -EINVAL;
    
    return stm->intf->read(stm, buf, blkcnt);
}

long stream_write(struct stream * stm, const void * buf, long blkcnt) {
    assert (stm != NULL);

    if (stm->intf->write == NULL)
        return -ENOTSUP;
    
    if (blkcnt < 0)
        return -EINVAL;
    
    return stm->intf->write(stm, buf, blkcnt);
}

int stream_ioctl(struct stream * stm, int op, void * arg) {
    assert (stm != NULL);

    if (stm->intf->ioctl == NULL)
        return -ENOTSUP;
    
    return stm->intf->ioctl(stm, op, arg);
}