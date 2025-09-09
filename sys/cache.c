// cache.c - Block cache for a storage device
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef CACHE_TRACE
#define TRACE
#endif

#ifdef CACHE_DEBUG
#define DEBUG
#endif

#include "cache.h"
#include "assert.h"
#include "conf.h"
#include "io.h"
#include "memory.h"
#include "error.h"
#include "string.h"
#include "thread.h"
#include "heap.h"

#define CACHE_CAPACITY 64 // must be power of two

// INTERNAL TYPE DEFINITIONS
//

struct cache_entry {
    unsigned long long pos; // position of cached block on device
    unsigned int refcnt; // number of references to this entry
    char locked; // 1 of locked, 0 otherwise
    char used; // 1 if entry accessed recently, 0 otherwise
    char dirty; // needs to be written to disk
};

struct cache {
    struct io * bkgio;
    struct condition unlocked; // an entry has been unlocked
    struct condition evictable; // a entry became evictable
    struct condition writable; // a block can be written back
    struct condition nodirty; // no dirty blocks in cache
    unsigned short nidx; // position from which to start search for pages
    unsigned short evictable_cnt; // number of evictable entries
    unsigned short dirty_cnt; // number of dirty blocks in cache
    int wtid; // writer thread id (we don't actually use it for anything)
    void * blkbuf; // all block are in a single contiguous buffer
    struct cache_entry entries[CACHE_CAPACITY];
};

// Some definitions:
//
// A cache entry is DIRTY if its dirty flag is set. This happens
// cache_release_block() is called with dirty = 1; dirty_cnt is number of such
// entries in the cache.
//
// A cache entry is EVICTABLE if its reference count (refcnt) is zero and it is
// not dirty; evictable_cnt is the number of such entries in the cache.
// Initially, all entries are evictable.
//
// A cache entry is WRITABLE if it is dirty and unreferenced. Note that locked
// implies refcnt > 0.

static int evictable(const struct cache_entry * ent) {
    return (ent->refcnt == 0) && !ent->dirty;
}

static int writable(const struct cache_entry * ent) {
    return (ent->refcnt == 0) && ent->dirty;
}

// INTERNAL FUNCTION DECLARATIONS
//

/// @brief Converts a cache block index to a block psysical pointer.
/// @param cache The cache for which the index should be convered.
/// @param idx The block index in the cache.
/// @return A physical pointer to the block.

static void * blkidx_to_blkptr (
    const struct cache * cache, unsigned long idx);

/// @brief Converts a physical pointer to a cache block into a block index.
/// @param cache The cache containing the physical block.
/// @param pblk the physical block address.
/// @return An index of the block in the cache.

static unsigned long blkptr_to_blkidx (
    const struct cache * cache, void * pblk);

/// @brief Returns the index of a block that can be evicted from the cache.
/// @param cache The cache in which the evictable block is to be found.
/// @return An index to the evicable block.
/// The referenced block is guaranteed to be unreferenced. This function may
/// block until an evictable block becomes available.
static int find_evictable(struct cache * cache);

static void cache_writeback_thrfn(struct cache * cache);

// EXPORTED FUNCTION DEFINITIONS
//

int create_cache(struct io * bkgio, struct cache ** cptr) {
    struct cache * cache;
    int bkgblksz;
    int i;

    trace("%s(%p)", __func__, bkgio);

    // Get backing device block size. Make sure it divides cache block size.

    bkgblksz = ioblksz(bkgio);
    assert (bkgblksz < 0 || CACHE_BLKSZ % bkgblksz == 0);

    cache = kcalloc(1, sizeof(struct cache));

    cache->bkgio = ioaddref(bkgio);
    condition_init(&cache->unlocked, "cache.unlocked");
    condition_init(&cache->evictable, "cache.evictable");
    condition_init(&cache->writable, "cache.writable");
    condition_init(&cache->nodirty, "cache.nodirty");

    cache->blkbuf = alloc_phys_pages (
        (CACHE_CAPACITY * CACHE_BLKSZ + PAGE_SIZE-1) / PAGE_SIZE);

    for (i = 0; i < CACHE_CAPACITY; i++)
        cache->entries[i].pos = -1ULL;
    
    cache->evictable_cnt = CACHE_CAPACITY;

    // Create write-back thread

    cache->wtid = thread_spawn (
        "cache_writeback", (void(*)(void))&cache_writeback_thrfn, cache);
    
    assert (0 <= cache->wtid);
    // thread_detach(cache->wtid);

    *cptr = cache;
    return 0;
}

