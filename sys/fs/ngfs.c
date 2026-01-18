// ngfs.h - A FAT-like file system
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _NGFS_H_
#define _NGFS_H_

#include "ngfs.h"

#include "fsimpl.h"
#include "ioimpl.h"
#include "error.h"
#include "cache.h"
#include "heap.h"
#include "misc.h"

int mount_ngfs(const char * mpname, struct io * bkgio) {
    return -ENOTSUP;
}

#endif // _NGFS_H_