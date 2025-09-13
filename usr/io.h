// io.h - Abstract io interface
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

/*! @file io.h
    @brief Unified I/O object
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA
*/

#ifndef _IO_H_
#define _IO_H_

#include "error.h"
#include "string.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

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

/**
 * @brief An io_term object is a wrapper around a "raw" I/O object. It provides newline conversion and interactive line-editing for string input.
 */
struct io_term {
    struct io io; ///<I/O abstraction
    struct io * rawio; ///< "raw" I/O object
    int8_t cr_out; ///< Output CRLF normalization
    int8_t cr_in; ///< Input CRLF normalization
};

/**
 * @brief Get Block size command number
 */
#define IOCTL_GETBLKSZ  0

/**
 * @brief Get end command number
 */
#define IOCTL_GETEND    2

/**
 * @brief Get set end command number
 */
#define IOCTL_SETEND    3

/**
 * @brief Get position command number
 */
#define IOCTL_GETPOS    4

/**
 * @brief Set position command number
 */
#define IOCTL_SETPOS    5

// refcount functions
/**
 * @brief Returns reference count of passed I/O struct
 * @param io Pointer to I/O struct to check reference count on
 * @return Reference count of passed I/O struct
 */
unsigned long iorefcnt(const struct io * io);

/**
 * @brief Increments reference count of passed I/O struct
 * @details To ensure proper reference counting, we recommend
 * using the ioaddref function at the point where the reference is created by
 * assignment to a persistent variable, e.g. io1 = ioaddref(io2);
 * @param io Pointer to I/O struct to increment reference count on
 * @return I/O struct with incremented pointer (same struct that was passed in)
 */
struct io * ioaddref(struct io * io);

// base iopos functions
/**
 * @brief Decrements reference count of passed I/O struct, calling backing endpoint's _close_ if reference count is 0
 * @param io Pointer to I/O struct of backing endpoint to close
 * @return None
 */
void ioclose(struct io * io);

/**
 * @brief Calls backing endpoint's _read_
 * @param io Pointer to I/O struct of backing endpoint to read from
 * @param buf Buffer for backing endpoint to copy data into
 * @param bufsz Size of passed buffer in bytes
 * @return Number of bytes read from backing endpoint, error if backing endpoint doesn't support _read_ or bufsz < 0
 */
long ioread (struct io * io, void * buf, long bufsz);

/**
 * @brief Calls backing endpoint's _write_ until len bytes are written or error is returned
 * @param io Pointer to I/O struct of backing endpoint to write to
 * @param buf Buffer for backing endpoint to copy data from
 * @param len Number of bytes for backing endpoint to write from buf
 * @return Number of bytes successfully written to backing endpoint, error if backing endpoint doesn't support _write_ or len < 0
 */
long iowrite (struct io * io, const void * buf, long len);

/**
 * @brief Calls backing endpoint's _readat_
 * @param io Pointer to I/O struct of backing endpoint to read from
 * @param pos Position to read from
 * @param buf Buffer for backing endpoint to copy data into
 * @param bufsz Size of passed buffer in bytes
 * @return Number of bytes successfully read from backing endpoint, error if backing endpoint doesn't support _readat_ or bufsz < 0
 */
long ioreadat (struct io * io, unsigned long long pos, void * buf, long bufsz);

/**
 * @brief Calls backing endpoint's _writeat_
 * @param io Pointer to I/O struct of backing endpoint to write to
 * @param pos Position to write to
 * @param buf Buffer for backing endpoint to copy data from
 * @param len Number of bytes for backing endpoint to write from buf
 * @return Number of bytes successfully written to backing endpoint, error if backing endpoint doesn't support _writeat_ or len < 0
 */
long iowriteat (struct io * io, unsigned long long pos, const void * buf, long len);

/**
 * @brief Calls backing endpoint's _ioctl_
 * @param io Pointer to I/O struct of backing endpoint to perform ioctl operation on
 * @param cmd Operation to perform
 * @param arg Additional argument for operation (if needed)
 * @return Output of _ioctl_ if successfully called, 1 if backing endpoint doesn't support _ioctl_ and command was IOCTL_GETBLKSZ, error otherwise 
 */
int ioctl (struct io * io, int cmd, void * arg);

// put, get, print
/**
 * @brief Writes a character to an I/O abstraction
 * @param io The I/O abstraction to interact with
 * @param c The character to write
 * @return The character writen, or negative error code
 */
static inline int ioputc(struct io * io, char c);

/**
 * @brief Reads a character to an I/O abstraction
 * @param io The I/O abstraction to interact with
 * @return The character read, or negative error code
 */
