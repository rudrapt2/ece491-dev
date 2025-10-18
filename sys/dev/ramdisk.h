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
 * @return None
 */
extern void ramdisk_attach();

#endif // _MEMIO_H_