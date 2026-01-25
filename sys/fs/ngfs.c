// ngfs.c - A FAT-like file system
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef NGFS_TRACE
#define TRACE
#endif

#ifdef NGFS_DEBUG
#define DEBUG
#endif

#include "ngfs.h"

#include "../cache.h"
#include "../console.h"
#include "../device.h"
#include "../error.h"
#include "../filesys.h"
#include "../fsimpl.h"
#include "../heap.h"
#include "../misc.h"
#include "../string.h"
#include "../thread.h"
#include "../ioimpl.h"

// INTERNAL TYPE DEFINITIONS
//

#define IDX_TO_ABS(block)       (ngfs->num_fat_blocks+block)
#define DENTRYSZ                (sizeof(struct ngfs_dir_entry))
#define FAT_ENTRYSZ             (sizeof(uint32_t))
#define DENTRIES_PER_BLOCK      (NGFS_BLKSZ / DENTRYSZ)

#define CACHE_CLEAN 0
#define CACHE_DIRTY 1

#define RM_FILE(file)                                   \
    do {                                                \
        if (file->next) file->next->prev = file->prev;  \
        if (file->prev) file->prev->next = file->next;  \
        if (file == files_list) files_list = file->next;\
    } while (0)

// TODO: add locks
struct ngfs_file {
    struct ngfs_dir_entry dentry;
    struct ngfs * fs;
    struct ngfs_file * next;
    struct ngfs_file * prev;

    unsigned long refcnt;
};

struct ngfs_io {
    struct seekio base;
    struct ngfs_file * file;
    uint32_t blockno;
    uint32_t block;
};

struct ngfs_listing_io {
    struct io base;
    struct ngfs * fs;
    uint32_t dir_idx;
};

struct ngfs {
    struct filesystem base;
    struct cache * cache;
    struct ngfs_file * files_list;
    unsigned int size;
    uint32_t num_fat_blocks;
    struct ngfs_dir_entry root_dir;
};

// INTERNAL FUNCTION DECLARATIONS
//

static int ngfs_open(struct filesystem * fs, const char * name, struct io ** ioptr);
static void ngfs_reclaim(struct io* io);
static int ngfs_ioctl(struct io * io, int op, void * arg);
static long ngfs_fetch(struct io * io, unsigned long long pos, void * buf, long bufsz);
static long ngfs_store(struct io * io, unsigned long long pos, const void * buf, long len);
static int ngfs_create(struct filesystem * fs, const char* name);
static int ngfs_delete(struct filesystem * fs, const char* name);
static void ngfs_flush(struct filesystem * fs);

static int ngfs_open_file(struct ngfs * fs, const char * name, struct io ** ioptr);
static int ngfs_open_listing(struct ngfs * fs, struct io ** ioptr);
static long ngfs_listing_read(struct io * io, void * buf, long bufsz);

// Interal helper functions
static long read_from_block(struct cache * cache, uint32_t block, uint32_t offset, void* buf, long bufsz);
static long write_to_block(struct cache * cache, uint32_t block, uint32_t offset, void* buf, long bufsz);

static uint32_t get_free_data_block(struct ngfs * fs);
static void set_next_data_block(struct cache * cache, uint32_t block, uint32_t next);
static uint32_t get_next_data_block(struct cache * cache, uint32_t block);
static void free_blocks(struct cache * cache, uint32_t start);
static void * get_cache_from_block(struct cache * cache, uint32_t block);
static uint32_t get_nth_data_block(struct cache * cache, struct ngfs_dir_entry * file, unsigned int n);
static inline long update_root_dir(struct ngfs * ngfs);
static int iterate_dentry(struct ngfs * ngfs, uint32_t * idx, uint32_t * block, struct ngfs_dir_entry ** dentry, int dirty);
static int free_associated_cache_block(struct cache * cache, struct ngfs_dir_entry * entry, int dirty);
static int update_size(struct ngfs * ngfs, struct ngfs_dir_entry * dentry, uint32_t end);

// INTERNAL GLOBAL VARIABLES

static const struct filesystem ngfs_fs = {
    .implname = "ngfs",
    .openfile = &ngfs_open,
    .createfile = &ngfs_create,
    .deletefile = &ngfs_delete,
    .flush = &ngfs_flush
};

