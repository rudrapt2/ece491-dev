// uio.c - Uniform I/O interface
//

#include "uio.h"
#include "uioimpl.h"
#include "error.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "heap.h"
#include "misc.h"

#include <stddef.h> // for NULL

#define PIPE_BUFSZ PAGE_SIZE

/**
 * @brief Unidirectional pipe. Consists of a reader I/O interface which may only read from buffer,
 * and a writer I/O interface which may only write to buffer
 */
struct pipe {
    struct condition updated; ///< Wait condition for writer/reader to use if no data is available/buffer is full
    struct uio wio; ///< I/O struct of writer
    struct uio rio; ///< I/O struct of reader
    void * buf; ///< Circular buffer where data is written/read
    unsigned int hpos; ///< where next byte will be read
    unsigned int tpos; ///< where next byte will be written
};

static void pipe_writer_close(struct uio * uio);
static void pipe_reader_close(struct uio * uio);
static long pipe_read(struct uio * uio, void * buf, unsigned long bufsz);
static long pipe_write(struct uio * uio, const void * buf, unsigned long len);

static void nulluio_close(struct uio * uio);

static long nulluio_read (
    struct uio * uio, void * buf, unsigned long bufsz);

static long nulluio_write (
    struct uio * uio, const void * buf, unsigned long buflen);

// INTERNAL GLOBAL VARIABLES AND CONSTANTS
//

void uio_close(struct uio * uio) {
    if (uio->refcnt == 0 && uio->intf->close != NULL)
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

unsigned long uio_refcnt(const struct uio * uio) {
    assert (uio != NULL);
    return uio->refcnt;
}

int uio_addref(struct uio * uio) {
    return ++uio->refcnt;
}

static const struct uio_intf pipe_writer_intf = {
    .close = &pipe_writer_close,
    .write = &pipe_write
};

static const struct uio_intf pipe_reader_intf = {
    .close = &pipe_reader_close,
    .read = &pipe_read
};

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

// pipes
void create_pipe(struct uio ** wptr, struct uio ** rptr) {
    struct pipe * pipe;

    pipe = kcalloc(1, sizeof(struct pipe));

    condition_init(&pipe->updated, "pipe.updated");
    pipe->buf = alloc_phys_page();

    *wptr = uio_init1(&pipe->wio, &pipe_writer_intf);
    *rptr = uio_init1(&pipe->rio, &pipe_reader_intf);
}

void pipe_writer_close(struct uio * uio) {
	struct pipe * const pipe = (void*)uio - offsetof(struct pipe, wio);
	assert (uio_refcnt(&pipe->wio) == 0);
	
	if (uio_refcnt(&pipe->rio) == 0) {
		free_phys_page(pipe->buf);
		kfree(pipe);
	}
}

void pipe_reader_close(struct uio * uio) {
    struct pipe * const pipe = (void*)uio - offsetof(struct pipe, rio);
	assert (uio_refcnt(&pipe->rio) == 0);
	
	if (uio_refcnt(&pipe->wio) == 0) {
		free_phys_page(pipe->buf);
		kfree(pipe);
	}
}

long pipe_read(struct uio * uio, void * buf, unsigned long bufsz) {
    struct pipe * const pipe = (void*)uio - offsetof(struct pipe, rio);
	const void * src;
	long n;
	
	if (bufsz == 0)
		return 0;
	
	while (uio_refcnt(&pipe->wio) != 0 && pipe->tpos == 0)
		condition_wait(&pipe->updated);
	
	src = pipe->buf + pipe->hpos;
	
	if (pipe->tpos - pipe->hpos <= bufsz) {
		n = pipe->tpos - pipe->hpos;
		pipe->hpos = 0;
		pipe->tpos = 0;
	} else {
		n = bufsz;
		pipe->hpos += n;
	}

	memcpy(buf, src, n);
	condition_broadcast(&pipe->updated);
	return n;
}

long pipe_write(struct uio * uio, const void * buf, unsigned long len) {
	struct pipe * const pipe = (void*)uio - offsetof(struct pipe, wio);
	long n;
	
	if (len == 0)
		return 0;

    // Fast path: we can append the entire write buffer contents to the buffer.
    // However, if there are no readers left, we need to return -EPIPE and not
    // accept the data.

    if (len <= PIPE_BUFSZ - pipe->tpos) {
        if (uio_refcnt(&pipe->rio) == 0)
            return -EPIPE;
        memcpy(pipe->buf + pipe->tpos, buf, len);
        pipe->tpos += len;
        return len;
    }

    // Otherwise, wait until buffer is empty or there are no readers left.

	while (uio_refcnt(&pipe->rio) != 0 && pipe->tpos != 0)
		condition_wait(&pipe->updated);

    // If there are no readers left, return broken pipe error.

	if (uio_refcnt(&pipe->rio) == 0)
		return -EPIPE;
	
	n = (PIPE_BUFSZ - pipe->tpos < len) ? PIPE_BUFSZ - pipe->tpos : len;
	memcpy(pipe->buf + pipe->tpos, buf, n);
	pipe->tpos += n;

    condition_broadcast(&pipe->updated);
	return n;
}