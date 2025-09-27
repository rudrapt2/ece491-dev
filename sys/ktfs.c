#ifdef KTFS_TRACE
#define TRACE
#endif

#ifdef KTFS_DEBUG
#define DEBUG
#endif

#include "heap.h"
#include "filesys.h"
#include "uioimpl.h"
#include "ktfs.h"
#include "error.h"
#include "thread.h"
#include "string.h"
#include "console.h"
#include "cache.h"
#include "uio.h"
#include "device.h"
#include "devimpl.h"
#include "fsimpl.h"
#include "misc.h"

// INTERNAL TYPE DEFINITIONS
//

// struct ktfs_free_inode_elem {
//     uint16_t inode_index;
//     struct ktfs_free_inode_elem * next; 
// };

struct ktfs_file {
    struct uio uio;
    struct ktfs_dir_entry dentry;
    struct ktfs_file * prev;
    struct ktfs_file * next;
    uint32_t file_size;
    uint8_t flag;
    unsigned long pos;// add pos
};

// INTERNAL FUNCTION DECLARATIONS
//

int ktfs_open(struct filesystem * fs, const char * name, struct uio ** uioptr);
void ktfs_close(struct uio* uio);
int ktfs_cntl(struct uio* uio, int cmd, void* arg);
long ktfs_fetch(struct uio* uio, void * buf, unsigned long len); 
long ktfs_store(struct uio* uio, const void * buf, unsigned long len);
int ktfs_create(struct filesystem * fs, const char* name);
int ktfs_delete(struct filesystem * fs, const char* name);
void ktfs_flush(struct filesystem * fs);

int ktfs_getblksz(struct ktfs_file *fd);
int ktfs_getend(struct ktfs_file *fd, void *arg);
int ktfs_setend(struct ktfs_file *fd, void *arg);
int ktfs_setpos(struct ktfs_file *fd, void *arg);
int ktfs_getpos(struct ktfs_file *fd, void *arg);

// Interal helper functions
uint64_t inode_number_to_absolute_position(uint16_t inode_number);
uint64_t data_block_number_to_absolute_position(uint16_t data_block_number);

long arbitrary_read(unsigned long pos, void * buf, long bufsz);
long arbitrary_write(unsigned long pos, void* buf, long bufsz);

uint32_t get_free_data_block();
uint32_t get_free_inode();
void update_bitmap(uint32_t block_index, uint8_t value, uint32_t bitmap_block);

uint64_t compute_data_block_pointer_address(uint32_t i, uint16_t inode_number, uint8_t allocate);
int get_ith_data_block(uint32_t i, uint32_t* cur_data_block_number, uint16_t inode_number);
int set_ith_data_block(uint32_t i, uint32_t new_data_block_number, uint16_t inode_number);
int add_data_block_to_inode(uint16_t inode_number);

static inline int min(int a, int b) {
    return (a < b) ? a : b;
}

static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

#define FILE_OPENED (1<<0)

// INTERNAL GLOBAL VARIABLES

static struct lock fs_lock;

static struct cache * ktfs_block_cache;
static struct storage * ktfs_backing_device;
static struct ktfs_superblock superblock;

static struct ktfs_free_inode_elem * free_inode_list;
static struct ktfs_file * files_list;
static uint16_t max_num_of_inodes;

#define SUPERBLOCK_BLOCK (0)
#define INODE_BITMAP_BLOCK (SUPERBLOCK_BLOCK + 1)
#define BITMAP_BLOCK (INODE_BITMAP_BLOCK + superblock.inode_bitmap_block_count)
#define INODE_BLOCK (BITMAP_BLOCK + superblock.bitmap_block_count)
#define DATA_BLOCK (INODE_BLOCK + superblock.inode_block_count)

static const struct uio_intf file_intf = {
    .close = &ktfs_close,
    .cntl = &ktfs_cntl,
    .read = &ktfs_fetch,
    .write = &ktfs_store
};

static struct filesystem fs_intf = {
    .open = &ktfs_open,
    .create = &ktfs_create,
    .delete = &ktfs_delete,
    .flush = &ktfs_flush
};


/**
 * @brief Mounts the hard drive represented by the io object as a filesystem
 * @param io the io object to get the raw filesystem image from (e.g. block device io)
 * @return 0 if mount successful, negative values if there's error.
 */
