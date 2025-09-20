// filesys.h - File system interface
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _FILESYS_H_
#define _FILESYS_H_

#include "cache.h"
#include "uio.h"

extern char fsmgr_initialized;

extern int fsmgr_init(void);
extern void fsmgr_flushall(void);

extern int open_file(const char * name, struct uio ** uioptr);
extern int create_file(const char * name);
extern int delete_file(const char * name);

extern int mount_devfs(const char * name); // device.c
extern int mount_ktfs(const char * name, struct cache * cache); // fs/ktfs.c
extern int mount_tarfs(const char * name, struct cache * cache); // fs/tarfs.c
extern int mount_nullfs(const char * name); // fs.c

#endif // _FILESYS_H_