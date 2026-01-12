/*! @file ktfs.c
    @brief KTFS Implementation.
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifdef KTFS_TRACE
#define TRACE
#endif

#ifdef KTFS_DEBUG
#define DEBUG
#endif

#include "ktfs.h"

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

/// @brief File struct for a file in the Keegan Teal Filesystem
struct ktfs_file {
    /// uio struct for accessing file
    struct uio uio;

    /// directory entry attached to file
    struct ktfs_dir_entry dentry;

    /// Previous file in linked list
    struct ktfs_file * prev;

    /// Next file in linked list
    struct ktfs_file * next;

    /// Size of file in bytes
    uint32_t file_size;

    /// Flags related to the file
    uint8_t flag;

    /// Position in file
    unsigned long pos;// add pos
};

/// @brief Elem struct for listing deep copies
struct ktfs_listing_elem {
    struct ktfs_listing_elem * next;
    char name[KTFS_MAX_FILENAME_LEN+sizeof(uint8_t)];
};

/// @brief Wrapper struct for uio and attached file
struct ktfs_listing_uio {
    struct uio base;
    struct ktfs_listing_elem * file;
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

int ktfs_open_file(const char * name, struct uio ** uioptr);
int ktfs_open_listing(struct uio ** uioptr);
void ktfs_listing_close(struct uio * uio);
long ktfs_listing_read (struct uio * uio, void * buf, unsigned long bufsz);

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

static const struct uio_intf ktfs_listing_uio_intf = {
    .close = &ktfs_listing_close,
    .read = &ktfs_listing_read
};

/**
 * @brief Mounts the file system with associated backing cache
 * @param cache Pointer to cache struct for the file system
 * @return 0 if mount successful, negative error code if error
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

        debug("Mounted file: name=%s, inode=%d, size=%d", new_file->dentry.name, new_file->dentry.inode, new_file->file_size);
    }
    return 0;
}

/**
 * @brief Opens a file or ls (listing) with the given name and returns a pointer to the uio through
 * the double pointer. 
 * @details A file may only be referenced by one uioptr at a time.
 * @param name The name of the file to open or "" for listing (CP3)
 * @param uioptr Will return a pointer to a file or ls (list) uio pointer through this double
 * pointer
 * @return 0 if open successful, negative error code if error
 */
int ktfs_open(struct filesystem * fs, const char * name, struct uio ** uioptr) {
    if (name == NULL || *name == '\0')
        return ktfs_open_listing(uioptr);
    else
        return ktfs_open_file(name, uioptr);
}


/**
 * @brief Opens a device and returns a pointer to the file through the uio
 * @param name the name of the file to open
 * @param uioptr will return a pointer to a file through the uio double pointer
 * @return 0 if successful or negative values for errors
 */
int ktfs_open_file(const char * name, struct uio ** uioptr) {
    for(struct ktfs_file* curr_file = files_list; curr_file != NULL; curr_file = curr_file->next){
        // found file in filesystem
        if (strncmp(name, curr_file->dentry.name, KTFS_MAX_FILENAME_LEN) == 0){
          debug("ktfs_open: file=%s, flag=%d, pos=%ld, refcnt=%d",
                name, curr_file->flag, curr_file->pos, curr_file->uio.refcnt);
          if (curr_file->flag & FILE_OPENED)
          { // do not allow opening a file multiple times simultaneously
            debug("ktfs_open: EBUSY - file already opened");
            return -EBUSY;
          }
            curr_file->flag |= FILE_OPENED;
            curr_file->pos = 0;        // Reset file position to beginning when opening
            curr_file->uio.refcnt = 1; // Initialize refcnt to 1 for the initial reference
            debug("ktfs_open: SUCCESS - file=%s, reset pos=0, refcnt=1", name);
            // wrap in a seekio
            *uioptr = &curr_file->uio;
            return 0;
        }
    }

    // no file in the filesystem
    return -ENOENT;
}

/**
 * @brief Closes the file that is represented by the uio struct
 * @param uio The file io to be closed
 * @return None
 */
void ktfs_close(struct uio* uio) {
    struct ktfs_file* curr_file = (void*)uio - offsetof(struct ktfs_file, uio);
    debug("ktfs_close: file=%s, flag=%d, pos=%ld, refcnt=%d",
          curr_file->dentry.name, curr_file->flag, curr_file->pos, uio->refcnt);
    curr_file->flag &= ~FILE_OPENED;
    debug("ktfs_close: DONE - file=%s, flag now=%d", curr_file->dentry.name, curr_file->flag);
}

