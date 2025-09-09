// fs.h - File system interface
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _FS_H_
#define _FS_H_

#include "uio.h"

extern int fsmgr_init(void);

extern int open_file(const char * name, struct uio * uio);
extern int creat_file(const char * name);
extern int unlink_file(const char * name);

extern int mount_devfs(const char * name); // device.c
extern int mount_ktfs(const char * name, struct cache * cache);
extern int mount_tarfs(const char * name, struct cache * cache);
extern int mount_nullfs(const char name); // fs.c

#endif // _FS_H_