int mount_ktfs(const char * name, struct cache * cache) {
    struct ktfs_inode root_directory;
    if(cache == NULL)
        return -ENOENT;

    if(name == NULL)
        return -EINVAL;

    lock_init(&fs_lock);

    int result = attach_filesystem(name, &fs_intf);
    if(result != 0)
        return result;

    ktfs_block_cache = cache;
    
    cache_get_backing_device(cache, &ktfs_backing_device);

    // loads the super block to memory
    arbitrary_read(0, &superblock, sizeof(struct ktfs_superblock));

    debug("Mounting filesystem with %d inode bitmap blocks, %d bitmap blocks, %d inode blocks, %d total blocks, and root directory inode number %d\n",superblock.inode_bitmap_block_count, superblock.bitmap_block_count, superblock.inode_block_count, superblock.block_count, superblock.root_directory_inode);
    trace("%s: superblock: %d,%d,%d,%d,%d\n", __func__, superblock.inode_bitmap_block_count, superblock.bitmap_block_count, superblock.block_count, superblock.inode_block_count, superblock.root_directory_inode);

    // loads the root_directory_inode to memory
    arbitrary_read(inode_number_to_absolute_position(superblock.root_directory_inode), &root_directory, sizeof(struct ktfs_inode));

    // this is different from the number of dentries because some inodes might be free
    max_num_of_inodes = superblock.inode_block_count * KTFS_BLKSZ / KTFS_INOSZ;

    uint32_t current_data_block = 0;

    files_list = NULL;
    
    for(uint64_t i = 0; i < root_directory.size / KTFS_DENSZ; i++) {
        // create a new file struct
        struct ktfs_file * new_file = kmalloc(sizeof(struct ktfs_file));
        uio_init0(&(new_file->uio), &file_intf);
        new_file->flag = 0;

        // get the address of the current dentry 
        get_ith_data_block((i * KTFS_DENSZ) / KTFS_BLKSZ, &current_data_block, superblock.root_directory_inode);
        uint64_t address_of_dentry = data_block_number_to_absolute_position(current_data_block) + (i * KTFS_DENSZ) % KTFS_BLKSZ;
        // current_data_block should be 0
        // load the dentry into file struct
        arbitrary_read(address_of_dentry, &(new_file->dentry), sizeof(struct ktfs_dir_entry));

        // loads the inode that is pointed by the dentry, to get the file size
        struct ktfs_inode curr_inode;
        uint64_t address_of_inode = inode_number_to_absolute_position(new_file->dentry.inode); 
        arbitrary_read(address_of_inode, &curr_inode, sizeof(struct ktfs_inode));

        new_file->file_size = curr_inode.size;

        // add the new file to the files_list
        new_file->next = files_list;
        
        if(files_list != NULL)
            files_list->prev = new_file;

        new_file->prev = NULL;
        files_list = new_file;
        new_file->pos = 0;
    }
    return 0;
}


/**
 * @brief Opens a file with the given name and returns a pointer to the file io through double pointer
 * @param name the name of the file to open
 * @param ioptr will return a pointer to a file io object through this double pointer 
 * @return 0 if open successful, negative values if there's error.
 */
int ktfs_open(struct filesystem * fs, const char * name, struct uio ** uioptr) {
    for(struct ktfs_file* curr_file = files_list; curr_file != NULL; curr_file = curr_file->next){
        // found file in filesystem
        if (strncmp(name, curr_file->dentry.name, KTFS_MAX_FILENAME_LEN) == 0){
            if(curr_file->flag & FILE_OPENED){ // do not allow opening a file multiple times simultaneously
                return -EBUSY;
            }
            curr_file->flag |= FILE_OPENED;
            // ioaddref(&curr_file->io);
            // wrap in a seekio
            *uioptr = &curr_file->uio;
            return 0;
        }
    }

    // no file in the filesystem
    return -ENOENT;
}


/**
 * @brief Closes the file that is represented by the io object
 * @param io the file io of the file to close
 * @return None
 */
void ktfs_close(struct uio* uio) {
    struct ktfs_file* curr_file = (void*)uio - offsetof(struct ktfs_file, uio);
    curr_file->flag &= ~FILE_OPENED;
}

