// uioimpl.h - Header for uio implementations
// 

#ifndef _UIOIMPL_H_
#define _UIOIMPL_H_


struct uio; // forward decl.

struct uio_intf {
    void (*close)(struct uio * uio);
    long (*read)(struct uio * uio, void * buf, unsigned long bufsz);
    long (*write)(struct uio * uio, const void * buf, unsigned long buflen);
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

#endif