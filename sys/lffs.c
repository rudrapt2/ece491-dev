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

#define BLOCK_NO_TO_POS(block)  ((num_fat_blocks+block)*LFFS_BLKSZ)
#define DENTRIES_PER_BLOCK      (LFFS_BLKSZ / sizeof(struct lffs_dir_entry))

// TODO: add locks
struct lffs_file {
    struct lffs_dir_entry dentry;
    struct lffs_file * next;

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
static long arbitrary_read(unsigned long pos, void * buf, long bufsz);
static long arbitrary_write(unsigned long pos, void* buf, long bufsz);

static uint32_t get_free_data_block();
static long set_next_data_block(uint32_t block, uint32_t next);
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

    arbitrary_read(BLOCK_NO_TO_POS(LFFS_ROOT_DATA_BLOCK), 
        &root_dir, 
        sizeof(root_dir));

    // well-formed fs
    assert(strncmp(root_dir.name, ".", LFFS_MAX_FILENAME_LEN) == 0);
    assert(root_dir.start_block == 0);
    assert(root_dir.size % sizeof(struct lffs_dir_entry) == 0);

    debug("Starting with %d files\n", 
        root_dir.size / sizeof(struct lffs_dir_entry));

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
    struct lffs_uio * file_uio;
    struct lffs_file * curr_file;
    struct lffs_dir_entry * dentry;
    trace("%s(%s,%p)", __func__, name, uioptr);

    for(curr_file = files_list; curr_file != NULL; 
        curr_file = curr_file->next) {
        // found file in filesystem
        if (strncmp(name, curr_file->dentry.name, LFFS_MAX_FILENAME_LEN) == 0){
            debug("lffs_open: file=%s, refcnt=%d", 
                name, curr_file->refcnt);
            break;
        }
    }

    if (curr_file == NULL) {
        dentry = find_dir_entry(name);

        if (dentry == NULL) return -ENOENT;

        curr_file = kcalloc(1, sizeof(struct lffs_file));
        memcpy(&curr_file->dentry, dentry, sizeof(struct lffs_dir_entry));
        free_associated_cache_block(dentry, CACHE_CLEAN);

        curr_file->next = files_list;
        files_list = curr_file;
    }


    curr_file->refcnt++;

    file_uio = kcalloc(1, sizeof(struct lffs_uio));
    file_uio->block = curr_file->dentry.start_block;
    file_uio->file = curr_file;
    uio_init1(&file_uio->uio, &file_intf);
    debug("lffs_open: SUCCESS - file=%s, pos=0, refcnt=1", name);
    
    *uioptr = &file_uio->uio;
    return 0;
}

void lffs_close(struct uio * uio) {
    struct lffs_uio * curr_uio = (void*)uio - offsetof(struct lffs_uio, uio);
    struct lffs_file * curr_file = curr_uio->file;
    struct lffs_dir_entry * curr_dentry = &curr_file->dentry;
    struct lffs_dir_entry * dentry = find_dir_entry(curr_dentry->name);
    
    debug("lffs_close: file=%s, pos=%ld, refcnt=%d",
          curr_dentry->name, curr_uio->pos, curr_file->refcnt);
    
    // update dentry (if needed)
    if (dentry->size != curr_dentry->size ||
        dentry->start_block != curr_dentry->start_block) {
        debug("Updating dentry for file=%s, old size=%ld, new size=%ld",
            curr_dentry->name, dentry->size, curr_dentry->size);
        
        dentry->start_block = curr_dentry->start_block;
        dentry->size = curr_dentry->size;
        free_associated_cache_block(dentry, CACHE_DIRTY);
    }
    else {
        free_associated_cache_block(dentry, CACHE_CLEAN);
    }

    kfree(curr_uio);
    assert(curr_file->refcnt > 0);
    curr_file->refcnt--;
}

