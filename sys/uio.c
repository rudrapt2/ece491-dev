// uio.c - Uniform I/O interface
//

#ifdef UIO_DEBUG
#define DEBUG
#endif

#ifdef UIO_TRACE
#define TRACE
#endif

#include "uio.h"
#include "uioimpl.h"
#include "error.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "heap.h"
#include "misc.h"

#include <stddef.h> // for NULL and offsetof

#define PIPE_BUFSZ PAGE_SIZE

/**
 * @brief I/O endpoint backed by a block of memory. Allows modification of the backing memory block.
 */
struct memuio
{
  struct uio uio; ///< I/O struct of memory I/O
  void *buf;      ///< Block of memory
  size_t size;    ///< Size of memory block
};

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

static int memuio_cntl(struct uio *uio, int cmd, void *arg);
static long memuio_read(struct uio *uio, void *buf, unsigned long bufsz);
static long memuio_write(struct uio *uio, const void *buf, unsigned long len);

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
  debug("uio_close: refcnt=%d, has_close=%d", uio->refcnt, (uio->intf->close != NULL));

  // Decrement reference count if it's greater than 0
  if (uio->refcnt > 0)
  {
    uio->refcnt--;
    debug("uio_close: decremented refcnt to %d", uio->refcnt);
  }

  // Only call the actual close method when refcnt reaches 0
  if (uio->refcnt == 0 && uio->intf->close != NULL)
  {
    debug("uio_close: calling close method");
    uio->intf->close(uio);
  }
  else if (uio->refcnt > 0)
  {
    debug("uio_close: NOT calling close (refcnt=%d still has references)", uio->refcnt);
  }
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

static const struct uio_intf memuio_intf = {
    .close = (void (*)(struct uio *))&kfree,
    .read = &memuio_read,
    .write = &memuio_write,
    .cntl = &memuio_cntl};

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

struct uio *create_memory_uio(void *buf, size_t size)
{
  struct memuio *muio;

  muio = kmalloc(sizeof(struct memuio));

  muio->buf = buf;
  muio->size = size;

  return uio_init1(&muio->uio, &memuio_intf);
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

/**
 * @brief _read_ implementation for mem I/O.
 * @details Performs proper bounds checks, then copies data from memory block to passed buffer
 * @param uio I/O struct pointer for memory I/O backend
 * @param buf Buffer to copy data from memory to
 * @param bufsz Number of bytes to read from memory
 * @return Number of bytes successfully read
 */
long memuio_read(struct uio *uio, void *buf, unsigned long bufsz)
{
  struct memuio *const muio = (void *)uio - offsetof(struct memuio, uio);
  unsigned long len;

  if (muio->size < bufsz)
    len = muio->size;
  else
    len = bufsz;

  memcpy(buf, muio->buf, len);
  return len;
}

/**
 * @brief _write_ implementation for mem I/O.
 * @details Performs proper bounds checks, then copies data from passed buffer to memory block.
 * Writes should **not** exceed size of backing memory.
 * @param uio I/O struct pointer for memory I/O
 * @param buf Buffer containing data to write into memory
 * @param len Number of bytes to write to memory
 * @return Number of bytes successfully written
 */
long memuio_write(struct uio *uio, const void *buf, unsigned long len)
{
  struct memuio *const muio = (void *)uio - offsetof(struct memuio, uio);

  if (muio->size < len)
    len = muio->size;

  memcpy(muio->buf, buf, len);
  return len;
}

/**
 * @brief _cntl_ functions for mem I/O.
 * @details Memory I/O supports the following _cntl_ commands:
 * FCNTL_GETEND, FCNTL_SETEND (new size should **not** exceed size of backing memory)
 * @param uio I/O struct pointer for memory I/O
 * @param cmd command to run
 * @param arg Argument for commands; size of backing memory is returned via arg for FCNTL_GETEND, new end is passed via arg for FCNTL_SETEND
 * @return 0 on success for FCNTL_GETEND or FCNTL_SETEND, error on failure or unsupported command
 */
int memuio_cntl(struct uio *uio, int cmd, void *arg)
{
  struct memuio *const muio = (void *)uio - offsetof(struct memuio, uio);
  unsigned long long *const ullarg = arg;

  switch (cmd)
  {
  case FCNTL_GETEND:
    *ullarg = muio->size;
    return 0;
  case FCNTL_SETEND:
    if (muio->size < *ullarg)
      return -EINVAL;
    muio->size = (size_t)*ullarg;
    return 0;
  default:
    return -ENOTSUP;
  }
}