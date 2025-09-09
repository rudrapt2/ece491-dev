// uio.h - Uniform I/O interface
//

#ifndef _UIO_H_
#define _UIO_H_

struct uio; // forward decl.

struct uio_intf {
    void (*close)(struct uio * uio);
    int (*read)(struct uio * uio, void * buf, unsigned long bytecnt);
    int (*write)(struct uio * uio, const void * buf, unsigned long bytecnt);
    int (*cntl)(struct uio * uio, int op, void * arg);
};

struct uio {
    const struct uio_intf * intf;
    unsigned long refcnt;
};

// Initialize a uio object with a reference count of zero.

static inline struct uio * uio_init0 (
    struct uio * uio, const struct uio_intf * intf)
{
    uio->intf = intf;
    uio->refcnt = 0;
    return uio;
}

// Initialize a uio object with a reference count of one.

static inline struct uio * uio_init1 (
    struct uio * uio, const struct uio_intf * intf)
{
    uio->intf = intf;
    uio->refcnt = 1;
    return uio;
}

#endif // _UIO_H_