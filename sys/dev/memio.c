/*! @file memio.c
    @brief Memory-backed I/O implementation
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifdef MEMIO_DEBUG
#define DEBUG
#endif

#ifdef MEMIO_TRACE
#define TRACE
#endif

#include "uio.h"
#include "uioimpl.h"
#include "error.h"
#include "string.h"
#include "heap.h"
#include "misc.h"

#include <stddef.h>

// INTERNAL TYPE DEFINITIONS
//

/**
 * @brief I/O endpoint backed by a block of memory. Allows modification of the backing memory block.
 */
struct memuio {
    struct uio uio; ///< I/O struct of memory I/O
    void * buf; ///< Block of memory
    size_t size; ///< Size of memory block
};

// INTERNAL FUNCTION DECLARATIONS
//

static int memuio_cntl(struct uio * uio, int cmd, void * arg);
static long memuio_read(struct uio * uio, void * buf, unsigned long bufsz);
static long memuio_write(struct uio * uio, const void * buf, unsigned long len);

// INTERNAL GLOBAL CONSTANTS
//

static const struct uio_intf memuio_intf = {
    .close = (void(*)(struct uio*))&kfree,
    .read = &memuio_read,
    .write = &memuio_write,
    .cntl = &memuio_cntl
};

// EXPORTED FUNCTION DEFINITIONS
//

struct uio * create_memory_uio(void * buf, size_t size) {
    struct memuio * muio;

    muio = kmalloc(sizeof(struct memuio));

    muio->buf = buf;
    muio->size = size;

    return uio_init1(&muio->uio, &memuio_intf);
}

// INTERNAL FUNCTION DEFINITIONS
//

/**
 * @brief _read_ implementation for mem I/O.
 * @details Performs proper bounds checks, then copies data from memory block to passed buffer
 * @param uio I/O struct pointer for memory I/O backend
 * @param buf Buffer to copy data from memory to
 * @param bufsz Number of bytes to read from memory
 * @return Number of bytes successfully read
 */
long memuio_read(struct uio * uio, void * buf, unsigned long bufsz) {
    struct memuio * const muio = (void*)uio - offsetof(struct memuio, uio);
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
long memuio_write(struct uio * uio, const void * buf, unsigned long len) {
    struct memuio * const muio = (void*)uio - offsetof(struct memuio, uio);

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
int memuio_cntl(struct uio * uio, int cmd, void * arg) {
    struct memuio * const muio = (void*)uio - offsetof(struct memuio, uio);
    unsigned long long * const ullarg = arg;

    switch (cmd) {
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