int cache_get_block(struct cache * cache, unsigned long long pos, void ** pptr) {
    struct cache_entry * ent; // cache entry for block
    long rcnt; // return value from ioread
    int i; // block index

    trace("%s(0x%llx)", __func__, pos);

    if (pos % CACHE_BLKSZ != 0)
        return -EINVAL;

    for (;;) {
        // If the block is already in the cache, increment reference count to it
        // does not get evicted and sleep until we can lock it.

        for (i = 0; i < CACHE_CAPACITY; i++) {
            if (cache->entries[i].pos == pos) {
                ent = cache->entries + i;
                
                if (evictable(ent))
                    cache->evictable_cnt -= 1;
                
                ent->refcnt += 1;
                
                debug("Found block 0x%llx in cache (locked=%d) at %p", pos, ent->locked, blkidx_to_blkptr(cache, i));

                while (ent->locked)
                    condition_wait(&cache->unlocked);
                
                ent->locked = 1;
                debug("Locked block %d (pos=0x%llx) at %p in cache", i, pos, blkidx_to_blkptr(cache, i));

                *pptr = blkidx_to_blkptr(cache, i);

                debug("%08lx: "
                    "%02x %02x %02x %02x %02x %02x %02x %02x "
                    "%02x %02x %02x %02x %02x %02x %02x %02x  "
                    "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                    *pptr,
                    ((uint8_t*)*pptr)[0], ((uint8_t*)*pptr)[1], ((uint8_t*)*pptr)[2], ((uint8_t*)*pptr)[3],
                    ((uint8_t*)*pptr)[4], ((uint8_t*)*pptr)[5], ((uint8_t*)*pptr)[6], ((uint8_t*)*pptr)[7],
                    ((uint8_t*)*pptr)[8], ((uint8_t*)*pptr)[9], ((uint8_t*)*pptr)[0xA], ((uint8_t*)*pptr)[0xB],
                    ((uint8_t*)*pptr)[0xC], ((uint8_t*)*pptr)[0xD], ((uint8_t*)*pptr)[0xE], ((uint8_t*)*pptr)[0xF],
                    (0x20 <= ((uint8_t*)*pptr)[0] && ((uint8_t*)*pptr)[0] < 0x7f) ? ((char*)*pptr)[0] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[1] && ((uint8_t*)*pptr)[1] < 0x7f) ? ((char*)*pptr)[1] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[2] && ((uint8_t*)*pptr)[2] < 0x7f) ? ((char*)*pptr)[2] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[3] && ((uint8_t*)*pptr)[3] < 0x7f) ? ((char*)*pptr)[3] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[4] && ((uint8_t*)*pptr)[4] < 0x7f) ? ((char*)*pptr)[4] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[5] && ((uint8_t*)*pptr)[5] < 0x7f) ? ((char*)*pptr)[5] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[6] && ((uint8_t*)*pptr)[6] < 0x7f) ? ((char*)*pptr)[6] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[7] && ((uint8_t*)*pptr)[7] < 0x7f) ? ((char*)*pptr)[7] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[8] && ((uint8_t*)*pptr)[8] < 0x7f) ? ((char*)*pptr)[8] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[9] && ((uint8_t*)*pptr)[9] < 0x7f) ? ((char*)*pptr)[9] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[0xA] && ((uint8_t*)*pptr)[0xA] < 0x7f) ? ((char*)*pptr)[0xA] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[0xB] && ((uint8_t*)*pptr)[0xB] < 0x7f) ? ((char*)*pptr)[0xB] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[0xC] && ((uint8_t*)*pptr)[0xC] < 0x7f) ? ((char*)*pptr)[0xC] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[0xD] && ((uint8_t*)*pptr)[0xD] < 0x7f) ? ((char*)*pptr)[0xD] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[0xE] && ((uint8_t*)*pptr)[0xE] < 0x7f) ? ((char*)*pptr)[0xE] : '.',
                    (0x20 <= ((uint8_t*)*pptr)[0xF] && ((uint8_t*)*pptr)[0xF] < 0x7f) ? ((char*)*pptr)[0xF] : '.');
                
                return 0;
            }
        }
    
        debug("Block 0x%llx not in cache (%u blocks are evictable)",
                pos, cache->evictable_cnt);

        // If there are no cache entries that we can evict right now, wait for a
        // cache update. We have to go back to the top of the loop and re-scan
        // the cache, in case our block was added by another thread.

        if (cache->evictable_cnt == 0) {
            do condition_wait(&cache->evictable);
            while (cache->evictable_cnt == 0);
        } else
            break;
    }

    // If we get here, the block we want is not in the cache and there is an
    // entry we can evict.

    i = find_evictable(cache);
    ent = cache->entries+i;

    if (ent->pos != -1ULL)
        debug("Evicting block 0x%llx from cache (entry %d)", ent->pos, i);
    else
        debug("Using previously unused entry %d for 0x%llx", i, pos);

    ent->pos = pos;

    cache->evictable_cnt -= 1;
    ent->refcnt = 1;
    ent->locked = 1;
    debug("Locked block %d (pos=0x%llx) at %p in cache", i, pos, blkidx_to_blkptr(cache, i));

    // Load block into cache

    *pptr = blkidx_to_blkptr(cache, i);
    debug("Reading block from 0x%llx into cache at %d (pblk = %lp)", pos, i, *pptr);
    rcnt = ioreadat(cache->bkgio, pos, *pptr, CACHE_BLKSZ);

    debug("%08lx: "
        "%02x %02x %02x %02x %02x %02x %02x %02x "
        "%02x %02x %02x %02x %02x %02x %02x %02x  "
        "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
        *pptr,
        ((uint8_t*)*pptr)[0], ((uint8_t*)*pptr)[1], ((uint8_t*)*pptr)[2], ((uint8_t*)*pptr)[3],
        ((uint8_t*)*pptr)[4], ((uint8_t*)*pptr)[5], ((uint8_t*)*pptr)[6], ((uint8_t*)*pptr)[7],
        ((uint8_t*)*pptr)[8], ((uint8_t*)*pptr)[9], ((uint8_t*)*pptr)[0xA], ((uint8_t*)*pptr)[0xB],
        ((uint8_t*)*pptr)[0xC], ((uint8_t*)*pptr)[0xD], ((uint8_t*)*pptr)[0xE], ((uint8_t*)*pptr)[0xF],
        (0x20 <= ((uint8_t*)*pptr)[0] && ((uint8_t*)*pptr)[0] < 0x7f) ? ((char*)*pptr)[0] : '.',
        (0x20 <= ((uint8_t*)*pptr)[1] && ((uint8_t*)*pptr)[1] < 0x7f) ? ((char*)*pptr)[1] : '.',
        (0x20 <= ((uint8_t*)*pptr)[2] && ((uint8_t*)*pptr)[2] < 0x7f) ? ((char*)*pptr)[2] : '.',
        (0x20 <= ((uint8_t*)*pptr)[3] && ((uint8_t*)*pptr)[3] < 0x7f) ? ((char*)*pptr)[3] : '.',
        (0x20 <= ((uint8_t*)*pptr)[4] && ((uint8_t*)*pptr)[4] < 0x7f) ? ((char*)*pptr)[4] : '.',
        (0x20 <= ((uint8_t*)*pptr)[5] && ((uint8_t*)*pptr)[5] < 0x7f) ? ((char*)*pptr)[5] : '.',
        (0x20 <= ((uint8_t*)*pptr)[6] && ((uint8_t*)*pptr)[6] < 0x7f) ? ((char*)*pptr)[6] : '.',
        (0x20 <= ((uint8_t*)*pptr)[7] && ((uint8_t*)*pptr)[7] < 0x7f) ? ((char*)*pptr)[7] : '.',
        (0x20 <= ((uint8_t*)*pptr)[8] && ((uint8_t*)*pptr)[8] < 0x7f) ? ((char*)*pptr)[8] : '.',
        (0x20 <= ((uint8_t*)*pptr)[9] && ((uint8_t*)*pptr)[9] < 0x7f) ? ((char*)*pptr)[9] : '.',
        (0x20 <= ((uint8_t*)*pptr)[0xA] && ((uint8_t*)*pptr)[0xA] < 0x7f) ? ((char*)*pptr)[0xA] : '.',
        (0x20 <= ((uint8_t*)*pptr)[0xB] && ((uint8_t*)*pptr)[0xB] < 0x7f) ? ((char*)*pptr)[0xB] : '.',
        (0x20 <= ((uint8_t*)*pptr)[0xC] && ((uint8_t*)*pptr)[0xC] < 0x7f) ? ((char*)*pptr)[0xC] : '.',
        (0x20 <= ((uint8_t*)*pptr)[0xD] && ((uint8_t*)*pptr)[0xD] < 0x7f) ? ((char*)*pptr)[0xD] : '.',
        (0x20 <= ((uint8_t*)*pptr)[0xE] && ((uint8_t*)*pptr)[0xE] < 0x7f) ? ((char*)*pptr)[0xE] : '.',
        (0x20 <= ((uint8_t*)*pptr)[0xF] && ((uint8_t*)*pptr)[0xF] < 0x7f) ? ((char*)*pptr)[0xF] : '.');

    if (rcnt < 0)
        return rcnt;
    
    if (rcnt != CACHE_BLKSZ)
        return -EIO;
    
    return 0;
}

