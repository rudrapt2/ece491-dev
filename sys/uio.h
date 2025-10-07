// uio.h - Uniform I/O interface
//

#ifndef _UIO_H_
#define _UIO_H_

#include <stddef.h> // for size_t

struct uio; // opaque decl.

/**
 * @brief Returns reference count of passed uio struct
 * @param uio Pointer to uio struct to check reference count on
 * @return Reference count of passed uio struct
 */
extern unsigned long uio_refcnt(const struct uio * uio);

/**
 * @brief Increments reference count of passed uio struct
 * @param uio Pointer to uio struct to increment reference count on
 * @return Incremented reference count
 */
extern int uio_addref(struct uio * uio);

/**
 * @brief Decrements reference count of passed uio struct, calling backing endpoint's _close_ if reference count is 0
 * @param uio Pointer to uio struct of backing endpoint to close
 * @return None
 */
extern void uio_close(struct uio * uio);

/**
 * @brief Calls backing endpoint's _read_
 * @param uio Pointer to uio struct of backing endpoint to read from
 * @param buf Buffer for backing endpoint to copy data into
 * @param bufsz Size of passed buffer in bytes
 * @return Number of bytes read from backing endpoint, error if backing endpoint doesn't support _read_ or bufsz < 0
 */
extern long uio_read(struct uio * uio, void * buf, unsigned long bufsz);

/**
 * @brief Calls backing endpoint's _write_
 * @param uio Pointer to uio struct of backing endpoint to write to
 * @param buf Buffer for backing endpoint to copy data from
 * @param buflen Number of bytes for backing endpoint to write from buf
 * @return Number of bytes successfully written to backing endpoint, error if backing endpoint doesn't support _write_ or buflen < 0
 */
extern long uio_write(struct uio * uio, const void * buf, unsigned long buflen);
extern int uio_cntl(struct uio * uio, int op, void * arg);
extern void create_pipe(struct uio ** wptr, struct uio ** rptr);

// FNCTL OP CONSTANTS
//

#define FCNTL_GETEND 0 // arg is unsigned long long *
#define FCNTL_SETEND 1 // arg is unsigned long long *
#define FCNTL_GETPOS 2 // arg is unsigned long long *
#define FCNTL_SETPOS 3 // arg is unsigned long long *

#define FCNTL_MMAP   4 // arg is void **

// See also device.h for device-specific FNCTL values

// The create_null_uio() function returns a pointer to a null_uio uio object,
// which supports the following operations:
//
//    close(): decrements reference count
//     read(): returns 0
//    write(): accepts all data
//     cntl(): return -ENOTSUP
//    
// There is a single system-wide null uio object. Calling nulluio() increments
// its reference count and returns a pointer to it.

extern struct uio * create_null_uio(void);

// The create_memory_uio() function creates a memory-backed uio object that
// supports read, write, and control operations on a block of memory.

extern struct uio *create_memory_uio(void *buf, size_t size);

#endif // _UIO_H_