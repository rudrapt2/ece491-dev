/*! @file filesys.h
    @brief File system interface
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _FILESYS_H_
#define _FILESYS_H_

#include "cache.h"
#include "fsimpl.h"
#include "uio.h"

extern char fsmgr_initialized;

extern int fsmgr_init(void);
extern void fsmgr_flushall(void);

extern int open_file(const char * mpname, const char * flname, struct uio ** uioptr);
extern int create_file(const char * mpname, const char * flname);
extern int delete_file(const char * mpname, const char * flname);

extern int attach_filesystem(const char * name, struct filesystem * fs);

// Splits a string path into two strings: a mount point name and a file name.
// The existing string /path/ in modified and the /mpname/ and /flname/ are set
// ot point inside /path/.

extern int parse_path(char * path, char ** mpnameptr, char ** flnameptr);

extern int mount_devfs(const char * name); // device.c
extern int mount_tarfs(const char * name, struct cache * cache); // tarfs.c
extern int mount_nullfs(const char * name); // filesys.c

#endif // _FILESYS_H_