long ktfs_fetch(struct uio *uio, void *buf, unsigned long len) {
    long total_num_bytes_to_read = len;
    trace("%s(%p,%ld)", __func__, buf, len);
    
    // Check for bad inputs
    if (!uio || !buf || len < 0) {
        return -EINVAL;
    }
    
    struct ktfs_file* f = (void*)uio - offsetof(struct ktfs_file, uio);
    long pos = f->pos;
    int file_opened = f->flag & FILE_OPENED;
    if (!file_opened) {
        return -EBADFD;
    }
    
    // we can not read past the end of the file, so if len is too large, total_num_bytes_to_read is modified appropriately
    total_num_bytes_to_read = min(total_num_bytes_to_read, f->file_size - pos);
  
    // cur_data_block: the index of the actual data block
    uint32_t ith_data_block;
    uint32_t offset_from_block_start;
    uint32_t cur_data_block;
    long num_bytes_left_to_read = total_num_bytes_to_read;
    long num_bytes_read = 0;
    while (num_bytes_left_to_read > 0){
        ith_data_block = pos / KTFS_BLKSZ;
        offset_from_block_start = pos % KTFS_BLKSZ;
        get_ith_data_block(ith_data_block, &cur_data_block, f->dentry.inode);

        // we are in the data block in which pos is located
        // read until a block edge or read all the bytes left, which ever comes first.
        uint32_t read_len = min(num_bytes_left_to_read, KTFS_BLKSZ-offset_from_block_start);
        uint32_t address_to_start_read = data_block_number_to_absolute_position(cur_data_block) + offset_from_block_start;
        arbitrary_read(address_to_start_read, (uint8_t*)buf + num_bytes_read, read_len);

        num_bytes_read += read_len;
        num_bytes_left_to_read -= read_len;
        pos += read_len;
    }
    f->pos = pos;

    return num_bytes_read;
}

long ktfs_store(struct uio* uio, const void* buf, unsigned long len){
    struct ktfs_file* f = (void*) uio - offsetof(struct ktfs_file, uio);
    int file_opened = f->flag & FILE_OPENED;
    long pos = f->pos;

    if (!file_opened){
        return -EBADFD;
    }

    // we cannot write past the length of the file
    long total_num_bytes_to_write = min(len, f->file_size - pos);

    uint32_t ith_data_block;
    uint32_t offset_from_block_start;
    uint32_t cur_data_block;
    long num_bytes_left_to_write = total_num_bytes_to_write;
    long num_bytes_written = 0;
    while (num_bytes_left_to_write > 0){

        ith_data_block = pos / KTFS_BLKSZ;
        offset_from_block_start = pos % KTFS_BLKSZ;
        get_ith_data_block(ith_data_block, &cur_data_block, f->dentry.inode);

        uint32_t write_len = min(num_bytes_left_to_write, KTFS_BLKSZ - offset_from_block_start);

        uint64_t address_to_start_write = data_block_number_to_absolute_position(cur_data_block) + offset_from_block_start;
        arbitrary_write(address_to_start_write, (uint8_t*)buf + num_bytes_written, write_len);
        num_bytes_written += write_len;
        num_bytes_left_to_write -= write_len;
        pos += write_len;
    }
    f->pos = pos;
    return num_bytes_written;
}

int ktfs_create(struct filesystem * fs, const char* name) {
    struct ktfs_inode root_directory;
    if (!name || strlen(name) > KTFS_MAX_FILENAME_LEN) {
        return -EINVAL;
    }

    // Check if the file already exists
    for (struct ktfs_file * curr_file = files_list; curr_file != NULL; curr_file = curr_file->next) {
        if (strncmp(name, curr_file->dentry.name, KTFS_MAX_FILENAME_LEN) == 0) {
            return -EMFILE;
        }
    }

    // Get free inode
    uint32_t free_inode_index = get_free_inode();
    if (free_inode_index == -ENOINODEBLKS) {
        return -ENOINODEBLKS;
    }

    // Create the new file
    struct ktfs_file* new_file = kmalloc(sizeof(struct ktfs_file));
    uio_init0(&(new_file->uio), &file_intf);
    new_file->flag = 0;
    new_file->file_size = 0;
    memset(&(new_file->dentry), 0, sizeof(struct ktfs_dir_entry));
    strncpy(new_file->dentry.name, name, KTFS_MAX_FILENAME_LEN);
    new_file->dentry.inode = free_inode_index;

    arbitrary_read(inode_number_to_absolute_position(superblock.root_directory_inode), &root_directory, sizeof(struct ktfs_inode));

    // Write the dentry into the root directory
    uint32_t data_block_idx = root_directory.size / KTFS_BLKSZ;
    uint32_t data_block_offset = root_directory.size % KTFS_BLKSZ;

    // If the offset is 0, then we are at the beginning of a new data block
    if (data_block_offset == 0) {
        add_data_block_to_inode(superblock.root_directory_inode);
    }

    uint32_t dentry_address = data_block_number_to_absolute_position(data_block_idx) + data_block_offset;
    arbitrary_write(dentry_address, &new_file->dentry, sizeof(struct ktfs_dir_entry));
    root_directory.size += KTFS_DENSZ;

    // Update the root directory size
    arbitrary_write(inode_number_to_absolute_position(superblock.root_directory_inode) + offsetof(struct ktfs_inode, size), &root_directory.size, sizeof(uint32_t));

    // might not need this?
    cache_flush(ktfs_block_cache);

    // Add the new file to the files list
    new_file->next = files_list;
    if (files_list != NULL) {
        files_list->prev = new_file;
    }
    new_file->prev = NULL;
    files_list = new_file;
    return 0;
}

