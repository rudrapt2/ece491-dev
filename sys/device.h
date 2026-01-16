/*! @file device.h
    @brief Interface to device system
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _DEVICE_H_
#define _DEVICE_H_

extern char devmgr_initialized;
extern void devmgr_init(void);

extern int mount_devfs(const char * mpname);

typedef struct io * (*devopenfn_t)(int instno, void * aux);
extern int register_device(const char * name, devopenfn_t openfn, void * ofaux);

extern int open_device(const char * name, int instno);

#endif