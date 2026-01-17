// device.h - Device manager
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//


#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "io.h"

extern char devmgr_initialized;
extern void devmgr_init(void);

extern struct filesystem devfs;

typedef int (*device_openfn_t)(int instno, struct io ** ioptr, void * aux);

extern int register_device (
    const char * name,
    int unique,
    device_openfn_t openfn,
    void * ofaux
);

extern int open_device(const char * name, struct io ** ioptr);

#endif