int ktfs_delete(struct filesystem * fs, const char* name) {
    struct ktfs_file* f;
    int i;
    uint32_t data_block_idx, data_block_offset, data_block_num;
    uint32_t last_data_block_idx, last_data_block_offset, last_data_block_num;
    struct ktfs_dir_entry dentry, last_dentry;

    for (f = files_list; f != NULL; f = f->next) {
        if (strncmp(name, f->dentry.name, KTFS_MAX_FILENAME_LEN) == 0) {
            break;
        }
    }

    if (f == NULL) {
        return -ENOENT;
    }

    // Remove the file from the files list
    if (f->prev != NULL) {
        f->prev->next = f->next;
    }
    if (f->next != NULL) {
        f->next->prev = f->prev;
    }
    if (files_list == f) {
        files_list = f->next;
    }

    // Free data blocks associated with the file
    for (i = 0; i < (f->file_size + KTFS_BLKSZ - 1) / KTFS_BLKSZ; i++) {
        if (get_ith_data_block(i, &data_block_idx, f->dentry.inode) == 0) {
            update_bitmap((1 + superblock.inode_bitmap_block_count + superblock.bitmap_block_count + superblock.inode_block_count) + data_block_idx, KTFS_FILE_FREE, (1 + superblock.inode_bitmap_block_count + superblock.bitmap_block_count + superblock.inode_block_count));
        }
    }

    // free inode
    update_bitmap(f->dentry.inode, KTFS_FILE_FREE, 1);

    // Overwrite the inode in the filesystem image
    struct ktfs_inode empty_inode;
    memset(&empty_inode, 0, sizeof(struct ktfs_inode));
    arbitrary_write(inode_number_to_absolute_position(f->dentry.inode), &empty_inode, sizeof(struct ktfs_inode));

    // Find the dentry in the root directory and remove it
    struct ktfs_inode root_directory;
    arbitrary_read(inode_number_to_absolute_position(superblock.root_directory_inode), &root_directory, sizeof(struct ktfs_inode));

    uint32_t original_num_inodes = root_directory.size/KTFS_DENSZ;
    for (i = 0; i < root_directory.size; i += KTFS_DENSZ) {
        data_block_idx = i / KTFS_BLKSZ;
        data_block_offset = i % KTFS_BLKSZ;
        get_ith_data_block(data_block_idx, &data_block_num, superblock.root_directory_inode);
        
        arbitrary_read(data_block_number_to_absolute_position(data_block_num) + data_block_offset, &dentry, sizeof(struct ktfs_dir_entry));

        if (strncmp(dentry.name, f->dentry.name, KTFS_MAX_FILENAME_LEN) == 0) {
            // Clear the dentry
            memset(&dentry, 0, sizeof(struct ktfs_dir_entry));
            arbitrary_write(data_block_number_to_absolute_position(data_block_num) + data_block_offset, &dentry, sizeof(struct ktfs_dir_entry));
            
            // Move the last dentry to the current position
            last_data_block_idx = (root_directory.size - KTFS_DENSZ) / KTFS_BLKSZ;
            last_data_block_offset = (root_directory.size - KTFS_DENSZ) % KTFS_BLKSZ;
            get_ith_data_block(last_data_block_idx, &last_data_block_num, superblock.root_directory_inode);
            arbitrary_read(data_block_number_to_absolute_position(last_data_block_num) + last_data_block_offset, &last_dentry, sizeof(struct ktfs_dir_entry));
            arbitrary_write(data_block_number_to_absolute_position(data_block_num) + data_block_offset, &last_dentry, sizeof(struct ktfs_dir_entry));

            // Update the root directory size
            root_directory.size -= KTFS_DENSZ;

            if (root_directory.size % KTFS_BLKSZ == 0) {
                // Free the last data block
                update_bitmap((1 + superblock.bitmap_block_count + superblock.inode_bitmap_block_count + superblock.inode_block_count) + last_data_block_num, KTFS_FILE_FREE, (1 + superblock.inode_bitmap_block_count + superblock.bitmap_block_count + superblock.inode_block_count));
            }

            arbitrary_write(inode_number_to_absolute_position(superblock.root_directory_inode), &root_directory, sizeof(struct ktfs_inode));
            break;
        }
    }

    if (i == original_num_inodes) {
        debug("Could not find dentry for file %s in root directory\n", f->dentry.name);
        return -ENOENT;
    }

    // Ensure that file deletion is flushed to the backing device
    cache_flush(ktfs_block_cache);

    // Free the file struct
    kfree(f);
    return 0;
}


