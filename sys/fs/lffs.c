/*! @file lffs.c
    @brief LFFS Implementation.
    @copyright Copyright (c) 2025-2026 University of Illinois

*/

#include <stddef.h>
#include <stdint.h>
#ifdef LFFS_TRACE
#define TRACE
#endif

#ifdef LFFS_DEBUG
#define DEBUG
#endif

#include "lffs.h"

#include "cache.h"
#include "console.h"
#include "device.h"
#include "devimpl.h"
#include "error.h"
#include "filesys.h"
#include "fsimpl.h"
#include "heap.h"
#include "misc.h"
#include "string.h"
#include "thread.h"
#include "uio.h"
#include "uioimpl.h"

// INTERNAL TYPE DEFINITIONS
//

#define IDX_TO_ABS(block)       (num_fat_blocks+block)
#define DENTRYSZ                (sizeof(struct lffs_dir_entry))
#define FAT_ENTRYSZ             (sizeof(uint32_t))
#define DENTRIES_PER_BLOCK      (LFFS_BLKSZ / DENTRYSZ)

#define RM_FILE(file)                                   \
    do {                                                \
        if (file->next) file->next->prev = file->prev;  \
        if (file->prev) file->prev->next = file->next;  \
        if (file == files_list) files_list = file->next;\
        kfree(file);                                    \
    } while (0)

// TODO: add locks
struct lffs_file {
    struct lffs_dir_entry dentry;
    struct lffs_file * next;
    struct lffs_file * prev;

    unsigned long refcnt;
};

struct lffs_uio {
    struct uio uio;
    struct lffs_file * file;
    unsigned long pos;
    uint32_t block;
};

struct lffs_listing_uio {
    struct uio base;
    uint32_t dir_idx;
};

// INTERNAL FUNCTION DECLARATIONS
//

int lffs_open(struct filesystem * fs, const char * name, struct uio ** uioptr);
void lffs_close(struct uio* uio);
int lffs_cntl(struct uio* uio, int cmd, void* arg);
long lffs_fetch(struct uio* uio, void * buf, unsigned long len); 
long lffs_store(struct uio* uio, const void * buf, unsigned long len);
int lffs_create(struct filesystem * fs, const char* name);
int lffs_delete(struct filesystem * fs, const char* name);
void lffs_flush(struct filesystem * fs);

int lffs_getend(struct lffs_dir_entry * entry, void *arg);
int lffs_setend(struct lffs_dir_entry * entry, uint32_t end);
int lffs_setpos(struct lffs_uio * uio, uint32_t pos);
int lffs_getpos(struct lffs_uio * uio, void *arg);

int lffs_open_file(const char * name, struct uio ** uioptr);
int lffs_open_listing(struct uio ** uioptr);
void lffs_listing_close(struct uio * uio);
long lffs_listing_read (struct uio * uio, void * buf, unsigned long bufsz);

// Interal helper functions
static long read_from_block(uint32_t block, uint32_t offset, void* buf, long bufsz);
static long write_to_block(uint32_t block, uint32_t offset, void* buf, long bufsz);

static uint32_t get_free_data_block();
static void set_next_data_block(uint32_t block, uint32_t next);
static uint32_t get_next_data_block(uint32_t block);
static void free_blocks(uint32_t start);
static void * get_cache_from_block(uint32_t block);
static uint32_t get_nth_data_block(struct lffs_dir_entry * file, unsigned int n);
static inline long update_root_dir();
static struct lffs_dir_entry * find_dir_entry(const char * name);
static int free_associated_cache_block(struct lffs_dir_entry * entry, int dirty);


static inline int min(int a, int b) {
    return (a < b) ? a : b;
}

static inline int max(int a, int b) {
    return (a > b) ? a : b;
}


// INTERNAL GLOBAL VARIABLES

static struct cache * lffs_cache;

static struct lffs_file * files_list;

static unsigned int fs_size;

static uint32_t num_fat_blocks;

static struct lffs_dir_entry root_dir;

static const struct uio_intf file_intf = {
    .close = &lffs_close,
    .cntl = &lffs_cntl,
    .read = &lffs_fetch,
    .write = &lffs_store
};