void cache_release_block(struct cache * cache, void * pblk, int dirty) {
    struct cache_entry * ent;
    int i;

    trace("%s(%p,dirty:%d)", __func__, pblk, dirty);

    debug("%08lx: "
        "%02x %02x %02x %02x %02x %02x %02x %02x "
        "%02x %02x %02x %02x %02x %02x %02x %02x  "
        "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
        pblk,
        ((uint8_t*)pblk)[0], ((uint8_t*)pblk)[1], ((uint8_t*)pblk)[2], ((uint8_t*)pblk)[3],
        ((uint8_t*)pblk)[4], ((uint8_t*)pblk)[5], ((uint8_t*)pblk)[6], ((uint8_t*)pblk)[7],
        ((uint8_t*)pblk)[8], ((uint8_t*)pblk)[9], ((uint8_t*)pblk)[0xA], ((uint8_t*)pblk)[0xB],
        ((uint8_t*)pblk)[0xC], ((uint8_t*)pblk)[0xD], ((uint8_t*)pblk)[0xE], ((uint8_t*)pblk)[0xF],
        (0x20 <= ((uint8_t*)pblk)[0] && ((uint8_t*)pblk)[0] < 0x7f) ? ((char*)pblk)[0] : '.',
        (0x20 <= ((uint8_t*)pblk)[1] && ((uint8_t*)pblk)[1] < 0x7f) ? ((char*)pblk)[1] : '.',
        (0x20 <= ((uint8_t*)pblk)[2] && ((uint8_t*)pblk)[2] < 0x7f) ? ((char*)pblk)[2] : '.',
        (0x20 <= ((uint8_t*)pblk)[3] && ((uint8_t*)pblk)[3] < 0x7f) ? ((char*)pblk)[3] : '.',
        (0x20 <= ((uint8_t*)pblk)[4] && ((uint8_t*)pblk)[4] < 0x7f) ? ((char*)pblk)[4] : '.',
        (0x20 <= ((uint8_t*)pblk)[5] && ((uint8_t*)pblk)[5] < 0x7f) ? ((char*)pblk)[5] : '.',
        (0x20 <= ((uint8_t*)pblk)[6] && ((uint8_t*)pblk)[6] < 0x7f) ? ((char*)pblk)[6] : '.',
        (0x20 <= ((uint8_t*)pblk)[7] && ((uint8_t*)pblk)[7] < 0x7f) ? ((char*)pblk)[7] : '.',
        (0x20 <= ((uint8_t*)pblk)[8] && ((uint8_t*)pblk)[8] < 0x7f) ? ((char*)pblk)[8] : '.',
        (0x20 <= ((uint8_t*)pblk)[9] && ((uint8_t*)pblk)[9] < 0x7f) ? ((char*)pblk)[9] : '.',
        (0x20 <= ((uint8_t*)pblk)[0xA] && ((uint8_t*)pblk)[0xA] < 0x7f) ? ((char*)pblk)[0xA] : '.',
        (0x20 <= ((uint8_t*)pblk)[0xB] && ((uint8_t*)pblk)[0xB] < 0x7f) ? ((char*)pblk)[0xB] : '.',
        (0x20 <= ((uint8_t*)pblk)[0xC] && ((uint8_t*)pblk)[0xC] < 0x7f) ? ((char*)pblk)[0xC] : '.',
        (0x20 <= ((uint8_t*)pblk)[0xD] && ((uint8_t*)pblk)[0xD] < 0x7f) ? ((char*)pblk)[0xD] : '.',
        (0x20 <= ((uint8_t*)pblk)[0xE] && ((uint8_t*)pblk)[0xE] < 0x7f) ? ((char*)pblk)[0xE] : '.',
        (0x20 <= ((uint8_t*)pblk)[0xF] && ((uint8_t*)pblk)[0xF] < 0x7f) ? ((char*)pblk)[0xF] : '.');
    
    i = blkptr_to_blkidx(cache, pblk);
    ent = cache->entries + i;
    assert (ent->locked == 1);
    assert (ent->refcnt > 0);
    
    debug("Releasing block %d (pos = %llx) at %p", i, ent->pos, pblk);

    if (dirty && !ent->dirty) {
        cache->dirty_cnt += 1;
        ent->dirty = 1;
    }

    // Release lock and decrement refcnt, signal all applicable conditions.

    ent->locked = 0;
    debug("Unlocked block %d (pos=0x%llx) in cache", i, ent->pos);
    condition_broadcast(&cache->unlocked);

    ent->refcnt -= 1;
    if (ent->refcnt == 0) {
        if (!ent->dirty) {
            cache->evictable_cnt += 1;
            condition_broadcast(&cache->evictable);
        } else
            condition_broadcast(&cache->writable);
    }
}

