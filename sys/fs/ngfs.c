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
#include "../ioimpl.h"
#include "../thread.h"

// INTERNAL TYPE DEFINITIONS
//

#define IDX_TO_ABS(block)       (ngfs->num_fat_blocks+block)
#define DENTRYSZ                (sizeof(struct ngfs_dir_entry))
#define FAT_ENTRYSZ             (sizeof(uint32_t))
#define DENTRIES_PER_BLOCK      (NGFS_BLKSZ / DENTRYSZ)

#define CACHE_CLEAN 0
#define CACHE_DIRTY 1

// TODO: add locks
struct ngfs_file {
    struct ngfs_dir_entry dentry;
    struct ngfs * fs;
    struct ngfs_file * next;
    struct ngfs_file * prev;
    struct rwlock lock;

    unsigned int refcnt;
    unsigned int write_idx;
};

struct ngfs_io {
    struct seekio base;
    struct ngfs_file * file;
    uint32_t blockno;
    uint32_t block;
    unsigned int write_idx;
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
    struct ngfs_dir_entry root_dir;
    struct rwlock fs_lock;
    unsigned long long size;
    uint32_t num_fat_blocks;
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
static uint32_t blockno_to_block(struct cache * cache, struct ngfs_dir_entry * file, unsigned int blockno);
static inline long update_root_dir(struct ngfs * ngfs);
static int iterate_dentry(struct ngfs * ngfs, uint32_t * idx, uint32_t * block, struct ngfs_dir_entry ** dentry, int dirty);
static int free_associated_cache_block(struct cache * cache, struct ngfs_dir_entry * entry, int dirty);
static int update_size(struct ngfs * ngfs, struct ngfs_dir_entry * dentry, uint32_t end);
static void update_pos(struct ngfs * ngfs, struct ngfs_io * fio, uint32_t newpos);
static void clear_block(struct cache * cache, uint32_t block, uint32_t offset, uint32_t len);

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
    .ioctl = &ngfs_ioctl,
    .ioctl_u = (int(*)(struct io*, int, uintptr_t))&ngfs_ioctl
};

static const struct iointf ngfs_listing_io_intf = {
    .implname = "ngfs_lsio",
    .reclaim = (void(*)(struct io*))&kfree,
    .read = &ngfs_listing_read
};

int mount_ngfs(const char * name, struct io * bkgio) {
    int result;
    struct ngfs * ngfs;
    struct cache * cache;
    struct ngfs_file * f;
    struct ngfs_dir_entry * root_dir;
    struct ngfs_dir_entry * dentry = NULL;
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
        ngfs->num_fat_blocks, ngfs->size / NGFS_BLKSZ);

    // root entry is always the first entry
    read_from_block(
        cache, IDX_TO_ABS(NGFS_ROOT_DATA_BLOCK), 0, root_dir, DENTRYSZ);

    // well-formed fs
    assert(strcmp(root_dir->name, ".") == 0);
    assert(root_dir->start_block == NGFS_ROOT_DATA_BLOCK);
    assert(root_dir->size % DENTRYSZ == 0);

    debug("Starting with %d files\n", root_dir->size / DENTRYSZ);

    for (uint32_t idx = 1; 
        iterate_dentry(ngfs, &idx, &block, &dentry, CACHE_CLEAN);) 
    {
        assert(dentry->name[0] != '\0'); // valid entry
        f = kcalloc(1, sizeof(struct ngfs_file));
        memcpy(&f->dentry, dentry, DENTRYSZ);
        rwlock_init(&f->lock, f->dentry.name);

        f->fs = ngfs;
        f->next = ngfs->files_list;
        if (f->next) f->next->prev = f;
        ngfs->files_list = f;
    }

    rwlock_init(&ngfs->fs_lock, "ngfs.lock");

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
    struct ngfs_dir_entry * root_dir = &fs->root_dir;
    trace("%s(%s,%p)", __func__, name, ioptr);

    if (strcmp(name, root_dir->name) == 0)
        return -EACCESS;

    rwlock_acquire(&fs->fs_lock, 1);
    for(f = fs->files_list; f != NULL; f = f->next) 
        if (strcmp(name, f->dentry.name) == 0)
            break;

    if (f == NULL) {
        rwlock_release(&fs->fs_lock);
        return -ENOENT;
    }

    rwlock_acquire(&f->lock, 1);

    if (++f->refcnt == 0) { // too many opens
        f->refcnt--;
        rwlock_release(&f->lock);
        rwlock_release(&fs->fs_lock);
        return -EBUSY;
    }

    fio = kcalloc(1, sizeof(struct ngfs_io));
    fio->blockno = 0;
    fio->block = f->dentry.start_block;
    fio->write_idx = f->write_idx;
    fio->file = f;
    *ioptr = seekio_init(&fio->base, &ngfs_file_intf, 1, 1);
    rwlock_release(&f->lock);
    rwlock_release(&fs->fs_lock);

    debug("ngfs_open: SUCCESS - file=%s, refcnt=%d", name, f->refcnt);
    return 0;
}