static struct filesystem fs_intf = {
    .open = &lffs_open,
    .create = &lffs_create,
    .delete = &lffs_delete,
    .flush = &lffs_flush
};

static const struct uio_intf lffs_listing_uio_intf = {
    .close = &lffs_listing_close,
    .read = &lffs_listing_read
};

int mount_lffs(const char * name, struct cache * cache, unsigned int size) {
    if(cache == NULL)
        return -EINVAL;

    if(name == NULL)
        return -EINVAL;

    int result = attach_filesystem(name, &fs_intf);
    if(result != 0)
        return result;

    lffs_cache = cache;
    fs_size = size;
    num_fat_blocks = CEIL(size / LFFS_BLKSZ, LFFS_FAT_ENTRIES_PER_BLOCK);

    debug("Mounting filesystem with %d FAT blocks, %d total blocks\n", 
        num_fat_blocks, size / LFFS_BLKSZ);

    // root entry is always the first entry
    read_from_block(
        IDX_TO_ABS(LFFS_ROOT_DATA_BLOCK), 0, &root_dir, sizeof(root_dir));

    // well-formed fs
    assert(strncmp(root_dir.name, ".", LFFS_MAX_FILENAME_LEN) == 0);
    assert(root_dir.start_block == 0);
    assert(root_dir.size % DENTRYSZ == 0);

    debug("Starting with %d files\n", root_dir.size / DENTRYSZ);

    files_list = NULL;
    
    return 0;
}

int lffs_open(struct filesystem * fs, const char * name, struct uio ** uioptr) {
    trace("%s(%s,%p)", __func__, name, uioptr);
    if (name == NULL || *name == '\0')
        return lffs_open_listing(uioptr);
    else
        return lffs_open_file(name, uioptr);
}

int lffs_open_file(const char * name, struct uio ** uioptr) {
    struct lffs_uio * fuio;
    struct lffs_file * f;
    struct lffs_dir_entry * dentry;
    trace("%s(%s,%p)", __func__, name, uioptr);

    if (strncmp(name, root_dir.name, LFFS_MAX_FILENAME_LEN) == 0)
        return -EACCESS;

    for(f = files_list; f != NULL; f = f->next) {
        // found file in filesystem
        if (strncmp(name, f->dentry.name, LFFS_MAX_FILENAME_LEN) == 0){
            break;
        }
    }

    if (f == NULL) {
        dentry = find_dir_entry(name);

        if (dentry == NULL) return -ENOENT;

        f = kcalloc(1, sizeof(struct lffs_file));
        memcpy(&f->dentry, dentry, DENTRYSZ);
        free_associated_cache_block(dentry, CACHE_CLEAN);

        f->next = files_list;
        if (files_list) files_list->prev = f;
        files_list = f;
    }


    f->refcnt++;

    fuio = kcalloc(1, sizeof(struct lffs_uio));
    fuio->block = f->dentry.start_block;
    fuio->file = f;
    uio_init1(&fuio->uio, &file_intf);
    debug("lffs_open: SUCCESS - file=%s, pos=0, refcnt=1", name);
    
    *uioptr = &fuio->uio;
    return 0;
}

void lffs_close(struct uio * uio) {
    trace("%s()", __func__);
    struct lffs_uio * fuio = (void*)uio - offsetof(struct lffs_uio, uio);
    struct lffs_file * f = fuio->file;
    struct lffs_dir_entry * fdentry = &f->dentry;
    struct lffs_dir_entry * dentry = find_dir_entry(fdentry->name);
    
    // update dentry (if needed)
    if (dentry->size != fdentry->size ||
        dentry->start_block != fdentry->start_block) {
        debug("Updating dentry for file=%s, old size=%ld, new size=%ld",
            fdentry->name, dentry->size, fdentry->size);
        
        dentry->start_block = fdentry->start_block;
        dentry->size = fdentry->size;
        free_associated_cache_block(dentry, CACHE_DIRTY);
        cache_flush(lffs_cache);
    }
    else {
        free_associated_cache_block(dentry, CACHE_CLEAN);
    }

    kfree(fuio);
    assert(f->refcnt > 0);
    f->refcnt--;
    if (f->refcnt == 0) {
        RM_FILE(f);
    }
}

