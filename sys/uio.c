// uio.c - Uniform I/O interface
//

#include "uio.h"
#include "uioimpl.h"
#include "error.h"
#include "misc.h"

#include <stddef.h> // for NULL

static void nulluio_close(struct uio * uio);

static long nulluio_read (
    struct uio * uio, void * buf, unsigned long bufsz);

static long nulluio_write (
    struct uio * uio, const void * buf, unsigned long buflen);

// INTERNAL GLOBAL VARIABLES AND CONSTANTS
//

void uio_close(struct uio * uio) {
    if (uio->intf->close != NULL)
        uio->intf->close(uio);
}

long uio_read(struct uio * uio, void * buf, unsigned long bufsz) {
    if (uio->intf->read != NULL) {
        if (0 <= (long)bufsz)
            return uio->intf->read(uio, buf, bufsz);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

long uio_write(struct uio * uio, const void * buf, unsigned long buflen) {
    if (uio->intf->write != NULL) {
        if (0 <= (long)buflen)
            return uio->intf->write(uio, buf, buflen);
        else
            return -EINVAL;
    } else
        return -ENOTSUP;
}

int uio_cntl(struct uio * uio, int op, void * arg) {
    if (uio->intf->cntl != NULL)
        return uio->intf->cntl(uio, op, arg);
    else
        return -ENOTSUP;
}

int uio_addref(struct uio * uio) {
    return ++uio->refcnt;
}

struct uio * create_null_uio(void) {
    static const struct uio_intf nulluio_intf = {
        .close = &nulluio_close,
        .read = &nulluio_read,
        .write = &nulluio_write
    };

    static struct uio nulluio = {
        .intf = &nulluio_intf,
        .refcnt = 0
    };

    return &nulluio;
}

static void nulluio_close(struct uio * uio) {
    // ...
}

static long nulluio_read (
    struct uio * uio, void * buf, unsigned long bufsz)
{
    // ...
    return -ENOTSUP;
}

static long nulluio_write (
    struct uio * uio, const void * buf, unsigned long buflen)
{
    // ...
    return -ENOTSUP;
}