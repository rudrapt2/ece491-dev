// cache.h - Cache for a storage object
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _CACHE_H_
#define _CACHE_H_

#include "io.h"

struct cache; // opaque decl.

#define CACHE_BLKSZ 512

// extern struct cache * create_cache(struct io * bkgio, unsigned int capacity);

extern struct cache * create_cache(struct io * bkgio);

extern unsigned int cache_blksz(const struct cache * ca);

// extern int cache_fetch(struct cache * ca, unsigned long long pos, void ** pptr, int exclusive);

extern int cache_fetch(struct cache * ca, unsigned long long pos, void ** pptr);

extern void cache_release(struct cache * ca, void * ptr, int dirty);

extern int cache_flush(struct cache * ca);

extern void cache_close(struct cache * ca);

#endif // _CACHE_H_
