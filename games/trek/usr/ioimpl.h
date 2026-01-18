/*! @file ioimpl.h
    @brief Header for implementors of struct io
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA

*/

#ifndef _IOIMPL_H_
#define _IOIMPL_H_

// #include "io.h"

// EXPORTED TYPE DEFINITIONS
//

/**
 * @brief I/O abstraction used for devices, files, pipes, etc.
 */
struct io {
    const struct iointf * intf; ///< I/O interface for backing endpoint 
    unsigned long refcnt; ///< Number of active references to this I/O endpoint 
};

/**
 * @brief Table of RWOC-type functions that backing endpoints may support. These functions allow
 * modification of a specific device, file, pipe, or backing endpoint via the I/O abstraction.
 * Backing endpoints may support any combination of the provided operations. Return values for 
 * each function depend on backing endpoint.
 */
struct iointf {
    /**
     * @brief Closes I/O endpoint
     * @param io Pointer to I/O endpoint
     */
    void (*close) (
        struct io * io
    );

    /**
     * @brief Control operation on I/O endpoint
     * @param io An I/O endpoint to control/interrogate
     * @param cmd Command (IOCTL_* defined in io.h)
     * @param arg Argument for operation
     */
    int (*cntl) (
        struct io * io,
        int cmd,
        void * arg
    );

    /**
     * @brief Reads from sequential access I/O endpoint
     * @param io A sequential access I/O endpoint
     * @param buf buffer read into
     * @param bufsz buffer size in bytes
     */
    long (*read) (
        struct io * io,
        void * buf,
        long bufsz
    );

    /**
     * @brief Writes to sequential access I/O endpoint
     * @param io A sequential access I/O endpoint
     * @param buf Buffer to read from
     * @param len Number of bytes to read
     */
    long (*write) (
        struct io * io,
        const void * buf,
        long len
    );

    /**
     * @brief Reads from random access I/O endpoint
     * @param io A random access I/O endpoint
     * @param pos Position to read from
     * @param buf Buffer to read into
     * @param len Number of bytes to read
     */
    long (*readat) (
        struct io * io,
        unsigned long long pos,
        void * buf,
        long len
    );

    /**
     * @brief Writes to random access I/O endpoint
     * @param io A random access I/O endpoint
     * @param pos Position to write to
     * @param buf Buffer to write from
     * @param len Number of bytes to write
     */
    long (*writeat) (
        struct io * io,
        unsigned long long pos,
        const void * buf,
        long len
    );
};

// EXPORTED FUNCTION DECLARATIONS
//

// The ioinit0() and ioinit1() functions initialize an I/O endpoint struct
// allocated by the caller. They sets the _intf_ member of the I/O endpoint to
// the _intf_ argument. The ioinit0() function initializes the reference count
// of the I/O endpoint to 0, while the ioinit1() function initializes it to 1.
// The function always succeeds and returns its first argument.

// extern struct io * ioinit0(struct io * io, const struct iointf * intf);
// extern struct io * ioinit1(struct io * io, const struct iointf * intf);

#endif // _IOIMPL_H_