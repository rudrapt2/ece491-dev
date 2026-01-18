/*! @file fsimpl.h
    @brief File system implementers' interface
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _FSIMPL_H_
#define _FSIMPL_H_

#include "io.h"

struct filesystem {
    const char * implname;
    int (*openfile)(struct filesystem * fs, const char * flname, struct io ** ioptr);
    int (*createfile)(struct filesystem * fs, const char * flname);
    int (*deletefile)(struct filesystem * fs, const char * flname);
    void (*flush)(struct filesystem * fs);
};

#endif  // _FSIMPL_H_