static const struct iointf ngfs_file_intf = {
    .implname = "ngfs_fileio",
    .reclaim = &ngfs_reclaim,
    .read = &seekio_read,
    .write = &seekio_write,
    .fetch = &ngfs_fetch,
    .store = &ngfs_store,
    .ioctl = &ngfs_ioctl
};

static const struct iointf ngfs_listing_io_intf = {
    .implname = "ngfs_lsio",
    .reclaim = (void(*)(struct io*))&kfree,
    .read = &ngfs_listing_read
};

int mount_ngfs(const char * name, struct io * bkgio) {
    int result;
    struct ngfs * ngfs;
    uint32_t num_files;
    struct cache * cache;
    struct ngfs_file * f;
    struct ngfs_dir_entry * root_dir;
    struct ngfs_dir_entry * dentry;
    uint32_t block;

    ngfs = kcalloc(1, sizeof(*ngfs));
    root_dir = &ngfs->root_dir;

    cache = create_cache(bkgio, NGFS_BLKSZ);

    if (cache == NULL) return -EINVAL;
    ngfs->cache = cache;

    result = ioctl(bkgio, IOC_GETEND, &ngfs->size);

    if(result != 0) return result;

    ngfs->num_fat_blocks = 
        CEIL(ngfs->size / NGFS_BLKSZ, NGFS_FAT_ENTRIES_PER_BLOCK);

    debug("Mounting filesystem with %d FAT blocks, %d total blocks\n", 
        ngfs->num_fat_blocks, size / NGFS_BLKSZ);

    // root entry is always the first entry
    read_from_block(cache, NGFS_ROOT_DATA_BLOCK, 0, root_dir, DENTRYSZ);

    // well-formed fs
    assert(strncmp(root_dir->name, ".", NGFS_MAX_FILENAME_LEN) == 0);
    assert(root_dir->start_block == NGFS_ROOT_DATA_BLOCK);
    assert(root_dir->size % DENTRYSZ == 0);

    num_files = root_dir->size / DENTRYSZ;

    debug("Starting with %d files\n", num_files);

    for (uint32_t idx = 1; 
        iterate_dentry(ngfs, &idx, &block, &dentry, CACHE_CLEAN);) 
    {
        f = kcalloc(1, sizeof(struct ngfs_file));
        memcpy(&f->dentry, dentry, DENTRYSZ);

        f->fs = ngfs;
        f->next = ngfs->files_list;
        if (f->next) f->next->prev = f;
        ngfs->files_list = f;
    }

    ngfs->base = ngfs_fs;

    return mount_filesys(name, &ngfs->base);
}

int ngfs_open(struct filesystem * fs, const char * name, struct io ** ioptr) {
    struct ngfs * ngfs = (void *)fs;
    trace("%s(%s,%p)", __func__, name, ioptr);

    if (name == NULL || *name == '\0')
        return ngfs_open_listing(ngfs, ioptr);
    else
        return ngfs_open_file(ngfs, name, ioptr);
}

int ngfs_open_file(struct ngfs * fs, const char * name, struct io ** ioptr) {
    struct ngfs_io * fio;
    struct ngfs_file * f;
    struct ngfs_dir_entry * dentry;
    struct ngfs_dir_entry * root_dir = &fs->root_dir;
    struct ngfs_file * files_list = fs->files_list;
    trace("%s(%s,%p)", __func__, name, ioptr);

    if (strncmp(name, root_dir->name, NGFS_MAX_FILENAME_LEN) == 0)
        return -EACCESS;

    for(f = files_list; f != NULL; f = f->next) 
        if (strcmp(name, f->dentry.name) == 0)
            break;

    if (f == NULL) return -ENOENT;

    f->refcnt++;

    fio = kcalloc(1, sizeof(struct ngfs_io));
    fio->blockno = 0;
    fio->block = f->dentry.start_block;
    fio->file = f;
    *ioptr = seekio_init(&fio->base, &ngfs_file_intf, f->dentry.size, 1, 1);

    debug("ngfs_open: SUCCESS - file=%s, pos=0, refcnt=1", name);
    return 0;
}