static inline int iogetc(struct io * io);

/**
 * @brief Writes a string of characters to an I/O abstraction
 * @param io The I/O abstraction to interact with
 * @param s A pointer to the string to be written
 * @return The number of character's written, or negative error code
 */
int ioputs(struct io * io, const char * s);

/**
 * @brief This function can take a variable amount of arguments. It formats and prints a string to an I/O abstraction
 * @param io The I/O abstraction to interact with
 * @param fmt A format specifier that specifies formatting and printing guidelines
 * @return The number of character's written, or negative error code
 */
long ioprintf(struct io * io, const char * fmt, ...);

/**
 * @brief Helper for ioprintf that calls vgprintf which writes the properly formated string.
 * @param io The I/O abstraction to interact with
 * @param fmt A format specifier that specifies formatting and printing guidelines
 * @param ap Optional additional paramaters for formating
 * @return The number of character's written, or negative error code
 */
long iovprintf(struct io * io, const char * fmt, va_list ap);

// I/O term provides three features:
//
//     1. Input CRLF normalization. Any of the following character sequences in
//        the input are converted into a single \n:
//
//            (a) \r\n,
//            (b) \r not followed by \n,
//            (c) \n not preceeded by \r.
//
//     2. Output CRLF normalization. Any \n not preceeded by \r, or \r not
//        followed by \n, is written as \r\n. Sequence \r\n is written as \r\n.
//
//     3. Line editing. The ioterm_getsn function provides line editing of the
//        input.
//
// Input CRLF normalization works by maintaining one bit of state: cr_in.
// Initially cr_in = 0. When a character ch is read from rawio:
// 
// if cr_in = 0 and ch == '\r': return '\n', cr_in <- 1;
// if cr_in = 0 and ch != '\r': return ch;
// if cr_in = 1 and ch == '\r': return \n;
// if cr_in = 1 and ch == '\n': skip, cr_in <- 0;
// if cr_in = 1 and ch != '\r' and ch != '\n': return ch, cr_in <- 0.
//
// Ouput CRLF normalization works by maintaining one bit of state: cr_out.
// Initially, cr_out = 0. When a character ch is written to I/O term:
//
// if cr_out = 0 and ch == '\r': output \r\n to rawio, cr_out <- 1;
// if cr_out = 0 and ch == '\n': output \r\n to rawio;
// if cr_out = 0 and ch != '\r' and ch != '\n': output ch to rawio;
// if cr_out = 1 and ch == '\r': output \r\n to rawio;
// if cr_out = 1 and ch == '\n': no ouput, cr_out <- 0;
// if cr_out = 1 and ch != '\r' and ch != '\n': output ch, cr_out <- 0.


// An io_term object is a wrapper around a "raw" I/O object. It provides newline
// conversion and interactive line-editing for string input.
//
// ioterm_init initializes an io_term object for use with an underlying raw I/O
// object. The /iot/ argument is a pointer to an io_term struct to initialize
// and /rawio/ is a pointer to an io_intf that provides backing I/O.
/**
 * @brief Initializes an io_term object for use with an underlying raw I/O object.
 * @param iot A pointer to an io_term struct to initialize
 * @param rawio A pointer to an io_intf that provides backing I/O
 * @return A pointer to the io_term's I/O abstraction
 */
struct io * ioterm_init(struct io_term * iot, struct io * rawio);


// The ioterm_getsn function reads a line of input of up to /n/ characters from
// a terminal I/O object. The function supports line editing (delete and
// backspace) and limits the input to /n/ characters.
/**
 * @brief The ioterm_getsn function reads a line of input of up to /n/ characters from a terminal I/O object. The function supports line editing (delete and backspace) and limits the input to /n/ characters.
 * @param iot A pointer to an io_term struct
 * @param buf A pointer to the buffer that will be populated with read input
 * @param n Maximum number of possible character's to be inputted
 * @return Null
 * @
 */
char * ioterm_getsn(struct io_term * iot, char * buf, size_t n);

// definitions for putc and getc
static inline int ioputc(struct io * io, char c) {
    long wlen;

    wlen = iowrite(io, &c, 1);

    if (wlen < 0)
        return wlen;
    else if (wlen == 0)
        return -EIO;
    else
        return (unsigned char)c;
}

static inline int iogetc(struct io * io) {
    long rlen;
    char c;

    rlen = ioread(io, &c, 1);
    
    if (rlen < 0)
        return rlen;
    else if (rlen == 0)
        return -EIO;
    else
        return (unsigned char)c;
}

#endif // _IO_H_