long lffs_fetch(struct uio * uio, void * buf, unsigned long len) {
    long total_num_bytes_to_read = len;
    trace("%s(%p,%ld)", __func__, buf, len);
    
    // Check for bad inputs
    if (!uio || !buf || len < 0) {
        return -EINVAL;
    }
    
    struct lffs_uio* curr_uio = (void*)uio - offsetof(struct lffs_uio, uio);
    struct lffs_file* curr_file = curr_uio->file;
    long pos = curr_uio->pos;
    
    // we can not read past the end of the file, so if len is too large, total_num_bytes_to_read is modified appropriately
    total_num_bytes_to_read = min(total_num_bytes_to_read, curr_file->dentry.size - pos);
  
    // cur_data_block: the index of the actual data block
    uint32_t offset;
    uint32_t cur_data_block = curr_uio->block;
    long num_bytes_left_to_read = total_num_bytes_to_read;
    long num_bytes_read = 0;
    while (num_bytes_left_to_read > 0){
        assert(cur_data_block != LFFS_BLOCK_END);
        offset = pos % LFFS_BLKSZ;

        // we are in the data block in which pos is located
        // read until a block edge or read all the bytes left, which ever comes first.
        uint32_t read_len = min(num_bytes_left_to_read, LFFS_BLKSZ-offset);
        uint32_t address_to_start_read = BLOCK_NO_TO_POS(cur_data_block) + offset;
        arbitrary_read(address_to_start_read, (uint8_t*)buf + num_bytes_read, read_len);

        num_bytes_read += read_len;
        num_bytes_left_to_read -= read_len;
        pos += read_len;
        if (pos % LFFS_BLKSZ == 0) 
            cur_data_block = get_next_data_block(cur_data_block);
    }
    curr_uio->pos = pos;
    curr_uio->block = cur_data_block;

    return num_bytes_read;
}

long lffs_store(struct uio * uio, const void * buf, unsigned long len){
    struct lffs_uio * curr_uio = (void*) uio - offsetof(struct lffs_uio, uio);
    struct lffs_file * f = curr_uio->file;
    long pos = curr_uio->pos;
    uint32_t cur_data_block = curr_uio->block;
    int result;

    // extend file size of necessary
    if (f->dentry.size - pos < len) {
        result = lffs_setend(&f->dentry, pos + len);
        if (result != 0)
            return result;

        // check if start block updated
        if (cur_data_block == LFFS_BLOCK_END)
            cur_data_block = f->dentry.start_block;
    }

    long num_bytes_left_to_write = len;
    long num_bytes_written = 0;
    while (num_bytes_left_to_write > 0){
        assert(cur_data_block != LFFS_BLOCK_END);
        uint32_t offset = pos % LFFS_BLKSZ;
        uint32_t write_len = min(num_bytes_left_to_write, LFFS_BLKSZ - offset);

        uint64_t address_to_start_write = BLOCK_NO_TO_POS(cur_data_block) + offset;
        arbitrary_write(address_to_start_write, (uint8_t*)buf + num_bytes_written, write_len);
        num_bytes_written += write_len;
        num_bytes_left_to_write -= write_len;
        pos += write_len;
        if (pos % LFFS_BLKSZ == 0) 
            cur_data_block = get_next_data_block(cur_data_block);
    }
    curr_uio->pos = pos;
    curr_uio->block = cur_data_block;
    return num_bytes_written;
}