/**
 * @brief Given a file io object, a specific command, and possibly some arguments, execute the 
 * corresponding functions
 * @param io the file io object of the file to perform the control function
 * @param cmd the io command to execute. Should support IOCTL_GETBUFSZ, IOCTL_GETEND, IOCTL_GETPOS, IOCTL_SETPOS
 * @param arg the argument to pass in, maybe different for different control functions
 * @return depends on specific control functions
 */
int ktfs_cntl(struct uio *uio, int cmd, void *arg) {
    // struct ktfs_file * f = (struct ktfs_file *)uio;
    struct ktfs_file* f = (void*) uio - offsetof(struct ktfs_file, uio);
    int file_opened = f->flag & FILE_OPENED;
    if (!file_opened) {
        return -EBADFD;
    }

    switch (cmd) {
    case FCNTL_GETEND:
        return ktfs_getend(f, arg);
    case FCNTL_SETEND:
        return ktfs_setend(f, arg);
    case FCNTL_SETPOS:
        return ktfs_setpos(f, arg);
    case FCNTL_GETPOS:
        return ktfs_getpos(f, arg);
    default:
        return -EINVAL;
    }
}

/**
 * @brief Get the block size of the filesystem
 * @param fd pointer to any file in the filesystem
 * @return the block size of the filesystem
 */

int ktfs_getblksz(struct ktfs_file *fd) {
    return 1;
}


/**
 * @brief Get the length of the given file, result will be returned through the pointer in argument
 * @param fd the pointer to the file that you want to find the length 
 * @param arg interpreted as an (unsigned long long *), which is used to store the result 
 * @return 0 if successful, negative values if there's an error.
 */
int ktfs_getend(struct ktfs_file *fd, void *arg) {
    if (arg == NULL) {
        return -EINVAL;
    }
    *((unsigned long long*)arg) = fd->file_size;
    return 0;
}

int ktfs_setend(struct ktfs_file *fd, void *arg) {
    int result;
    uint32_t amt, excess;
    uint32_t end = *(uint32_t*)arg;

    if (end > KTFS_MAX_FILE_SIZE) {
        return -EINVAL;
    }

    excess = end - fd->file_size;
    while (excess > 0){
        // if the current size is a multiple of block size we must allocate a new block
        if (fd->file_size % KTFS_BLKSZ == 0) {
            result = add_data_block_to_inode(fd->dentry.inode);
            
            // if result < 0 (no data blocks left in drive)
            if (result < 0){
                return -ENODATABLKS;
            }

            amt = min(excess, KTFS_BLKSZ);
        } else {
            amt = min(excess, KTFS_BLKSZ - (fd->file_size%KTFS_BLKSZ));
        }
        fd->file_size += amt;
        excess -= amt;

        arbitrary_write(inode_number_to_absolute_position(fd->dentry.inode), &(fd->file_size), sizeof(uint32_t));
    }

    return 0;
}

int ktfs_getpos(struct ktfs_file * fd, void * arg){
    if(arg == NULL)
        return -EINVAL;

    *((unsigned long*)arg) = fd->pos;
    return 0;
}

int ktfs_setpos(struct ktfs_file * fd, void * arg){
    if(arg == NULL)
        return -EINVAL;

    fd->pos = *((unsigned long*)arg);
    return 0;
}