void ngfs_reclaim(struct io * io) {
    trace("%s()", __func__);
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    
    rwlock_acquire(&f->lock, 1);
    assert(f->refcnt > 0);
    kfree(fio);
    f->refcnt--;
    rwlock_release(&f->lock);
}

long ngfs_fetch(
    struct io * io, unsigned long long pos, void * buf, long bufsz) 
{
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    struct ngfs * ngfs = f->fs;
    struct cache * cache = ngfs->cache;
    uint32_t bytes_read = 0;
    uint32_t offset, block, remaining_len, read_len;
    trace("%s(pos=%llu,bufsz=%ld,blockno=%lu,block=%lu)", 
        __func__, pos, bufsz, fio->blockno, fio->block);

    if (pos > UINT32_MAX) return -EINVAL;

    rwlock_acquire(&f->lock, 0);
    if (pos > f->dentry.size) {
        rwlock_release(&f->lock);
        return -EINVAL;
    }
    bufsz = MIN(bufsz, f->dentry.size - pos);
    
    // since we cache the block, we need to be careful if it changes
    update_pos(ngfs, fio, pos);

    block = fio->block;
    offset = pos % NGFS_BLKSZ;
    for (remaining_len = bufsz; remaining_len > 0; ) {
        assert(block != NGFS_BLOCK_END);
        
        read_len = MIN(remaining_len, NGFS_BLKSZ - offset);
        read_from_block(cache, IDX_TO_ABS(block), offset, 
            (uint8_t*)buf + bytes_read, read_len);
        
        remaining_len -= read_len;
        pos += read_len;
        bytes_read += read_len;
        offset = pos % NGFS_BLKSZ;
        if (offset == 0) block = get_next_data_block(cache, block);
    }
    
    rwlock_release(&f->lock);
    fio->block = block;
    fio->blockno = pos / NGFS_BLKSZ;

    return bytes_read;
}

long ngfs_store(
    struct io * io, unsigned long long pos, const void * buf, long len) 
{
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    struct ngfs * ngfs = f->fs;
    struct cache * cache = ngfs->cache;
    uint32_t bytes_written = 0;
    uint32_t offset, block, remaining_len, write_len;
    trace("%s(pos=%llu,blockno=%lu,block=%lu)", __func__, pos, fio->blockno, fio->block);

    if (pos > UINT32_MAX) return -EINVAL;

    rwlock_acquire(&f->lock, 1);
    if (pos + len > f->dentry.size) 
        update_size(ngfs, &f->dentry, pos + len);
    
    // since we cache the block, we need to be careful if it changes
    update_pos(ngfs, fio, pos);

    block = fio->block;
    offset = pos % NGFS_BLKSZ;
    for (remaining_len = len; remaining_len > 0; ) {
        assert(block != NGFS_BLOCK_END);
        
        write_len = MIN(remaining_len, NGFS_BLKSZ - offset);
        write_to_block(cache, IDX_TO_ABS(block), offset, 
            (uint8_t*)buf + bytes_written, write_len);
        
        remaining_len -= write_len;
        pos += write_len;
        bytes_written += write_len;
        offset = pos % NGFS_BLKSZ;
        if (offset == 0) block = get_next_data_block(cache, block);
    }
    
    rwlock_release(&f->lock);
    fio->block = block;
    fio->blockno = pos / NGFS_BLKSZ;

    return bytes_written;
}

