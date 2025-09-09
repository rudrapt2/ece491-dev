// fs.c - Default empty file system
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "fs.h"
#include "error.h"

int fsmount(struct io * io)
    __attribute__ ((alias("nullfs_mount"), weak));

int fsopen(const char * name, struct io ** ioptr)
    __attribute__ ((alias("nullfs_open"), weak));

int fscreate(const char * name)
    __attribute__ ((alias("nullfs_create"), weak));

int fsdelete(const char * name)
    __attribute__ ((alias("nullfs_delete"), weak));

int fsflush(void)
    __attribute__ ((alias("nullfs_flush"), weak));

int nullfs_mount(struct io * io) {
    return 0;
}

int nullfs_open(const char * name, struct io ** ioptr) {
    return -ENOENT;
}

int nullfs_create(const char * name) {
    return -ENOTSUP;
}

int nullfs_delete(const char * name) {
    return -ENOTSUP;
}

int nullfs_flush(void) {
    return 0;
}