/**
 * @brief Flushes the cache to the backing device
 * @return 0 if flush successful, negative values if there's an error.
 */
void ktfs_flush(struct filesystem * fs) {
    if (ktfs_block_cache == NULL) {
        return;
    }
    
    cache_flush(ktfs_block_cache);
    return;
}

/// @brief Converts an inode number to its absolute position in the filesystem.
/// @param inode_number The inode number to be converted to an absolute position.
/// @return The absolute position of the inode in the filesystem.
uint64_t inode_number_to_absolute_position(uint16_t inode_number) {
    return (1 + superblock.inode_bitmap_block_count + superblock.bitmap_block_count) * KTFS_BLKSZ + (inode_number / (KTFS_BLKSZ / KTFS_INOSZ)) * KTFS_BLKSZ + (inode_number % (KTFS_BLKSZ / KTFS_INOSZ)) * KTFS_INOSZ;
}

/// @brief Converts a data block number to its absolute position in the filesystem.
/// @param data_block_number The data block number to be converted to an absolute position.
/// @return The absolute position of the data block in the filesystem.
uint64_t data_block_number_to_absolute_position(uint16_t data_block_number) {
    return (1 + superblock.inode_bitmap_block_count + superblock.bitmap_block_count + superblock.inode_block_count) * KTFS_BLKSZ + data_block_number * KTFS_BLKSZ;
}

/// @brief Reads 'bufsz' bytes from position 'pos' into 'buf'. Truncates read if necessary.
/// @param pos The position in the filesystem to read from.
/// @param buf The buffer to read into.
/// @param bufsz The number of bytes to read.
/// @return The number of bytes read.
long arbitrary_read(unsigned long pos, void* buf, long bufsz) {
    trace("%s(0x%llx,%p,%ld)", __func__, pos, buf, bufsz);

    // Null checks
    if (bufsz <= 0 || !buf) {
        return 0;
    }
    
    // Get the size of the filesystem and truncate the read if necessary
    uint64_t fs_size;
    storage_cntl(ktfs_backing_device, FCNTL_GETEND, &fs_size);
    const unsigned long bytes_to_read = (pos >= fs_size) ? 0 : min(bufsz, fs_size - pos);
    unsigned long bytes_remaining = bytes_to_read;

    while (bytes_remaining > 0) {
        const uint64_t block_index = pos / CACHE_BLKSZ;
        const uint64_t block_offset = pos % CACHE_BLKSZ;
        void* block_data = NULL;
        
        if (cache_get_block(ktfs_block_cache, block_index * CACHE_BLKSZ, &block_data) != 0) {
            return -1;
        }

        const uint64_t bytes_available = CACHE_BLKSZ - block_offset;
        const uint64_t bytes_to_copy = min(bytes_remaining, bytes_available);

        memcpy(buf, (uint8_t*)block_data + block_offset, bytes_to_copy);
        
        buf += bytes_to_copy;
        pos += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;

        cache_release_block(ktfs_block_cache, block_data, CACHE_CLEAN);
    }

    return bytes_to_read - bytes_remaining;
}

/// @brief Writes 'len' bytes to position 'pos' from 'buf'. Truncates write if necessary.
/// @param pos The position in the filesystem to write to.
/// @param buf The buffer to read from.
/// @param len The number of bytes to write.
/// @return The number of bytes written.
long arbitrary_write(unsigned long pos, void* buf, long len) {    
    trace("%s(0x%llx,%p,%ld)", __func__, pos, buf, len);

    // Null checks
    if (len <= 0 || !buf) {
        return 0;
    }
    
    // Get the size of the filesystem and truncate the write if necessary
    uint64_t fs_size;
    storage_cntl(ktfs_backing_device, FCNTL_GETEND, &fs_size);
    const unsigned long bytes_to_write = (pos >= fs_size) ? 0 : min(len, fs_size - pos);
    unsigned long bytes_remaining = bytes_to_write;

    while (bytes_remaining > 0) {
        const uint64_t block_index = pos / CACHE_BLKSZ;
        const uint64_t block_offset = pos % CACHE_BLKSZ;
        void* block_data = NULL;
        
        if (cache_get_block(ktfs_block_cache, block_index * CACHE_BLKSZ, &block_data) != 0) {
            return -1;
        }

        const uint64_t bytes_available = CACHE_BLKSZ - block_offset;
        const uint64_t bytes_to_copy = min(bytes_remaining, bytes_available);

        memcpy((uint8_t*)block_data + block_offset, buf, bytes_to_copy);
        
        buf += bytes_to_copy;
        pos += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;

        cache_release_block(ktfs_block_cache, block_data, CACHE_DIRTY);
    }

    // cache_flush(ktfs_block_cache);

    return bytes_to_write - bytes_remaining;
}

