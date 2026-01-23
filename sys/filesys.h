// filesys.h - File system manager
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _FILESYS_H_
#define _FILESYS_H_

#include "io.h"

struct filesystem; // opaque decl.

extern char fsmgr_initialized;
extern int fsmgr_init(void);

extern int mount_filesys(const char * mpname, struct filesystem * fs);
extern void flush_all_filesys(void);

extern int open_file(const char * mpname, const char * flname, struct io ** ioptr);
extern int create_file(const char * mpname, const char * flname);
extern int delete_file(const char * mpname, const char * flname);

extern void parse_path(char * path, char ** mpnameptr, char ** flnameptr);

extern struct filesystem nullfs;

#endif // _FILESYS_H_
