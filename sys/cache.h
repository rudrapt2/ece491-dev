/*! @file cache.h
    @brief Block cache for a storage device. 
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifndef _CACHE_H_
#define _CACHE_H_

#define CACHE_BLKSZ 4096UL // size of cache block
#define CACHE_DIRTY 1
#define CACHE_CLEAN 0

struct storage; // external
struct cache; // opaque decl.

//helper function for ktfs (TODO: REMOVE)
extern int cache_get_backing_device(struct cache * cache, struct storage ** disk);

/**
 * @brief Creates/initializes a cache with the passed backing storage device (disk) and makes it
 * available through cptr.
 * @param disk Pointer to the backing storage device.
 * @param cptr Pointer to the cache to create.
 * @return 0 on success, negative error code if error
 */
extern int create_cache(struct storage* sto, struct cache** cptr);

/**
 * @brief Reads a CACHE_BLKSZ sized block from the backing interface into the cache.
 * @param cache Pointer to the cache.
 * @param pos Position in the backing storage device. Must be aligned to a multiple of the block
 * size of the backing interface.
 * @param pptr Pointer to the block pointer read from the cache. Assume that CACHE_BLKSZ will always
 * be equal to the block size of the storage disk. Any replacement policy is permitted, as long as
 * your design meets the above specifications.
 * @return 0 on success, negative error code if error
 */
extern int cache_get_block(struct cache* cache, unsigned long long pos, void** pptr);

/**
 * @brief Releases a block previously obtained from cache_get_block().
 * @param cache Pointer to the cache.
 * @param pblk Pointer to a block that was made available in cache_get_block() (which means that
 * pblk == *pptr for some pptr).
 * @param dirty Indicates whether the block has been modified (1) or not (0). If dirty == 1, the
 * block has been written to. If dirty == 0, the block has not been written to.
 * @return 0 on success, negative error code if error
 */
extern void cache_release_block(struct cache* cache, void* pblk, int dirty);

/**
 * @brief Flushes the cache to the backing device
 * @param cache Pointer to the cache to flush
 * @return 0 on success, error code if error
 */
extern int cache_flush(struct cache* cache);

#endif // _CACHE_H_
