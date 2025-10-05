/*! @file memio.h
    @brief Memory-backed I/O interface
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _MEMIO_H_
#define _MEMIO_H_

#include <stddef.h>

struct storage; // forward declaration

// EXPORTED FUNCTION DECLARATIONS
//

/**
 * @brief Creates and registers a memory-backed storage device
 * @param name Device name to register with
 * @param buf Pointer to memory buffer that will back the storage operations
 * @param size Size of memory buffer in bytes
 * @return 0 on success, negative error code on failure
 */
extern int attach_memory_storage(const char *name, void *buf, size_t size);

/**
 * @brief Creates and registers a blob-backed storage device using embedded kernel blob data
 * @param name Device name to register with
 * @return 0 on success, negative error code on failure
 */
extern int attach_blob_storage(const char *name);

/**
 * @brief Creates a memory-backed storage device that supports fetch, store, and control operations
 * @param buf Pointer to memory buffer that will back the storage operations
 * @param size Size of memory buffer in bytes
 * @return Pointer to newly created storage device
 */
extern struct storage *create_memory_storage(void *buf, size_t size);

#endif // _MEMIO_H_