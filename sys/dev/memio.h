/*! @file memio.h
    @brief Memory-backed I/O interface
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _MEMIO_H_
#define _MEMIO_H_

#include <stddef.h>

struct uio; // forward declaration

// EXPORTED FUNCTION DECLARATIONS
//

/**
 * @brief Creates a memory-backed UIO object that supports read, write, and control operations
 * @param buf Pointer to memory buffer that will back the I/O operations
 * @param size Size of memory buffer in bytes
 * @return Pointer to newly created UIO object
 */
extern struct uio * create_memory_uio(void * buf, size_t size);

#endif // _MEMIO_H_