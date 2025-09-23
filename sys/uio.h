// uio.h - Uniform I/O interface
//

#ifndef _UIO_H_
#define _UIO_H_

struct uio; // opaque decl.

extern int uio_addref(struct uio * uio);
extern void uio_close(struct uio * uio);
extern long uio_read(struct uio * uio, void * buf, unsigned long bufsz);
extern long uio_write(struct uio * uio, const void * buf, unsigned long buflen);
extern int uio_cntl(struct uio * uio, int op, void * arg);

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

#endif // _UIO_H_