long lffs_fetch(struct uio * uio, void * buf, unsigned long len) {
    trace("%s(%p,%ld)", __func__, buf, len);
    
    // Check for bad inputs
    if (!uio || !buf || len < 0) {
        return -EINVAL;
    }
    
    struct lffs_uio* fuio = (void*)uio - offsetof(struct lffs_uio, uio);
    struct lffs_file* f = fuio->file;
    long pos = fuio->pos;
    uint32_t bytes_read = 0;
    uint32_t block;
    uint32_t offset, remaining_len, read_len;
    
    // since we cache the block, we need to be careful if it changes
    block = (fuio->block != LFFS_BLOCK_END) ? fuio->block :
        get_nth_data_block(&f->dentry, pos / LFFS_BLKSZ);

    // truncate len
    len = min(len, f->dentry.size - pos);

    for (remaining_len = len; remaining_len > 0; 
        block = get_next_data_block(block)) {
        assert(block != LFFS_BLOCK_END);
        
        offset = pos % LFFS_BLKSZ;
        read_len = min(remaining_len, LFFS_BLKSZ - offset);
        read_from_block(
            IDX_TO_ABS(block), offset, (uint8_t*)buf + bytes_read, read_len);

        remaining_len -= read_len;
        pos += read_len;
        bytes_read += read_len;
        fuio->block = block;
    }

    fuio->pos = pos;

    return bytes_read;
}

long lffs_store(struct uio * uio, const void * buf, unsigned long len) {
    trace("%s(%p,%ld)", __func__, buf, len);
    
    // Check for bad inputs
    if (!uio || !buf || len < 0) {
        return -EINVAL;
    }
    
    struct lffs_uio* fuio = (void*)uio - offsetof(struct lffs_uio, uio);
    struct lffs_file* f = fuio->file;
    long pos = fuio->pos;
    uint32_t bytes_written = 0;
    uint32_t block, start_block;
    uint32_t offset, remaining_len, write_len;
    
    // extend file size of necessary
    if (f->dentry.size - pos < len) {
        int result = lffs_setend(&f->dentry, pos + len);
        if (result != 0)
            return result;
    }

    // since we cache the block, we need to be careful if it changes
    start_block = (fuio->block != LFFS_BLOCK_END) ? fuio->block :
        get_nth_data_block(&f->dentry, pos / LFFS_BLKSZ);
    
    remaining_len = len;

    for (block = start_block; 
        block != LFFS_BLOCK_END && remaining_len > 0; 
        block = get_next_data_block(block)) {
        
        offset = pos % LFFS_BLKSZ;
        write_len = min(remaining_len, LFFS_BLKSZ - offset);
        write_to_block(
            IDX_TO_ABS(block), offset, (uint8_t*)buf+bytes_written, write_len);

        remaining_len -= write_len;
        pos += write_len;
        bytes_written += write_len;
    }

    fuio->pos = pos;
    fuio->block = block;

    return bytes_written;
}

int lffs_create(struct filesystem * fs, const char * name) {
    uint32_t file_idx;
    struct lffs_dir_entry * root_entries;
    uint32_t curr_size = root_dir.size;
    if (!name || strlen(name) > LFFS_MAX_FILENAME_LEN) {
        return -EINVAL;
    }

    // Check if the file already exists
    // if the cache block was freed successfully, an entry was found
    if (free_associated_cache_block(find_dir_entry(name), CACHE_CLEAN) == 0)
        return -EMFILE;

    file_idx = (curr_size % LFFS_BLKSZ) / DENTRYSZ;

    if (lffs_setend(&root_dir, curr_size + DENTRYSZ))
        return -ENODATABLKS;

    root_entries = (struct lffs_dir_entry *)get_cache_from_block(
        get_nth_data_block(&root_dir, curr_size / LFFS_BLKSZ));

    // Add to root directory
    root_entries[file_idx].size = 0;
    root_entries[file_idx].start_block = LFFS_BLOCK_END;
    strncpy(root_entries[file_idx].name, name, LFFS_MAX_FILENAME_LEN);
    cache_release_block(lffs_cache, (void *)root_entries, CACHE_DIRTY);

    update_root_dir();

    // might not need this?
    cache_flush(lffs_cache);

    return 0;
}