int cache_flush(struct cache * cache) {
    while (cache->dirty_cnt > 0)
        condition_wait(&cache->nodirty);
    return 0;
}

// INTERNAL FUNCTION DEFINITIONS
//

void * blkidx_to_blkptr(const struct cache * cache, unsigned long idx) {
    return cache->blkbuf + idx * CACHE_BLKSZ;
}

unsigned long blkptr_to_blkidx(const struct cache * cache, void * pblk) {
    assert (cache->blkbuf <= pblk);
    assert (pblk < cache->blkbuf + CACHE_CAPACITY * CACHE_BLKSZ);
    return (pblk - cache->blkbuf) / CACHE_BLKSZ;
}

int find_evictable(struct cache * cache) {
    struct cache_entry * const ents = cache->entries;
    int const n = CACHE_CAPACITY;
    int i = cache->nidx;

    assert (cache->evictable_cnt != 0);

    for (;;) {
        // Scan until we find an unreferenced entry. Such an entry must exist
        // because cache->unrefcnt != 0.
        while (ents[i % n].refcnt != 0)
            i += 1;
    
        // If the unreferenced entry's used bit is clear, return its index. Set
        // cache->nidx so we start search from following entry next time.

        if (!ents[i % n].used) {
            cache->nidx = i + 1;
            return i % n;
        }

        // Clear the entry's used bit and continue search.
        ents[i % n].used = 0;
        i += 1;
    }
}