/// @brief Get the next free data block in the filesystem. Marks the block as in-use
/// @return Data block index if successful, -ENODATABLKS if no free data blocks are available
uint32_t get_free_bit(uint32_t start_block_idx, uint32_t max_val){
    int i, j;
    // Iterate through each data block index
    for (i = 0; i * 8 < max_val; i++){
        uint8_t byte;
        arbitrary_read(start_block_idx * KTFS_BLKSZ + i, &byte, sizeof(uint8_t));

        for (j = 0; j < 8; j++){
            if (!(byte & (1 << j))){                
                if (i * 8 + j < max_val){
                    update_bitmap(i * 8 + j, KTFS_FILE_IN_USE, start_block_idx);
                    return i * 8 + j;
                }
            }
        }
    }

    return -1;
}

uint32_t get_free_data_block() {
    uint32_t result = get_free_bit(1 + superblock.inode_bitmap_block_count, superblock.block_count);
    if (result == (uint32_t)(-1)) return -ENODATABLKS;
    return result - (1 + superblock.inode_bitmap_block_count + superblock.bitmap_block_count + superblock.inode_block_count);
}

uint32_t get_free_inode() {
    uint32_t result = get_free_bit(1, max_num_of_inodes);
    if (result == (uint32_t)(-1)) return -ENOINODEBLKS;
    return result;
}

/// @brief Updates the bitmap for the given block index with the given value
/// @param index The index of the block to update (0-indexed)
/// @param value KTFS_FILE_IN_USE or KTFS_FILE_FREE
void update_bitmap(uint32_t index, uint8_t value, uint32_t bitmap_block) {
    uint8_t byte;
    uint32_t bitmap_index = index / (KTFS_BLKSZ * 8);
    index %= KTFS_BLKSZ * 8;
    uint32_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    uint64_t file_offset = (bitmap_block + bitmap_index) * KTFS_BLKSZ + byte_index;

    arbitrary_read(file_offset, &byte, 1);
    byte = (byte & ~(1 << bit_index)) | (value << bit_index);
    arbitrary_write(file_offset, &byte, 1);
}

/// @brief Gets the ith data block of an inode
/// @param i The index of the data block to get
/// @param cur_data_block_number A pointer to where to write the data block number
/// @param inode_number The inode number of the inode to get the data block from
/// @return 0 if successful, or error code
int get_ith_data_block(uint32_t i, uint32_t* cur_data_block_number, uint16_t inode_number) {
    uint64_t address = compute_data_block_pointer_address(i, inode_number, KTFS_NO_ALLOCATION);
    if (address == -ENODATABLKS) {
        return -ENODATABLKS;
    }
    if (arbitrary_read(address, cur_data_block_number, sizeof(uint32_t)) != sizeof(uint32_t)) {
        return -EIO;
    }
    return 0;
}

/// @brief Sets the ith data block of an inode
/// @param i The index of the data block to set
/// @param new_data_block_number A pointer to the new data block number
/// @param inode_number The inode number of the inode to set the data block in
/// @return 0 if successful, or error code
int set_ith_data_block(uint32_t i, uint32_t new_data_block_number, uint16_t inode_number) {
    uint64_t address = compute_data_block_pointer_address(i, inode_number, KTFS_NO_ALLOCATION);
    if (address == -ENODATABLKS) {
        return -ENODATABLKS;
    }
    if (arbitrary_write(address, &new_data_block_number, sizeof(uint32_t)) != sizeof(uint32_t)) {
        return -EIO;
    }
    return 0;
}

/// @brief Adds a data block to an inode sequentially
/// @param inode_number The inode number of the inode to add the data block to
/// @return 0 if successful, or error code
int add_data_block_to_inode(uint16_t inode_number) {
    struct ktfs_inode inode;

    if (arbitrary_read(inode_number_to_absolute_position(inode_number), &inode, sizeof(struct ktfs_inode)) != sizeof(struct ktfs_inode)) {
        return -EIO;
    }

    uint32_t curr_num_data_blocks = (inode.size + KTFS_BLKSZ - 1) / KTFS_BLKSZ;

    uint64_t address = compute_data_block_pointer_address(curr_num_data_blocks, inode_number, KTFS_ALLOCATE);
    if (address == -ENODATABLKS) {
        return -ENODATABLKS;
    }
    uint32_t new_data_block = get_free_data_block();
    if (new_data_block == -ENODATABLKS) {
        return -ENODATABLKS;
    }
    if (arbitrary_write(address, &new_data_block, sizeof(uint32_t)) != sizeof(uint32_t)) {
        return -EIO;
    }

    return 0;
}