int lffs_delete(struct filesystem * fs, const char * name) {
    struct lffs_dir_entry * dentry;
    struct lffs_dir_entry last_dentry;
    uint32_t last_block, last_offset, new_size;

    if (strncmp(name, root_dir.name, LFFS_MAX_FILENAME_LEN) == 0)
        return -EACCESS;

    // Remove from files list (if its there)
    for (struct lffs_file * f = files_list; f != NULL; f = f->next) {
        if (strncmp(name, f->dentry.name, LFFS_MAX_FILENAME_LEN)==0) {
            if (f->refcnt > 0) return -EBUSY;
            RM_FILE(f);
            break;
        }
    }

    // Get the last dentry 
    // we do this before entry checking to avoid cache conflicts
    new_size = root_dir.size - DENTRYSZ;
    last_block = get_nth_data_block(&root_dir, new_size / LFFS_BLKSZ);
    last_offset = new_size % LFFS_BLKSZ;
    read_from_block(
        IDX_TO_ABS(last_block), last_offset, &last_dentry, DENTRYSZ);

    assert(last_dentry.name[0] != '\0'); // real entry
    
    dentry = find_dir_entry(name);
    if (dentry == NULL) return -ENOENT;

    // Free data blocks associated with the file
    free_blocks(dentry->start_block);

    // Overwrite dir entry
    memcpy(dentry, &last_dentry, sizeof(last_dentry));
    free_associated_cache_block(dentry, CACHE_DIRTY);

    lffs_setend(&root_dir, new_size);
    update_root_dir();

    // Ensure that file deletion is flushed to the backing device
    cache_flush(lffs_cache);

    return 0;
}

int lffs_cntl(struct uio *uio, int cmd, void *arg) {
    struct lffs_uio * curr_uio = (void*) uio - offsetof(struct lffs_uio, uio);
    struct lffs_file * f = curr_uio->file;

    if(arg == NULL)
        return -EINVAL;

    switch (cmd) {
        case FCNTL_GETEND:
            return lffs_getend(&f->dentry, arg);
        case FCNTL_SETEND:
            return lffs_setend(&f->dentry, *(uint32_t *)arg);
        case FCNTL_SETPOS:
            return lffs_setpos(curr_uio, *(uint32_t *)arg);
        case FCNTL_GETPOS:
            return lffs_getpos(curr_uio, arg);
        default:
            return -EINVAL;
    }
}

int lffs_getend(struct lffs_dir_entry * entry, void *arg) {
    *((unsigned long long*)arg) = entry->size;
    return 0;
}

int lffs_setend(struct lffs_dir_entry * entry, uint32_t end) {
    uint32_t num_new_blocks, block, new_block;

    // check if any blocks allocated yet
    if (entry->start_block == LFFS_BLOCK_END) {
        block = get_free_data_block();
        if (block == LFFS_BLOCK_END) return -ENODATABLKS;
        entry->start_block = block;
    }
    else {
        block = get_nth_data_block(entry, end / LFFS_BLKSZ);
    }

    if (end > entry->size) { // extend
        num_new_blocks = end / LFFS_BLKSZ - entry->size / LFFS_BLKSZ;
        while (num_new_blocks-- > 0) {
            new_block = get_free_data_block();
            if (new_block == LFFS_BLOCK_END) return -ENODATABLKS;
            set_next_data_block(block, new_block);
        }
    }
    else { // truncate
        free_blocks(get_next_data_block(block));
        set_next_data_block(block, LFFS_BLOCK_END);
    }
    
    entry->size = end;
    return 0;
}

int lffs_setpos(struct lffs_uio * uio, uint32_t pos) {
    if (pos > uio->file->dentry.size) return -EBADFMT;
    uio->pos = pos;
    uio->block = get_nth_data_block(&uio->file->dentry, pos / LFFS_BLKSZ);
    return 0;
}

int lffs_getpos(struct lffs_uio * uio, void *arg) {
    *((unsigned long*)arg) = uio->pos;
    return 0;
}