int ngfs_create(struct filesystem * fs, const char * name) {
    trace("%s(%s)", __func__, name);
    uint32_t init_size, file_idx;
    struct ngfs_file * f;
    struct ngfs_dir_entry * root_entries;
    struct ngfs * ngfs = (void *)fs;
    struct cache * cache = ngfs->cache;
    struct ngfs_dir_entry * root_dir = &ngfs->root_dir;

    if (strlen(name) > NGFS_MAX_FILENAME_LEN || strlen(name) == 0)
        return -EINVAL;

    if (strcmp(name, root_dir->name) == 0)
        return -EACCESS;

    rwlock_acquire(&ngfs->fs_lock, 1);
    // Check if the file already exists
    for (f = ngfs->files_list; f != NULL; f = f->next) {
        if (strcmp(name, f->dentry.name) == 0) {
            rwlock_release(&ngfs->fs_lock);
            return -EEXIST;
        }
    }

    init_size = root_dir->size;
    file_idx = (init_size % NGFS_BLKSZ) / DENTRYSZ;

    if (update_size(ngfs, root_dir, init_size + DENTRYSZ)) {
        rwlock_release(&ngfs->fs_lock);
        return -ENOMEM;
    }

    root_entries = 
        get_cache_from_block(cache, 
        IDX_TO_ABS(blockno_to_block(cache, 
        root_dir, init_size / NGFS_BLKSZ)));

    // Add to root directory
    root_entries[file_idx].size = 0;
    root_entries[file_idx].start_block = NGFS_BLOCK_END;
    strncpy(root_entries[file_idx].name, name, NGFS_MAX_FILENAME_LEN);

    // Add to fslist
    f = kcalloc(1, sizeof(struct ngfs_file));
    memcpy(&f->dentry, &root_entries[file_idx], DENTRYSZ);
    cache_release(cache, (void *)root_entries, CACHE_DIRTY);

    rwlock_init(&f->lock, f->dentry.name);
    f->fs = ngfs;
    f->next = ngfs->files_list;
    if (f->next) f->next->prev = f;
    ngfs->files_list = f;

    update_root_dir(ngfs);
    cache_flush(cache);
    rwlock_release(&ngfs->fs_lock);

    return 0;
}

int ngfs_delete(struct filesystem * fs, const char * name) {
    struct ngfs_file * f;
    struct ngfs_dir_entry last_dentry;
    uint32_t block, last_block, last_offset, new_size;
    struct ngfs * ngfs = (void *)fs;
    struct cache * cache = ngfs->cache;
    struct ngfs_dir_entry * root_dir = &ngfs->root_dir;
    struct ngfs_dir_entry * dentry;

    if (strcmp(name, root_dir->name) == 0)
        return -EACCESS;

    rwlock_acquire(&ngfs->fs_lock, 1);
    // Remove from files list
    for (f = ngfs->files_list; f != NULL; f = f->next) {
        if (strcmp(name, f->dentry.name) == 0) {
            if (f->refcnt > 0) {
                rwlock_release(&ngfs->fs_lock);
                return -EBUSY;
            }

            if (f->next) f->next->prev = f->prev;  
            if (f->prev) f->prev->next = f->next;  
            if (f == ngfs->files_list) ngfs->files_list = f->next;
            break;
        }
    }

    if (f == NULL) {
        rwlock_release(&ngfs->fs_lock);
        return -ENOENT;
    }

    // Free data blocks associated with the file
    free_blocks(cache, f->dentry.start_block);
    kfree(f);

    // Get the last dentry 
    // we do this before entry checking to avoid cache conflicts
    new_size = root_dir->size - DENTRYSZ;
    last_block = blockno_to_block(cache, root_dir, new_size / NGFS_BLKSZ);
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
    rwlock_release(&ngfs->fs_lock);

    return 0;
}

int ngfs_ioctl(struct io * io, int op, void * arg) {
    int result;
    struct ngfs_io * fio = (void*)io;
    struct ngfs_file * f = fio->file;
    struct ngfs * ngfs = f->fs;
    unsigned long long * const ullarg = arg;
    trace("%s(op=%d)", __func__, op);

    if(arg == NULL)
        return -EINVAL;

    switch (op) {
        case IOC_GETPOS:
        case IOC_SETPOS:
            return seekio_ioctl(io, op, arg);

        case IOC_GETEND:
            *ullarg = f->dentry.size;
            return 0;

        case IOC_SETEND:
            if (*ullarg > UINT32_MAX) return -EINVAL;
            rwlock_acquire(&f->lock, 1);
            result = update_size(ngfs, &f->dentry, *ullarg);
            f->write_idx++;
            update_pos(ngfs, fio, fio->base.pos);
            rwlock_release(&f->lock);
            return result;

        default:
            return -ENOTSUP;
    }
}

