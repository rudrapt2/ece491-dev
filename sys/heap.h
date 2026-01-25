// heap.h - Heap memory manager
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//
//
// The heap module provides dynamic memory allocation for kernel code.
//
// This is the allocator used by most of the kernel when the size or lifetime
// of an object is not known at compile time. It is analogous to malloc/calloc/
// free in user space, but it is tailoreid for kernel use and integrates with
// the kernel's page allocator when available.
//
// External users should treat this module as a service with a small API:
//   - heap_init() must be called once during boot before using kmalloc/kfree.
//   - kmalloc() allocates a requested number of bytes.
//   - kcalloc() allocates and zero-fills an array.
//   - kfree() releases a previously allocated object.
//   - copy_heap_stats() reports allocator statistics.
//
// Key behavioral points (from the caller's perspective):
//   - Allocation requests larger than HEAP_ALLOC_MAX are not allowed.
//   - Returned pointers are aligned to at least HEAP_ALIGN bytes.
//   - Passing NULL to kfree() is allowed and has no effect.
//   - Passing an invalid pointer to kfree() will panic. 
//   - Heap API calls with invalid arguments will almost alwasys crash
//     the program. This is intentional as failing silently
//     often hides memory corruption bugs that demand fixing.
//
// This allocator may request additional memory from the physical page allocator
// (alloc_phys_page()) when it cannot satisfy an allocation from existing free
// space. If the system does not provide a page allocator, a weak default
// alloc_phys_page() is used and will panic "Out of memory".
//

#ifndef _HEAP_H_
#define _HEAP_H_

#include <stddef.h>

#include "memory.h"

// Maximum allocation request size (in bytes).
//
// Requests larger than this are treated as programming errors and cause a
// kernel panic. The maximum is chosen so that typical small objects can be
// heap-allocated while discouraging use of the heap for very large buffers.
#define HEAP_ALLOC_MAX 4032

// Heap statistics snapshot.
//
// These fields are intended for introspection.
// Most kernel code should not make functional decisions based on
// these values.

struct heap_stats {
    unsigned long cumul_objects;    // total successful allocations since boot
    unsigned long cumul_bytes;      // total bytes allocated since boot
    unsigned int managed_bytes;     // bytes currently managed by the heap
    unsigned int alloc_objects;     // number of currently allocated objects
    unsigned int alloc_bytes;       // bytes currently allocated (live)
    unsigned int alloc_maxsz;       // largest allocation size ever returned
    unsigned int avail_bytes;       // total bytes currently available for alloc
    unsigned int avail_maxsz;       // largest single allocation currently possible
};

extern char heap_initialized;

// Set to 1 after heap_init() has been called. The allocator requires explicit
// initialization during boot.

void heap_init(void * start, size_t size);

// Initializes the heap allocator.
//
// The heap manages a region of memory supplied by the caller. If /size/ is 0,
// the heap is initialized in an "empty" state and will rely on alloc_phys_page()
// to obtain memory on demand (if available).
//
// The provided region is aligned internally and may be rounded down to satisfy
// alignment constraints.
//
// On entry heap_init() assumes:
// - It is called once during system initializatino.
//
// On return heap_init() guarantees:
// - heap_initialized is set to 1.
// - The heap may service allocation requests (subject to available memory).

extern void copy_heap_stats(struct heap_stats * stat);

// Copies a snapshot of heap allocator statistics into *stat.
//
// On entry copy_heap_stats() assumes:
// - /stat/ is a valid pointer to writable memory.
//
// On return copy_heap_stats() guarantees:
// - *stat contains a consistent snapshot of heap statistics.

void * kmalloc(size_t size);

// Allocates /size/ bytes from the heap and returns a pointer to the allocated
// memory.
//
// The returned pointer is aligned to at least HEAP_ALIGN bytes. The contents of
// the returned memory are not meaningful to the caller; this allocator may fill
// newly allocated memory with a debugging pattern.
//
// On entry kmalloc() assumes:
// - heap_init() has been called and heap_initialized is 1.
// - /size/ is in the range [1..HEAP_ALLOC_MAX].
//
// On return kmalloc() guarantees (on success):
// - The returned pointer is non-NULL.
// - The returned region has at least /size/ bytes of usable storage.
//
// * This function must _not_ be called from an ISR.

void * kcalloc(size_t nelts, size_t eltsz);

// Allocates an array of /nelts/ elements of size /eltsz/ each and returns a
// pointer to the allocated memory. The returned memory is zero-filled.
//
// This function checks for overflow in the multiplication nelts*eltsz. If the
// computed total size is too large, the kernel panics.
//
// On entry kcalloc() assumes:
// - heap_init() has been called and heap_initialized is 1.
// - /nelts/ > 0 and /eltsz/ > 0.
// - (nelts * eltsz) is in the range [1..HEAP_ALLOC_MAX].
//
// On return kcalloc() guarantees (on success):
// - The returned pointer is non-NULL.
// - The returned region contains all zeros.
//
// * This function must _not_ be called from an ISR.

void kfree(void * ptr);

// Frees an allocation previously returned by kmalloc() or kcalloc().
//
// Passing NULL is permitted and has no effect. Passing an invalid pointer
// (misaligned, not heap-allocated, or already freed) is treated as a fatal
// error and will panic.
//
// On entry kfree() assumes:
// - If /ptr/ is non-NULL, it was returned by kmalloc() or kcalloc() and has not
//   already been freed.
//
// On return kfree() guarantees:
// - The storage previously associated with /ptr/ is returned to the heap.
//
// * This function must _not_ be called from an ISR.

#endif  // _HEAP_H_