void lffs_flush(struct filesystem * fs) {
    struct lffs_dir_entry * root_entries;
    uint32_t remaining_files = root_dir.size / DENTRYSZ;
    uint32_t block = root_dir.start_block;
    int dirty;

    while (block != LFFS_BLOCK_END && files_list != NULL) {
        root_entries = (struct lffs_dir_entry *)get_cache_from_block(block);
        dirty = CACHE_CLEAN;

        for(int i = 0; i < min(remaining_files, DENTRIES_PER_BLOCK); i++) {
            for (struct lffs_file * f = files_list; f != NULL; f = f->next) {
                if (strncmp(f->dentry.name, root_entries[i].name,
                    LFFS_MAX_FILENAME_LEN) != 0)
                    continue;

                if (root_entries[i].size != f->dentry.size) {
                    memcpy(&root_entries[i], &f->dentry, sizeof(f->dentry));
                    dirty = CACHE_DIRTY;
                }
                RM_FILE(f);
                break;
            }
        }

        cache_release_block(lffs_cache, (void *)root_entries, dirty);
        block = get_next_data_block(block);
        remaining_files -= DENTRIES_PER_BLOCK;
    }

    // ensure all files were removed
    assert(files_list == NULL);
    
    cache_flush(lffs_cache);
    return;
}

int lffs_open_listing(struct uio ** uioptr) {
    struct lffs_listing_uio * ls;
    trace("%s(%p)", __func__, uioptr);

    ls = kcalloc(1, sizeof(struct lffs_listing_uio));
    uio_init1(&ls->base, &lffs_listing_uio_intf);
    *uioptr = &ls->base;

    return 0;
}

void lffs_listing_close(struct uio * uio) {
    struct lffs_listing_uio * const ls = (struct lffs_listing_uio*)uio;
    kfree(ls);
}

long lffs_listing_read(struct uio * uio, void * buf, unsigned long bufsz) {
    struct lffs_listing_uio * const ls = (struct lffs_listing_uio*)uio;
    uint32_t total_files = root_dir.size / DENTRYSZ;
    size_t len = min(bufsz, LFFS_MAX_FILENAME_LEN);
    uint32_t block, offset, offset_bytes;

    if (ls->dir_idx >= total_files) return 0;

    offset = ls->dir_idx % DENTRIES_PER_BLOCK;
    offset_bytes = offset * DENTRYSZ + offsetof(struct lffs_dir_entry, name);
    block = get_nth_data_block(&root_dir, ls->dir_idx / DENTRIES_PER_BLOCK);

    read_from_block(IDX_TO_ABS(block), offset_bytes, buf, len);
    
    debug("%s: reading listing %d (%s)", __func__, ls->dir_idx, (char *)buf);
    
    ls->dir_idx++;
    return len;
}

long read_from_block(uint32_t block, uint32_t offset, void* buf, long bufsz) {
    void * block_data;

    assert(offset + bufsz <= LFFS_BLKSZ); // bad requests
    assert(block != LFFS_BLOCK_END);
    trace("%s(block=%lu,off=%lu,%p,%ld)", __func__, block, offset, buf, bufsz);
    
    if (cache_get_block(lffs_cache, block * LFFS_BLKSZ, &block_data) != 0) {
        return -1;
    }

    memcpy(buf, (uint8_t*)block_data + offset, bufsz);
    cache_release_block(lffs_cache, block_data, CACHE_CLEAN);

    return bufsz;
}

long write_to_block(uint32_t block, uint32_t offset, void* buf, long bufsz) {
    void * block_data;

    assert(offset + bufsz <= LFFS_BLKSZ); // bad requests
    assert(block != LFFS_BLOCK_END);
    trace("%s(block=%lu,off=%lu,%p,%ld)", __func__, block, offset, buf, bufsz);
    
    if (cache_get_block(lffs_cache, block * LFFS_BLKSZ, &block_data) != 0) {
        return -1;
    }

    memcpy((uint8_t*)block_data + offset, buf, bufsz);
    cache_release_block(lffs_cache, block_data, CACHE_DIRTY);

    return bufsz;
}