void ngfs_reclaim(struct io * io) {
    trace("%s()", __func__);
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    
    assert(f->refcnt > 0);
    kfree(fio);
    f->refcnt--;
}

long ngfs_fetch(
    struct io * io, unsigned long long pos, void * buf, long bufsz) 
{
    trace("%s(%p,%ld)", __func__, buf, len);
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    struct ngfs * ngfs = f->fs;
    struct cache * cache = ngfs->cache;
    uint32_t new_blockno = pos / NGFS_BLKSZ;
    uint32_t bytes_read = 0;
    uint32_t block = fio->block;
    uint32_t offset, remaining_len, read_len;
    
    // since we cache the block, we need to be careful if it changes
    if (new_blockno > fio->blockno) {
        for (int i = 0; i < new_blockno - fio->blockno; i++)
            block = get_next_data_block(cache, block);
    }
    else if (new_blockno < fio->blockno) {
        block = get_nth_data_block(cache, &f->dentry, new_blockno);
    }

    for (remaining_len = bufsz; remaining_len > 0; 
        block = get_next_data_block(cache, block)) {
        assert(block != NGFS_BLOCK_END);
        
        offset = pos % NGFS_BLKSZ;
        read_len = MIN(remaining_len, NGFS_BLKSZ - offset);
        read_from_block(cache, IDX_TO_ABS(block), offset, 
            (uint8_t*)buf + bytes_read, read_len);

        remaining_len -= read_len;
        pos += read_len;
        bytes_read += read_len;
        fio->block = block;
    }
    
    fio->blockno = pos / NGFS_BLKSZ;

    return bytes_read;
}

long ngfs_store(
    struct io * io, unsigned long long pos, const void * buf, long len) 
{
    trace("%s(%p,%ld)", __func__, buf, len);
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    struct ngfs * ngfs = f->fs;
    struct cache * cache = ngfs->cache;
    uint32_t new_blockno = pos / NGFS_BLKSZ;
    uint32_t bytes_written = 0;
    uint32_t block, start_block;
    uint32_t offset, remaining_len, write_len;
    
    // extend file size of necessary
    if (f->dentry.size - pos < len) {
        int result = update_size(ngfs, &f->dentry, pos + len);
        if (result != 0)
            return result;
    }

    // since we cache the block, we need to be careful if it changes
    if (new_blockno > fio->blockno) {
        for (int i = 0; i < new_blockno - fio->blockno; i++)
            block = get_next_data_block(cache, block);
    }
    else if (new_blockno < fio->blockno) {
        block = get_nth_data_block(cache, &f->dentry, new_blockno);
    }
    
    remaining_len = len;

    for (remaining_len = len; remaining_len > 0; 
        block = get_next_data_block(cache, block)) {
        
        offset = pos % NGFS_BLKSZ;
        write_len = MIN(remaining_len, NGFS_BLKSZ - offset);
        write_to_block(cache, IDX_TO_ABS(block), offset, 
            (uint8_t*)buf+bytes_written, write_len);

        remaining_len -= write_len;
        pos += write_len;
        bytes_written += write_len;
        fio->block = block;
    }

    fio->blockno = pos / NGFS_BLKSZ;

    return bytes_written;
}

int ngfs_create(struct filesystem * fs, const char * name) {
    uint32_t file_idx;
    struct ngfs_dir_entry * root_entries;
    struct ngfs * ngfs = (void *)fs;
    struct cache * cache = ngfs->cache;
    struct ngfs_dir_entry * root_dir = &ngfs->root_dir;
    uint32_t curr_size = root_dir->size;

    if (strlen(name) > NGFS_MAX_FILENAME_LEN)
        return -EINVAL;

    // Check if the file already exists
    for (struct ngfs_file * f = ngfs->files_list; f != NULL; f = f->next)
        if (strcmp(name, f->dentry.name) == 0)
            return -EEXIST;

    file_idx = (curr_size % NGFS_BLKSZ) / DENTRYSZ;

    if (update_size(ngfs, root_dir, curr_size + DENTRYSZ))
        return -ENOMEM;

    root_entries = (struct ngfs_dir_entry *)get_cache_from_block(cache, 
        get_nth_data_block(cache, root_dir, curr_size / NGFS_BLKSZ));

    // Add to root directory
    root_entries[file_idx].size = 0;
    root_entries[file_idx].start_block = NGFS_BLOCK_END;
    strncpy(root_entries[file_idx].name, name, NGFS_MAX_FILENAME_LEN);
    cache_release(cache, (void *)root_entries, CACHE_DIRTY);

    update_root_dir(ngfs);

    cache_flush(cache);

    return 0;
}

