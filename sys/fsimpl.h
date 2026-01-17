/*! @file fsimpl.h
    @brief File system implementers' interface
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _FSIMPL_H_
#define _FSIMPL_H_

#include "io.h"

struct filesystem {
    int (*open_listing)(struct filesystem * fs, struct io ** ioptr);
    int (*open_file)(struct filesystem * fs, const char * name, struct io ** ioptr);
    int (*create_file)(struct filesystem * fs, const char * name);
    int (*delete_file)(struct filesystem * fs, const char * name);
    void (*flush)(struct filesystem * fs);
};

extern int attach_filesystem(const char * mpname, struct filesystem * fs);

#endif  // _FSIMPL_H_