/// @brief Computes the address of the ith data block pointer in an inode
/// @param i The index of the data block pointer to get
/// @param inode_number The inode number of the inode to get the data block pointer from
/// @param allocate Whether to allocate a new data block if necessary
/// @return The address of the data block pointer
uint64_t compute_data_block_pointer_address(uint32_t i, uint16_t inode_number, uint8_t allocate) {
    struct ktfs_inode inode;
    uint32_t indirect_block, dindirect_block;
    uint64_t inode_abs = inode_number_to_absolute_position(inode_number);
    uint64_t entries_per_block = KTFS_BLKSZ / sizeof(uint32_t);
    arbitrary_read(inode_abs, &inode, sizeof(struct ktfs_inode));

    // Direct data blocks
    if (i < KTFS_NUM_DIRECT_DATA_BLOCKS) {
        return inode_abs + offsetof(struct ktfs_inode, block) + i * sizeof(uint32_t);
    }
    // Indirect data blocks
    else if (i < KTFS_NUM_DIRECT_DATA_BLOCKS + entries_per_block * KTFS_NUM_INDIRECT_BLOCKS) {
        uint64_t address_of_indirect_ptr = inode_abs + offsetof(struct ktfs_inode, indirect);
        
        arbitrary_read(address_of_indirect_ptr, &indirect_block, sizeof(uint32_t));
        if ((i - KTFS_NUM_DIRECT_DATA_BLOCKS) % entries_per_block == 0 && allocate) {
            indirect_block = get_free_data_block();
            arbitrary_write(address_of_indirect_ptr, &indirect_block, sizeof(uint32_t));
        }

        uint32_t index_in_indirect = (i - KTFS_NUM_DIRECT_DATA_BLOCKS) % entries_per_block;
        return data_block_number_to_absolute_position(indirect_block) + index_in_indirect * sizeof(uint32_t);
    }
    // Doubly indirect data blocks
    else if (i < KTFS_NUM_DIRECT_DATA_BLOCKS + entries_per_block * KTFS_NUM_INDIRECT_BLOCKS + entries_per_block * entries_per_block * KTFS_NUM_DINDIRECT_BLOCKS) {
        uint32_t num_direct_and_indirect = KTFS_NUM_DIRECT_DATA_BLOCKS + entries_per_block * KTFS_NUM_INDIRECT_BLOCKS;
        uint64_t address_of_dindirect_ptr = inode_abs + offsetof(struct ktfs_inode, dindirect) + ((i - num_direct_and_indirect) / (entries_per_block * entries_per_block)) * sizeof(uint32_t);

        arbitrary_read(address_of_dindirect_ptr, &dindirect_block, sizeof(uint32_t));
        if ((i - num_direct_and_indirect) % ((KTFS_BLKSZ / sizeof(uint32_t)) * (KTFS_BLKSZ / sizeof(uint32_t))) == 0 && allocate) {
            dindirect_block = get_free_data_block();
            arbitrary_write(address_of_dindirect_ptr, &dindirect_block, sizeof(uint32_t));
        }
        
        uint32_t new_indirect = (i - num_direct_and_indirect) % (entries_per_block * entries_per_block);
        uint64_t address_of_indirect_ptr = data_block_number_to_absolute_position(dindirect_block) + (new_indirect / entries_per_block) * sizeof(uint32_t);

        arbitrary_read(address_of_indirect_ptr, &indirect_block, sizeof(uint32_t));
        if ((new_indirect) % (KTFS_BLKSZ / sizeof(uint32_t)) == 0 && allocate) {
            indirect_block = get_free_data_block();
            arbitrary_write(address_of_indirect_ptr, &indirect_block, sizeof(uint32_t));
        }
        
        uint32_t index_in_indirect = new_indirect % entries_per_block;
        return data_block_number_to_absolute_position(indirect_block) + index_in_indirect * sizeof(uint32_t);
    }
    return -ENODATABLKS;
}