void ngfs_flush(struct filesystem * fs) {
    trace("%s()", __func__);
    struct ngfs * ngfs = (void *)fs;
    struct ngfs_dir_entry * dentry = NULL;
    struct ngfs_file * f = ngfs->files_list;
    uint32_t block;

    rwlock_acquire(&ngfs->fs_lock, 1);
    if (f == NULL) goto flush_done;
    
    // technically this is unnecessary, since order doesnt matter
    // however for debugging purposes its easier to maintain order
    while (f->next) f = f->next; 

    // write all dentry info to root dir
    for (uint32_t idx = 1; 
        iterate_dentry(ngfs, &idx, &block, &dentry, CACHE_DIRTY);)
    {
        assert(f != NULL); // check consistency
        memcpy(dentry, &f->dentry, DENTRYSZ);
        f = f->prev;
    }
    assert(f == NULL); // check consistency

flush_done:
    rwlock_release(&ngfs->fs_lock);
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
    size_t len = MIN(bufsz, NGFS_MAX_FILENAME_LEN + 1);
    uint32_t block, offset, offset_bytes;

    rwlock_acquire(&ngfs->fs_lock, 0);
    if (ls->dir_idx >= root_dir->size / DENTRYSZ) {
        rwlock_release(&ngfs->fs_lock);
        return 0;
    }

    offset = ls->dir_idx % DENTRIES_PER_BLOCK;
    offset_bytes = offset * DENTRYSZ + offsetof(struct ngfs_dir_entry, name);
    block = blockno_to_block(
        cache, root_dir, ls->dir_idx / DENTRIES_PER_BLOCK);

    read_from_block(cache, IDX_TO_ABS(block), offset_bytes, buf, len);
    rwlock_release(&ngfs->fs_lock);
    
    debug("%s: reading listing %d (%s)", __func__, ls->dir_idx, (char *)buf);
    
    ls->dir_idx++;
    return len;
}

int update_size(
    struct ngfs * ngfs, 
    struct ngfs_dir_entry * dentry, 
    uint32_t end) 
{
    uint32_t new_block, block;
    struct cache * cache = ngfs->cache;
    // extend
    if (end > dentry->size) {
        block = blockno_to_block(cache, dentry, dentry->size / NGFS_BLKSZ);
        if (dentry->size % NGFS_BLKSZ != 0) {
            clear_block(cache, IDX_TO_ABS(block), 
            dentry->size % NGFS_BLKSZ, end - dentry->size);
            dentry->size = ROUND_UP(dentry->size, NGFS_BLKSZ);
        }

        while (CEIL(dentry->size, NGFS_BLKSZ) < CEIL(end, NGFS_BLKSZ)) {
            new_block = get_free_data_block(ngfs);
            if (new_block == NGFS_BLOCK_END) return -ENOMEM;
            clear_block(cache, IDX_TO_ABS(new_block), 0, end - dentry->size);
            if (dentry->size == 0) dentry->start_block = new_block;
            else set_next_data_block(cache, block, new_block);
            dentry->size += NGFS_BLKSZ; // for consistency
            block = new_block;
        }
    }
    // truncate
    else if (CEIL(end, NGFS_BLKSZ) < CEIL(dentry->size, NGFS_BLKSZ)) {
        block = blockno_to_block(cache, dentry, CEIL(end, NGFS_BLKSZ));
        free_blocks(cache, block);
        if (end == 0) dentry->start_block = NGFS_BLOCK_END;
        else set_next_data_block(cache, block, NGFS_BLOCK_END);
    }
    
    dentry->size = end;
    return 0;
}

void update_pos(struct ngfs * ngfs, struct ngfs_io * fio, uint32_t newpos) {
    struct cache * cache = ngfs->cache;
    if (newpos > fio->file->dentry.size) return;
    if (fio->write_idx != fio->file->write_idx || 
        fio->blockno > newpos / NGFS_BLKSZ)
    {
        fio->write_idx = fio->file->write_idx;
        fio->blockno = newpos / NGFS_BLKSZ;
        fio->block = blockno_to_block(cache, &fio->file->dentry, fio->blockno);
        return;
    }
    for (;fio->blockno < newpos / NGFS_BLKSZ; fio->blockno++)
        fio->block = get_next_data_block(cache, fio->block);
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
}

void * get_cache_from_block(struct cache * cache, uint32_t block) {
    void* ptr;
    if (cache_fetch(cache, block * NGFS_BLKSZ, &ptr))
        return NULL;
    return ptr;
}

uint32_t blockno_to_block(
    struct cache * cache, 
    struct ngfs_dir_entry * file, 
    unsigned int blockno) 
{
    uint32_t total_blocks, block;

    total_blocks = CEIL(file->size, NGFS_BLKSZ);

    // n too large -> get last block
    blockno = MIN(blockno, total_blocks-1);

    block = file->start_block;
    for (int bno = 0; bno < blockno; bno++) 
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
    if (dirty) cache_flush(cache);
    return 0;
}

void clear_block(struct cache * cache, uint32_t block, uint32_t offset, uint32_t len) {
    assert(offset < NGFS_BLKSZ);
    len = MIN(NGFS_BLKSZ - offset, len);
    void * data = get_cache_from_block(cache, block);
    memset((uint8_t *)data + offset, 0, len);
    cache_release(cache, data, CACHE_DIRTY);
}