/**
 * @brief Reads data from file attached to uio into provided argument buffer
 * @param uio uio of file to be read
 * @param buf Buffer to be filled
 * @param len Number of bytes to read
 * @return Number of bytes read if successful, negative error code if error
 */
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

/**
 * @brief Write data from the provided argument buffer into file attached to uio
 * @param uio The file to be written to
 * @param buf The buffer to be read from
 * @param len Number of bytes to write from the buffer to the file
 * @return Number of bytes written from the buffer to the file system if sucessful, negative error
 * code if error
 */
long ktfs_store(struct uio* uio, const void* buf, unsigned long len){
    struct ktfs_file* f = (void*) uio - offsetof(struct ktfs_file, uio);
    int file_opened = f->flag & FILE_OPENED;
    long pos = f->pos;
    int result;
    uint32_t end;

    if (!file_opened){
        return -EBADFD;
    }

    // extend file size of necessary
    if (f->file_size - pos < len) {     
        end = pos + len;
        result = ktfs_setend(f, &end);
        if (result != 0)
            return result;
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

/**
 * @brief Create a new file in the file system
 * @details Should fail if the file already exists. Should also keep dentries contiguous.
 * @param fs The file system in which to create the file
 * @param name The name of the file
 * @return 0 if successful, negative error code if error
 */
int ktfs_create(struct filesystem * fs, const char* name) {
    struct ktfs_inode root_directory;
    uint32_t data_block_idx;
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
    uint32_t ith_data_block = root_directory.size / KTFS_BLKSZ;
    uint32_t data_block_offset = root_directory.size % KTFS_BLKSZ;

    // If the offset is 0, then we are at the beginning of a new data block
    if (data_block_offset == 0) {
        add_data_block_to_inode(superblock.root_directory_inode);
    }

    get_ith_data_block(ith_data_block, &data_block_idx, superblock.root_directory_inode);

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

/**
 * @brief Deletes a certain file from the file system with the given name
 * @details Should fail if the file does not exist or is open. Should also keep dentries contiguous.
 * @param fs The file system to delete the file from
 * @param name The name of the file to be deleted
 * @return 0 if successful, negative error code if error
 */
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
 * @brief   Given a file io object, a specific command, and possibly some arguments, execute the
 *          corresponding functions
 * @details Any commands such as (FCNTL_GETEND, FCNTL_GETPOS, ...) should pass back through the arg
 *          variable. Do not directly return the value.
 * @details FCNTL_GETEND should pass back the size of the file in bytes through the arg variable.
 * @details FCNTL_SETEND should set the size of the file to the value passed in through arg. Does
 *          not need to support file truncation
 * @details FCNTL_GETPOS should pass back the current position of the file pointer in bytes through
 *          the arg variable.
 * @details FCNTL_SETPOS should set the current position of the file pointer to the value passed in
 *          through arg.
 * @param uio the uio object of the file to perform the control function
 * @param cmd the operation to execute. KTFS should support FCNTL_GETEND, FCNTL_SETEND (CP2),
 *            FCNTL_GETPOS, FCNTL_SETPOS.
 * @param arg the argument to pass in, may be different for different control functions
 * @return 0 if successful, negative error code if error
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
 * @param fd The pointer to the file that you want to find the length 
 * @param arg Interpreted as an (unsigned long long *), which is used to store the result 
 * @return 0 if successful, negative values if there's an error.
 */
int ktfs_getend(struct ktfs_file *fd, void *arg) {
    if (arg == NULL) {
        return -EINVAL;
    }
    *((unsigned long long*)arg) = fd->file_size;
    return 0;
}

/**
 * @brief Set the size of a given file, allocates new data blocks to the inode if needed
 * @param fd The pointer ot the file that you wish to resize
 * @param arg Interpreted as uint32_t and used as the new file size 
 * @return 0 if successful, negative values if there's an error
 */
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

/**
 * @brief Get the current position in a file and pass it back through arg
 * @param fd The pointer of the file to inspect the current position of
 * @param arg The pointer to a unsigned long variable where the current position of the file to be written to
 * @return 0 if successful, negative value if not
 */
int ktfs_getpos(struct ktfs_file * fd, void * arg){
    if(arg == NULL)
        return -EINVAL;

    *((unsigned long*)arg) = fd->pos;
    return 0;
}

/**
 * @brief Set the current position in a file to provided argument
 * @param fd The pointer of the file to set the position of
 * @param arg The pointer to an unsigned long which represents the new position we want to set for the file
 * @return 0 if successful, negative value if not
 */
int ktfs_setpos(struct ktfs_file * fd, void * arg){
    if(arg == NULL)
        return -EINVAL;

    fd->pos = *((unsigned long*)arg);
    return 0;
}

/**
 * @brief Flushes the cache to the backing device
 * @return None
 */
void ktfs_flush(struct filesystem * fs) {
    if (ktfs_block_cache == NULL) {
        return;
    }
    
    cache_flush(ktfs_block_cache);
    return;
}

/**
 * @brief Opens the listing (ls) uio and passes it back through the uioptr
 * @param uioptr will return the listing uio through this double pointer
 * @return 0 if successful 
 */
int ktfs_open_listing(struct uio ** uioptr) {
    struct ktfs_listing_uio * ls;
    struct ktfs_listing_elem** e;
    struct ktfs_file * f = files_list;

    // note we need to deep copy all filenames to
    // avoid use after free if a file is deleted
    ls = kcalloc(1, sizeof(*ls));
    e = &ls->file;
    while (f != NULL) {
        *e = kcalloc(1, sizeof(struct ktfs_listing_elem));
        strncpy((*e)->name, f->dentry.name, KTFS_MAX_FILENAME_LEN);
        e = &(*e)->next;
        f = f->next;
    }

    *uioptr = uio_init1(&ls->base, &ktfs_listing_uio_intf);

    return 0;
}

/**
 * @brief Closes the listing device represented by the uio pointer
 * @param uio The uio pointer of ls
 * @return None
 */
void ktfs_listing_close(struct uio * uio) {
    struct ktfs_listing_uio * const ls = (struct ktfs_listing_uio*)uio;
    struct ktfs_listing_elem *cur, *next;
    
    // free remaining deep copied names
    cur = ls->file;
    while (cur != NULL) {
        next = cur->next;
        kfree(cur);
        cur = next;
    }
    kfree(ls);
}

/**
 * @brief Reads one file name in the file system using ls and copies it into the
 * providied buffer
 * @param uio The uio pointer of ls
 * @param buf The buffer to copy the file names to
 * @param bufsz The size of the buffer
 * @return The size written to the buffer
 */
long ktfs_listing_read(struct uio * uio, void * buf, unsigned long bufsz) {
    struct ktfs_listing_uio * const ls = (struct ktfs_listing_uio*)uio;
    struct ktfs_listing_elem * file = ls->file;
    size_t len;

    if (file != NULL) {
        len = strlen(file->name);
        strncpy(buf, file->name, bufsz);
        ls->file = file->next;

        kfree(file); // no longer needed

        return (len < bufsz) ? len : bufsz;
    } else
        return 0;
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
        const uint64_t block_index = pos / KTFS_BLKSZ;
        const uint64_t block_offset = pos % KTFS_BLKSZ;
        void* block_data = NULL;
        
        if (cache_get_block(ktfs_block_cache, block_index * KTFS_BLKSZ, &block_data) != 0) {
            return -1;
        }

        const uint64_t bytes_available = KTFS_BLKSZ - block_offset;
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
        const uint64_t block_index = pos / KTFS_BLKSZ;
        const uint64_t block_offset = pos % KTFS_BLKSZ;
        void* block_data = NULL;
        
        if (cache_get_block(ktfs_block_cache, block_index * KTFS_BLKSZ, &block_data) != 0) {
            return -1;
        }

        const uint64_t bytes_available = KTFS_BLKSZ - block_offset;
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

/// @brief Gets the first free data block using the block availability bitmap
/// @return The location of the free block
uint32_t get_free_data_block() {
    uint32_t result = get_free_bit(1 + superblock.inode_bitmap_block_count, superblock.block_count);
    if (result == (uint32_t)(-1)) return -ENODATABLKS;
    return result - (1 + superblock.inode_bitmap_block_count + superblock.bitmap_block_count + superblock.inode_block_count);
}

/// @brief Gets the free inode using the inode bitmap
/// @return the number of the inode
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