int ngfs_delete(struct filesystem * fs, const char * name) {
    struct ngfs_file * f;
    struct ngfs_dir_entry last_dentry;
    uint32_t block, last_block, last_offset, new_size;
    struct ngfs * ngfs = (void *)fs;
    struct cache * cache = ngfs->cache;
    struct ngfs_dir_entry * root_dir = &ngfs->root_dir;
    struct ngfs_file * files_list = ngfs->files_list;
    uint32_t num_files = root_dir->size / DENTRYSZ;
    struct ngfs_dir_entry * dentry;

    if (strcmp(name, root_dir->name) == 0)
        return -EACCESS;

    // Remove from files list
    for (f = files_list; f != NULL; f = f->next) {
        if (strcmp(name, f->dentry.name) == 0) {
            if (f->refcnt > 0) return -EBUSY;
            RM_FILE(f);
            break;
        }
    }

    if (f == NULL) return -ENOENT;

    // Free data blocks associated with the file
    free_blocks(cache, f->dentry.start_block);
    kfree(f);

    // Get the last dentry 
    // we do this before entry checking to avoid cache conflicts
    new_size = root_dir->size - DENTRYSZ;
    last_block = get_nth_data_block(cache, root_dir, new_size / NGFS_BLKSZ);
    last_offset = new_size % NGFS_BLKSZ;
    read_from_block(
        cache, IDX_TO_ABS(last_block), last_offset, &last_dentry, DENTRYSZ);

    assert(last_dentry.name[0] != '\0'); // real entry
    
    // overwrite dir entry
    for (uint32_t idx = 1; 
        iterate_dentry(ngfs, &idx, &block, &dentry, CACHE_CLEAN);)
    {
        if (strcmp(name, dentry->name) == 0) {
            memcpy(dentry, &last_dentry, DENTRYSZ);
            free_associated_cache_block(cache, dentry, CACHE_DIRTY);
            break;
        }
    }

    update_size(ngfs, root_dir, new_size);
    update_root_dir(ngfs);

    // Ensure that file deletion is flushed to the backing device
    cache_flush(ngfs->cache);

    return 0;
}

int ngfs_ioctl(struct io * io, int op, void * arg) {
    int result;
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    struct ngfs * ngfs = f->fs;

    if(arg == NULL)
        return -EINVAL;

    switch (op) {
        case IOC_GETEND:
        case IOC_GETPOS:
        case IOC_SETPOS:
            return seekio_ioctl(io, op, arg);
        case IOC_SETEND:
            result = update_size(ngfs, &f->dentry, *(uint32_t *)arg);
            if (result != 0) return result;
            return seekio_ioctl(io, op, arg);
        default:
            return -ENOTSUP;
    }
}

int update_size(struct ngfs * ngfs, struct ngfs_dir_entry * dentry, uint32_t end) {
    uint32_t num_new_blocks, new_block;
    struct cache * cache = ngfs->cache;
    uint32_t block = get_nth_data_block(cache, dentry, dentry->size);

    if (end > dentry->size) { // extend
        num_new_blocks = CEIL(end, NGFS_BLKSZ) - CEIL(dentry->size, NGFS_BLKSZ);
        while (num_new_blocks-- > 0) {
            new_block = get_free_data_block(ngfs);
            if (new_block == NGFS_BLOCK_END) return -ENOMEM;
            if (dentry->size == 0) dentry->start_block = new_block;
            else set_next_data_block(cache, block, new_block);
        }
    }
    else { // truncate
        free_blocks(cache, get_next_data_block(cache, block));
        set_next_data_block(cache, block, NGFS_BLOCK_END);
    }
    
    dentry->size = end;
    return 0;
}