void cache_writeback_thrfn(struct cache * cache) {
    struct cache_entry * const ents = cache->entries;
    unsigned int writebackcnt;
    long wcnt;
    int i;

    for (;;) {
        debug("Starting writeback cycle");
        writebackcnt = 0;

        for (i = 0; i < CACHE_CAPACITY; i++) {
            if (writable(ents+i)) {
                ents[i].refcnt = 1;
                ents[i].locked = 1;
                debug("Locked block %d (pos=0x%llx) in cache", i, ents[i].pos);
                writebackcnt += 1;

                debug("Writing dirty block 0x%llx to storage", ents[i].pos);

                wcnt = iowriteat(cache->bkgio,
                    ents[i].pos, blkidx_to_blkptr(cache, i), CACHE_BLKSZ);
                
                if (wcnt != CACHE_BLKSZ) {
                    kprintf("ERROR Cache write-back failed: %s",
                        (wcnt < 0) ? error_name(wcnt) : "short write");
                }

                ents[i].dirty = 0;
                cache->dirty_cnt -= 1;
                ents[i].locked = 0;
                debug("Unlocked block %d (pos=0x%llx) in cache", i, ents[i].pos);

                ents[i].refcnt -= 1; // can't assume refcnt is still 1

                condition_broadcast(&cache->unlocked);

                if (ents[i].refcnt == 0) {
                    cache->evictable_cnt += 1;
                    condition_broadcast(&cache->evictable);
                }
            }
        }
        
        debug("Finished writeback cycle with writebackcnt = %d", writebackcnt);

        // If we wrote some blocks to disk, new blocks may have become dirty
        // (since iowriteat may context-switch), so we need to re-scan the
        // cache. Sleep only when a full scan finds no writable blocks.

        if (writebackcnt == 0 || cache->dirty_cnt == 0) {
            if (cache->dirty_cnt == 0)
                condition_broadcast(&cache->nodirty);
            debug("Waiting for updates");
            condition_wait(&cache->writable);
        }
    }
}