int lffs_create(struct filesystem * fs, const char * name) {
    uint32_t new_size, file_idx;
    struct lffs_dir_entry * root_entries;
    uint32_t curr_size = root_dir.size;
    if (!name || strlen(name) > LFFS_MAX_FILENAME_LEN) {
        return -EINVAL;
    }

    // Check if the file already exists
    // if the cache block was freed successfully, an entry was found
    if (free_associated_cache_block(find_dir_entry(name), CACHE_CLEAN) == 0)
        return -EMFILE;

    file_idx = curr_size / sizeof(struct lffs_dir_entry);
    file_idx %= DENTRIES_PER_BLOCK;

    if (lffs_setend(&root_dir, curr_size + sizeof(struct lffs_dir_entry)))
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
    struct lffs_file ** indirect;
    struct lffs_file * tmp;
    struct lffs_dir_entry * dentry;
    struct lffs_dir_entry last_dentry;
    uint32_t last_block, last_offset, new_size;

    if (strncmp(name, root_dir.name, LFFS_MAX_FILENAME_LEN) == 0)
        return -EACCESS;

    // Remove from files list (if its there)
    for (indirect = &files_list; *indirect != NULL; 
        indirect = &(*indirect)->next) {

        if (strncmp(name, (*indirect)->dentry.name, 
            LFFS_MAX_FILENAME_LEN) == 0) {

            if ((*indirect)->refcnt > 0) return -EBUSY;

            tmp = (*indirect)->next;
            kfree(*indirect);
            *indirect = tmp;
            break;
        }
    }

    // Get the last dentry 
    // we do this before entry checking to avoid cache conflicts
    new_size = root_dir.size - sizeof(struct lffs_dir_entry);
    last_block = get_nth_data_block(&root_dir, new_size / LFFS_BLKSZ);
    last_offset = new_size % LFFS_BLKSZ;
    arbitrary_read(BLOCK_NO_TO_POS(last_block) + last_offset, 
        &last_dentry, sizeof(last_dentry));

    if (last_dentry.name[0] == '\0')
        panic(NULL);
    
    dentry = find_dir_entry(name);
    if (dentry == NULL) return -ENOENT;

    // Free data blocks associated with the file
    free_blocks(dentry->start_block);

    // Overwrite last root dir entry
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
    uint32_t remaining_files = root_dir.size / sizeof(struct lffs_dir_entry);
    uint32_t block = root_dir.start_block;
    int dirty;

    while (block != LFFS_BLOCK_END) {
        root_entries = (struct lffs_dir_entry *)get_cache_from_block(block);
        dirty = CACHE_CLEAN;

        for(int i = 0; i < min(remaining_files, DENTRIES_PER_BLOCK); i++) {
            for (struct lffs_file * f = files_list; f != NULL; f = f->next) {
                if(strncmp(f->dentry.name, root_entries[i].name,
                    LFFS_MAX_FILENAME_LEN) == 0 &&
                    root_entries[i].size != f->dentry.size) {

                    memcpy(&root_entries[i], &f->dentry, sizeof(f->dentry));
                    dirty = CACHE_DIRTY;
                }
            }
        }

        cache_release_block(lffs_cache, (void *)root_entries, dirty);
        block = get_next_data_block(block);
        remaining_files = min(remaining_files, 
            remaining_files - DENTRIES_PER_BLOCK);
    }
    
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
    uint32_t total_files = root_dir.size / sizeof(struct lffs_dir_entry);
    size_t len = min(bufsz, LFFS_MAX_FILENAME_LEN);
    uint32_t block, offset;
    struct lffs_dir_entry dentry;

    if (ls->dir_idx >= total_files) return 0;

    offset = ls->dir_idx % DENTRIES_PER_BLOCK;
    block = get_nth_data_block(&root_dir, ls->dir_idx / DENTRIES_PER_BLOCK);

    arbitrary_read(
        BLOCK_NO_TO_POS(block) + offset*sizeof(struct lffs_dir_entry), 
        &dentry, sizeof(struct lffs_dir_entry));
    
    debug("lffs_listing_read: reading listing %d (%s)", 
        ls->dir_idx, dentry.name);
    
    ls->dir_idx++;
    strncpy(buf, dentry.name, len);
    return len;
}

