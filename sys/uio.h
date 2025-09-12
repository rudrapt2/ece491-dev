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

// IOCTL DEFINITIONS
//

#define IOCTL_GETEND 0 // arg is unsigned long long *
#define IOCTL_SETEND 1 // arg is unsigned long long *
#define IOCTL_GETPOS 2 // arg is unsigned long long *
#define IOCTL_SETPOS 3 // arg is unsigned long long *

// Returns a pointer to a null uio object, which supports the following operations:
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