// fs.c - Default empty file system
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "filesys.h"
#include "fsimpl.h"
#include "error.h"
#include <stddef.h>

// INTERNAL TYPE DEFINITIONS
//

struct mountpoint {

    const char * name;
    struct filesystem * fs;
};

// INTERNAL FUNCTION PROTOTYPES
//

static int nullfs_open(struct filesystem * fs, const char * name, struct uio ** uioptr);
static void nullfs_flush(struct filesystem * fs);

// INTERNAL GLOBAL VARIABLES
//

static const struct filesystem nullfs = {
    .open = &nullfs_open,
    .flush = &nullfs_flush
};

// EXPORTED GLOBAL VARIABLES
//

char fsmgr_initialized = 0;

// EXTERNAL FUNCTION DEFINITIONS
//

int fsmgr_init(void) {
    fsmgr_initialized = 1;
    return 0;
}

void fsmgr_flushall(void) {
    // ...
}

int open_file(const char * name, struct uio ** uioptr) {
    return -ENOTSUP; // TODO
}

int create_file(const char * name) {
    // ...
    return -ENOTSUP;
}

int delete_file(const char * name) {
    // ...
    return -ENOTSUP;
}

int mount_nullfs(const char * name) {
    return mount_fs(name, &nullfs);
}

int mount_fs(const char * name, struct filesystem * fs) {
    // ...
    return -ENOTSUP;
}

// INTERNAL FUNCTION DEFINITIONS
//

int nullfs_open (
    struct filesystem * fs __attribute__ ((unused)),
    const char * name,
    struct uio ** uioptr)
{
    *uioptr = NULL;
    return -ENOENT;
}

void nullfs_flush(struct filesystem * fs __attribute__ ((unused))) {
    // nothing
}