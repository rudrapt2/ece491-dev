// ngfs.h - A FAT-like file system
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _NGFS_H_
#define _NGFS_H_

struct filesystem; // extern decl.
struct io; // extern decl.

extern int mount_ngfs(const char * mpname, struct io * bkgio);

#endif // _NGFS_H_