void ngfs_flush(struct filesystem * fs) {
    struct ngfs * ngfs = (void *)fs;
    struct cache * cache = ngfs->cache;
    struct ngfs_dir_entry * dentry;
    uint32_t num_files = ngfs->root_dir.size / DENTRYSZ;
    struct ngfs_file * f = ngfs->files_list;
    uint32_t block;
    
    // write all dentry info to root dir
    // besides the root dentry, we dont care about the order
    for (uint32_t idx = 1; 
        iterate_dentry(ngfs, &idx, &block, &dentry, CACHE_DIRTY);)
    {
        assert(f != NULL); // check consistency
        memcpy(dentry, &f->dentry, DENTRYSZ);
        f = f->next;
    }
    assert(f == NULL); // check consistency

    cache_flush(cache);
}

int ngfs_open_listing(struct ngfs * fs, struct io ** ioptr) {
    struct ngfs_listing_io * ls;
    trace("%s(%p)", __func__, ioptr);

    ls = kcalloc(1, sizeof(struct ngfs_listing_io));
    ls->fs = fs;
    ioinit(&ls->base, &ngfs_listing_io_intf, 1, 1);
    *ioptr = &ls->base;

    return 0;
}

long ngfs_listing_read(struct io * io, void * buf, long bufsz) {
    struct ngfs_listing_io * const ls = (struct ngfs_listing_io*)io;
    struct ngfs * const ngfs = ls->fs;
    struct cache * cache = ngfs->cache;
    struct ngfs_dir_entry * root_dir = &ngfs->root_dir;
    uint32_t total_files = root_dir->size / DENTRYSZ;
    size_t len = MIN(bufsz, NGFS_MAX_FILENAME_LEN + 1);
    uint32_t block, offset, offset_bytes;

    if (ls->dir_idx >= total_files) return 0;

    offset = ls->dir_idx % DENTRIES_PER_BLOCK;
    offset_bytes = offset * DENTRYSZ + offsetof(struct ngfs_dir_entry, name);
    block = get_nth_data_block(
        cache, root_dir, ls->dir_idx / DENTRIES_PER_BLOCK);

    read_from_block(cache, IDX_TO_ABS(block), offset_bytes, buf, len);
    
    debug("%s: reading listing %d (%s)", __func__, ls->dir_idx, (char *)buf);
    
    ls->dir_idx++;
    return len;
}

long read_from_block(
    struct cache * cache, 
    uint32_t block, 
    uint32_t offset, 
    void* buf, 
    long bufsz) 
{
    void * block_data;

    assert(offset + bufsz <= NGFS_BLKSZ); // bad requests
    assert(block != NGFS_BLOCK_END);
    trace("%s(block=%lu,off=%lu,%p,%ld)", __func__, block, offset, buf, bufsz);
    
    if (cache_fetch(cache, block * NGFS_BLKSZ, &block_data) != 0) {
        return -1;
    }

    memcpy(buf, (uint8_t*)block_data + offset, bufsz);
    cache_release(cache, block_data, CACHE_CLEAN);

    return bufsz;
}

long write_to_block(
    struct cache * cache, 
    uint32_t block, 
    uint32_t offset, 
    void* buf, 
    long bufsz) 
{
    void * block_data;

    assert(offset + bufsz <= NGFS_BLKSZ); // bad requests
    assert(block != NGFS_BLOCK_END);
    trace("%s(block=%lu,off=%lu,%p,%ld)", __func__, block, offset, buf, bufsz);
    
    if (cache_fetch(cache, block * NGFS_BLKSZ, &block_data) != 0) {
        return -1;
    }

    memcpy((uint8_t*)block_data + offset, buf, bufsz);
    cache_release(cache, block_data, CACHE_DIRTY);

    return bufsz;
}

