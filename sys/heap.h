/*! @file heap.h
    @brief Heap memory manager
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _HEAP_H_
#define _HEAP_H_

#include <stddef.h>

#include "memory.h"

// Maximum allocation request size

#define HEAP_ALLOC_MAX 4032

struct heap_stats {
    uint64_t cumul_objects;
    uint64_t cumul_bytes;
    uint32_t managed_bytes;
    uint32_t alloc_objects;
    uint32_t alloc_bytes;
    uint32_t alloc_maxsz;
    uint32_t avail_bytes;
    uint32_t avail_maxsz;
};

extern char heap_initialized;
void heap_init(void * start, size_t size);

extern void heap_copy_stats(struct heap_stats * stat);

void * kmalloc(size_t size);
void * kcalloc(size_t nelts, size_t eltsz);
void kfree(void * ptr);

#endif  // _HEAP_H_