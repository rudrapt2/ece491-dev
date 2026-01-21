// device.h - Device manager
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//


#ifndef _DEVICE_H_
#define _DEVICE_H_

struct io; // extern decl.

extern char devmgr_initialized;
extern void devmgr_init(void);

#ifndef NO_FS
extern struct filesystem devfs;
#endif

typedef int (*device_openfn_t)(struct io ** ioptr, void * aux);

extern int register_device (
    const char * name,
    int instno,
    device_openfn_t openfn,
    void * ofaux
);

extern int open_device(const char * name, struct io ** ioptr);

#endif