uint32_t get_free_data_block(struct ngfs * fs) {
    struct ngfs_fat * fat = NULL;
    uint32_t free_block;

    for (uint32_t fat_block = 0; fat_block < fs->num_fat_blocks; fat_block++) {

        cache_fetch(fs->cache, fat_block * NGFS_BLKSZ, (void**)&fat);

        for (int idx = 0; idx < NGFS_FAT_ENTRIES_PER_BLOCK; idx++) {
            if (fat->fat[idx] == NGFS_BLOCK_FREE) {
                fat->fat[idx] = NGFS_BLOCK_END;
                cache_release(fs->cache, (void *)fat, CACHE_DIRTY);

                // this line is needed bc it breaks otherwise
                // i think this means theres a race condition
                // in the cache? not sure though
                cache_flush(fs->cache);

                free_block = fat_block * NGFS_FAT_ENTRIES_PER_BLOCK + idx;
                return (free_block < fs->size / NGFS_BLKSZ - fs->num_fat_blocks) ?
                    free_block : NGFS_BLOCK_END;
            }
        }

        cache_release(fs->cache, (void *)fat, CACHE_CLEAN);
    }

    // no free blocks
    return NGFS_BLOCK_END;
}

void set_next_data_block(struct cache * cache, uint32_t block, uint32_t next) {
    assert(block != NGFS_BLOCK_END);
    write_to_block(cache,
        block / NGFS_FAT_ENTRIES_PER_BLOCK, 
        (block % NGFS_FAT_ENTRIES_PER_BLOCK) * FAT_ENTRYSZ, 
        &next, sizeof(next));
    // this line is needed as well
    // i think this is also related to the cache race cond
    cache_flush(cache);
}

uint32_t get_next_data_block(struct cache * cache, uint32_t block) {
    uint32_t next;
    assert(block != NGFS_BLOCK_END);
    read_from_block(cache,
        block / NGFS_FAT_ENTRIES_PER_BLOCK, 
        (block % NGFS_FAT_ENTRIES_PER_BLOCK) * FAT_ENTRYSZ, 
        &next, sizeof(next));
    return next;
}

void free_blocks(struct cache * cache, uint32_t start) {
    uint32_t next;
    uint32_t cur = start;
    while (cur != NGFS_BLOCK_END) {
        assert(cur != NGFS_BLOCK_FREE);
        next = get_next_data_block(cache, cur);
        set_next_data_block(cache, cur, NGFS_BLOCK_FREE);
        cur = next;
    }
    cache_flush(cache);
}

void * get_cache_from_block(struct cache * cache, uint32_t block) {
    void* ptr;
    if (cache_fetch(cache, block * NGFS_BLKSZ, &ptr))
        return NULL;
    return ptr;
}

uint32_t get_nth_data_block(struct cache * cache, struct ngfs_dir_entry * file, unsigned int n) {
    uint32_t total_blocks, block;

    total_blocks = file->size / NGFS_BLKSZ;

    // n too large -> get last block
    n = MIN(n, total_blocks);

    block = file->start_block;
    for (int bno = 0; bno < n; bno++) 
        block = get_next_data_block(cache, block);

    return block;
}

inline long update_root_dir(struct ngfs * ngfs) {
    return write_to_block(ngfs->cache,
        IDX_TO_ABS(NGFS_ROOT_DATA_BLOCK), 0, &ngfs->root_dir, DENTRYSZ);
}

int iterate_dentry(
    struct ngfs * ngfs, 
    uint32_t * idx, 
    uint32_t * block, 
    struct ngfs_dir_entry ** dentry, 
    int dirty) 
{
    if (*idx >= ngfs->root_dir.size / DENTRYSZ) {
        free_associated_cache_block(ngfs->cache, *dentry, dirty);
        return 0;
    }

    if (*idx == 1) { // setup
        *block = NGFS_ROOT_DATA_BLOCK;
        *dentry = get_cache_from_block(ngfs->cache, IDX_TO_ABS(*block));
    }
    
    if (*idx % DENTRIES_PER_BLOCK == 0) {
        free_associated_cache_block(ngfs->cache, *dentry, dirty);
        *block = get_next_data_block(ngfs->cache, *block);
        *dentry = get_cache_from_block(ngfs->cache, IDX_TO_ABS(*block));
    }
    else (*dentry)++;

    (*idx)++;
    return 1;
}

int free_associated_cache_block(
    struct cache * cache, 
    struct ngfs_dir_entry * entry, 
    int dirty) 
{
    if (entry == NULL) return -1;
    cache_release(cache, 
        (void *)ROUND_DOWN((uintptr_t)entry, NGFS_BLKSZ), dirty);
    return 0;
}