long arbitrary_read(unsigned long pos, void* buf, long bufsz) {
    trace("%s(0x%llx,%p,%ld)", __func__, pos, buf, bufsz);

    // Null checks
    if (bufsz <= 0 || !buf) {
        return 0;
    }
    
    // Get the size of the filesystem and truncate the read if necessary
    const unsigned long bytes_to_read = (pos >= fs_size) ? 0 : min(bufsz, fs_size - pos);
    unsigned long bytes_remaining = bytes_to_read;

    while (bytes_remaining > 0) {
        const uint64_t block_index = pos / LFFS_BLKSZ;
        const uint64_t block_offset = pos % LFFS_BLKSZ;
        void* block_data = NULL;
        
        if (cache_get_block(lffs_cache, block_index * LFFS_BLKSZ, &block_data) != 0) {
            return -1;
        }

        const uint64_t bytes_available = LFFS_BLKSZ - block_offset;
        const uint64_t bytes_to_copy = min(bytes_remaining, bytes_available);

        memcpy(buf, (uint8_t*)block_data + block_offset, bytes_to_copy);
        
        buf += bytes_to_copy;
        pos += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;

        cache_release_block(lffs_cache, block_data, CACHE_CLEAN);
    }

    return bytes_to_read - bytes_remaining;
}

long arbitrary_write(unsigned long pos, void* buf, long len) {
    trace("%s(0x%llx,%p,%ld)", __func__, pos, buf, len);

    // Null checks
    if (len <= 0 || !buf) {
        return 0;
    }
    
    // Get the size of the filesystem and truncate the write if necessary
    const unsigned long bytes_to_write = (pos >= fs_size) ? 0 : min(len, fs_size - pos);
    unsigned long bytes_remaining = bytes_to_write;

    while (bytes_remaining > 0) {
        const uint64_t block_index = pos / LFFS_BLKSZ;
        const uint64_t block_offset = pos % LFFS_BLKSZ;
        void* block_data = NULL;
        
        if (cache_get_block(lffs_cache, block_index * LFFS_BLKSZ, &block_data) != 0) {
            return -1;
        }

        const uint64_t bytes_available = LFFS_BLKSZ - block_offset;
        const uint64_t bytes_to_copy = min(bytes_remaining, bytes_available);

        memcpy((uint8_t*)block_data + block_offset, buf, bytes_to_copy);
        
        buf += bytes_to_copy;
        pos += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;

        cache_release_block(lffs_cache, block_data, CACHE_DIRTY);
    }

    return bytes_to_write - bytes_remaining;
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

long set_next_data_block(uint32_t block, uint32_t next) {
    assert(block != LFFS_BLOCK_END);
    return arbitrary_write(block * sizeof(uint32_t), &next, sizeof(next));
}

uint32_t get_next_data_block(uint32_t block) {
    uint32_t next;
    assert(block != LFFS_BLOCK_END);
    arbitrary_read(block * sizeof(uint32_t), &next, sizeof(next));
    return next;
}

void free_blocks(uint32_t start) {
    uint32_t next;
    uint32_t cur = start;
    while (cur != LFFS_BLOCK_END) {
        if (cur == LFFS_BLOCK_FREE)
            panic(NULL);
        next = get_next_data_block(cur);
        set_next_data_block(cur, LFFS_BLOCK_FREE);
        cur = next;
    }
}

void* get_cache_from_block(uint32_t block) {
    void* ptr;
    if (cache_get_block(lffs_cache, 
        BLOCK_NO_TO_POS(block), 
        &ptr))
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
    return arbitrary_write(BLOCK_NO_TO_POS(LFFS_ROOT_DATA_BLOCK), 
        &root_dir, sizeof(root_dir));
}

struct lffs_dir_entry * find_dir_entry(const char * name) {
    uint32_t block = root_dir.start_block;
    struct lffs_dir_entry * root_entries;
    uint32_t remaining_files = root_dir.size / sizeof(struct lffs_dir_entry);

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
        remaining_files = min(remaining_files, 
            remaining_files - DENTRIES_PER_BLOCK);
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