uint32_t get_free_data_block() {
    struct lffs_fat * fat = NULL;
    uint32_t free_block;

    for (uint32_t fat_block = 0; fat_block < num_fat_blocks; fat_block++) {
        assert(0 == cache_get_block(lffs_cache, 
            fat_block * LFFS_BLKSZ, (void**)&fat));

        for (int idx = 0; idx < LFFS_FAT_ENTRIES_PER_BLOCK; idx++) {
            if (fat->fat[idx] == LFFS_BLOCK_FREE) {
                fat->fat[idx] = LFFS_BLOCK_END;
                cache_release_block(lffs_cache, (void *)fat, CACHE_DIRTY);

                // this line is needed bc it breaks otherwise
                // i think this means theres a race condition
                // in the cache? not sure though
                cache_flush(lffs_cache);

                free_block = fat_block * LFFS_FAT_ENTRIES_PER_BLOCK + idx;
                return (free_block < fs_size / LFFS_BLKSZ - num_fat_blocks) ?
                    free_block : LFFS_BLOCK_END;
            }
        }

        cache_release_block(lffs_cache, (void *)fat, CACHE_CLEAN);
    }

    // no free blocks
    return LFFS_BLOCK_END;
}

void set_next_data_block(uint32_t block, uint32_t next) {
    assert(block != LFFS_BLOCK_END);
    write_to_block(block / LFFS_FAT_ENTRIES_PER_BLOCK, 
        (block % LFFS_FAT_ENTRIES_PER_BLOCK) * FAT_ENTRYSZ, 
        &next, sizeof(next));
    // this line is needed as well
    // i think this is also related to the cache race cond
    cache_flush(lffs_cache);
}

uint32_t get_next_data_block(uint32_t block) {
    uint32_t next;
    assert(block != LFFS_BLOCK_END);
    read_from_block(block / LFFS_FAT_ENTRIES_PER_BLOCK, 
        (block % LFFS_FAT_ENTRIES_PER_BLOCK) * FAT_ENTRYSZ, 
        &next, sizeof(next));
    return next;
}

void free_blocks(uint32_t start) {
    uint32_t next;
    uint32_t cur = start;
    while (cur != LFFS_BLOCK_END) {
        assert(cur != LFFS_BLOCK_FREE);
        next = get_next_data_block(cur);
        set_next_data_block(cur, LFFS_BLOCK_FREE);
        cur = next;
    }
    cache_flush(lffs_cache);
}

void* get_cache_from_block(uint32_t block) {
    void* ptr;
    if (cache_get_block(lffs_cache, IDX_TO_ABS(block) * LFFS_BLKSZ, &ptr))
        return NULL;
    return ptr;
}

uint32_t get_nth_data_block(struct lffs_dir_entry * file, unsigned int n) {
    uint32_t total_blocks, block;

    total_blocks = file->size / LFFS_BLKSZ;

    // n too large -> get last block
    n = min(n, total_blocks);

    block = file->start_block;
    for (int bno = 0; bno < n; bno++) 
        block = get_next_data_block(block);

    return block;
}

inline long update_root_dir() {
    return write_to_block(
        IDX_TO_ABS(LFFS_ROOT_DATA_BLOCK), 0, &root_dir, DENTRYSZ);
}

struct lffs_dir_entry * find_dir_entry(const char * name) {
    trace("%s(%s)", __func__, name);
    uint32_t block = root_dir.start_block;
    struct lffs_dir_entry * root_entries;
    uint32_t remaining_files = root_dir.size / DENTRYSZ;

    while (block != LFFS_BLOCK_END) {
        root_entries = (struct lffs_dir_entry *)get_cache_from_block(block);

        for(int i = 0; i < min(remaining_files, DENTRIES_PER_BLOCK); i++) {
            if(strncmp(name, root_entries[i].name, 
                LFFS_MAX_FILENAME_LEN) == 0) {
                
                return &root_entries[i];
            }
        }

        cache_release_block(lffs_cache, (void *)root_entries, CACHE_CLEAN);
        block = get_next_data_block(block);
        remaining_files -= DENTRIES_PER_BLOCK;
    }

    // not found
    return NULL;
}

int free_associated_cache_block(struct lffs_dir_entry * entry, int dirty) {
    if (entry == NULL) return -1;
    cache_release_block(lffs_cache, 
        (void *)ROUND_DOWN((uintptr_t)entry, LFFS_BLKSZ), dirty);
    return 0;
}
