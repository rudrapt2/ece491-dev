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
    unsigned long cumul_objects;
    unsigned long cumul_bytes;
    unsigned int managed_bytes;
    unsigned int alloc_objects;
    unsigned int alloc_bytes;
    unsigned int alloc_maxsz;
    unsigned int avail_bytes;
    unsigned int avail_maxsz;
};

extern char heap_initialized;
void heap_init(void * start, size_t size);

extern void copy_heap_stats(struct heap_stats * stat);

void * kmalloc(size_t size);
void * kcalloc(size_t nelts, size_t eltsz);
void kfree(void * ptr);

#endif  // _HEAP_H_