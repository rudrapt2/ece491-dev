#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>

#include "../../sys/ktfs.h"

// Change this to turn off randomizing data block indices. This is provided for debugging purposes only.
// Your filesystem driver must be able to handle non-sequential data blocks.
#define RANDOMIZE_ON 1

#define MAX_DATA_BLOCKS_PER_FILE KTFS_NUM_DIRECT_DATA_BLOCKS + (KTFS_BLKSZ / sizeof(uint32_t)) + KTFS_NUM_DINDIRECT_BLOCKS * (KTFS_BLKSZ / sizeof(uint32_t)) * (KTFS_BLKSZ / sizeof(uint32_t))
#define MAX_DISK_SIZE (1 << 30)


struct ktfs_inode root_inode = {0};

uint32_t num_inode_bitmap_blocks;
uint32_t num_bitmap_blocks;
uint32_t num_inode_blocks;
uint32_t num_starter_blocks;

uint32_t current_inode;
uint32_t current_dentry;
uint32_t current_data_block;
uint32_t remaining_disk_size;

void shuffle(uint32_t* arr, size_t n)
{
  srand(time(NULL));
  for (int i = n - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int t = arr[i];
    arr[i] = arr[j];
    arr[j] = t;
  }
}

// Parse disk size from human-readable
uint32_t parse_size(const char* size_str) {
    char unit = size_str[strlen(size_str) - 1];
    uint32_t size = atoi(size_str);
    switch (unit) {
        case 'K': 
        case 'k': 
            size *= 1024;
            break;
        case 'M': 
        case 'm': 
            size *= 1024 * 1024;
            break;
        case 'G': 
        case 'g': 
            size *= 1024 * 1024 * 1024;
            break;
        default:
            fprintf(stderr, "Invalid size unit: %c\n", unit);
            exit(1);
    }

    if (size % KTFS_BLKSZ != 0) {
        fprintf(stderr, "Disk size must be a multiple of %d bytes\n", KTFS_BLKSZ);
        exit(1);
    }

    if (size > MAX_DISK_SIZE) {
        fprintf(stderr, "Disk size exceeds maximum allowed size: %d bytes\n", MAX_DISK_SIZE);
        exit(1);
    }

    return size;
}

// Given a block index, update the bitmap to mark the block as free or in use
void update_bitmap(FILE* fp, uint32_t offset_start, uint32_t block_index, uint8_t value) {
    uint8_t byte;
    uint32_t bitmap_index = block_index / (KTFS_BLKSZ * 8);
    block_index %= KTFS_BLKSZ * 8;
    uint32_t byte_index = block_index / 8;
    uint8_t bit_index = block_index % 8;
    uint64_t file_offset = (offset_start + bitmap_index) * KTFS_BLKSZ + byte_index;

    if (fseek(fp, file_offset, SEEK_SET) != 0) {
        perror("Failed to seek");
        return;
    }
    if (fread(&byte, 1, 1, fp) != 1) {
        perror("Failed to read");
        return;
    }
    byte = (byte & ~(1 << bit_index)) | (value << bit_index);
    
    if (fseek(fp, file_offset, SEEK_SET) != 0) {
        perror("Failed to seek");
        return;
    }
    if (fwrite(&byte, 1, 1, fp) != 1) {
        perror("Failed to write");
        return;
    }
    fflush(fp);
    
    remaining_disk_size += (value == KTFS_FILE_IN_USE) ? -KTFS_BLKSZ : (value == KTFS_FILE_FREE) ? KTFS_BLKSZ : 0;
}

int write_dentry(FILE* fp, struct ktfs_dir_entry* dentry) {
    // Check if the root inode has enough space for another directory entry
    if (root_inode.size % KTFS_BLKSZ == 0) {
        uint32_t new_data_block = current_data_block++;
        update_bitmap(fp, 1 + num_inode_bitmap_blocks, num_starter_blocks + new_data_block, KTFS_FILE_IN_USE);

        root_inode.block[root_inode.size / KTFS_BLKSZ] = new_data_block;
        root_inode.size += KTFS_DENSZ;

        fseek(fp, (num_starter_blocks + new_data_block) * KTFS_BLKSZ, SEEK_SET);
        fwrite(dentry, sizeof(struct ktfs_dir_entry), 1, fp);
    } else {
        fseek(fp, (num_starter_blocks + root_inode.block[root_inode.size / KTFS_BLKSZ]) * KTFS_BLKSZ + root_inode.size % KTFS_BLKSZ, SEEK_SET);
        fwrite(dentry, sizeof(struct ktfs_dir_entry), 1, fp);
        root_inode.size += KTFS_DENSZ;
    }
    fflush(fp);
    return 0;
}

// Write an inode to the filesystem image
int write_inode(FILE* fp, struct ktfs_inode* inode) {
    uint32_t inode_index = current_inode / (KTFS_BLKSZ / KTFS_INOSZ);
    uint32_t inode_offset = current_inode % (KTFS_BLKSZ / KTFS_INOSZ);

    if (inode_index >= num_inode_blocks) {
        fprintf(stderr, "Ran out of inodes\n");
        return -1;
    }

    fseek(fp, (1 + num_inode_bitmap_blocks + num_bitmap_blocks + inode_index) * KTFS_BLKSZ + inode_offset * KTFS_INOSZ, SEEK_SET);
    fwrite(inode, KTFS_INOSZ, 1, fp);
    fflush(fp);
    current_inode++;
    return 0;
}

char* get_filename(const char* filepath) {
    char* last_slash = strrchr(filepath, '/');
    if (last_slash != NULL) {
        return last_slash + 1;
    }
    return (char*)filepath;
}

// Loads a binary (or any other file) into the filesystem image
void load_binary(FILE* fp, const char* binary_path) {
    FILE *binary_fp = fopen(binary_path, "rb");
    if (!binary_fp) {
        fprintf(stderr, "Failed to open binary file: %s\n", binary_path);
        return;
    }

    struct stat st;
    stat(binary_path, &st);
    uint32_t file_size = st.st_size;

    if (file_size > MAX_DATA_BLOCKS_PER_FILE * KTFS_BLKSZ) {
        fprintf(stderr, "File %s size exceeds maximum allowed size: %ld bytes\n", binary_path, MAX_DATA_BLOCKS_PER_FILE * KTFS_BLKSZ);
        fclose(binary_fp);
        return;
    } else if (file_size > remaining_disk_size) {
        fprintf(stderr, "Not enough space remaining on disk to store file %s\n", binary_path);
        fclose(binary_fp);
        return;
    }

    struct ktfs_dir_entry dentry = {0};
    dentry.inode = current_inode;

    memset(dentry.name, 0, KTFS_MAX_FILENAME_LEN);
    strncpy(dentry.name, get_filename(binary_path), KTFS_MAX_FILENAME_LEN);
    write_dentry(fp, &dentry);

    struct ktfs_inode inode = {0};
    inode.size = file_size;

    // Create an array to hold the block indices necessary for the file
    uint32_t num_data_blocks = (file_size + KTFS_BLKSZ - 1) / KTFS_BLKSZ;
    uint32_t* data_block_indices = (uint32_t*)calloc(num_data_blocks, sizeof(uint32_t));

    for (uint32_t i = 0; i < num_data_blocks; i++) {
        data_block_indices[i] = current_data_block++;
        update_bitmap(fp, 1 + num_inode_bitmap_blocks, num_starter_blocks + data_block_indices[i], KTFS_FILE_IN_USE);
    }

    if (RANDOMIZE_ON) {
        shuffle(data_block_indices, num_data_blocks);
    }

    for (uint32_t i = 0; i < num_data_blocks; i++) {
        fseek(fp, (num_starter_blocks + data_block_indices[i]) * KTFS_BLKSZ, SEEK_SET);
        uint8_t buffer[KTFS_BLKSZ] = {0};
        fread(buffer, 1, KTFS_BLKSZ, binary_fp);
        fwrite(buffer, 1, KTFS_BLKSZ, fp);
    }

    uint32_t data_block_idx = 0;

    // Write the direct block indices to the inode
    for (uint32_t i = 0; i < KTFS_NUM_DIRECT_DATA_BLOCKS && i < num_data_blocks; i++) {
        inode.block[i] = data_block_indices[data_block_idx++];
    }

    // Write the indirect block
    if (data_block_idx < num_data_blocks) {
        uint32_t indirect_block = current_data_block++;
        update_bitmap(fp, 1 + num_inode_bitmap_blocks, num_starter_blocks + indirect_block, KTFS_FILE_IN_USE);

        uint32_t indirect_block_buf[KTFS_BLKSZ / sizeof(uint32_t)] = {0};
        for (uint32_t i = 0; i < KTFS_BLKSZ / sizeof(uint32_t) && data_block_idx < num_data_blocks; i++) {
            indirect_block_buf[i] = data_block_indices[data_block_idx++];
        }

        fseek(fp, (num_starter_blocks + indirect_block) * KTFS_BLKSZ, SEEK_SET);
        fwrite(indirect_block_buf, 1, KTFS_BLKSZ, fp);
        inode.indirect = indirect_block;
    }

    // Write the doubly-indirect blocks
    for (int k = 0; k < KTFS_NUM_DINDIRECT_BLOCKS; k++) {
        if (data_block_idx < num_data_blocks) {
            uint32_t dindirect_block = current_data_block++;
            update_bitmap(fp, 1 + num_inode_bitmap_blocks, num_starter_blocks + dindirect_block, KTFS_FILE_IN_USE);

            uint32_t dindirect_block_buf[KTFS_BLKSZ / sizeof(uint32_t)] = {0};
            for (uint32_t i = 0; i < KTFS_BLKSZ / sizeof(uint32_t) && data_block_idx < num_data_blocks; i++) {
                uint32_t indirect_block = current_data_block++;
                update_bitmap(fp, 1 + num_inode_bitmap_blocks, num_starter_blocks + indirect_block, KTFS_FILE_IN_USE);

                uint32_t indirect_block_buf[KTFS_BLKSZ / sizeof(uint32_t)] = {0};
                for (uint32_t j = 0; j < KTFS_BLKSZ / sizeof(uint32_t) && data_block_idx < num_data_blocks; j++) {
                    indirect_block_buf[j] = data_block_indices[data_block_idx++];
                }

                fseek(fp, (num_starter_blocks + indirect_block) * KTFS_BLKSZ, SEEK_SET);
                fwrite(indirect_block_buf, 1, KTFS_BLKSZ, fp);

                dindirect_block_buf[i] = indirect_block;
            }

            fseek(fp, (num_starter_blocks + dindirect_block) * KTFS_BLKSZ, SEEK_SET);
            fwrite(dindirect_block_buf, 1, KTFS_BLKSZ, fp);
            inode.dindirect[k] = dindirect_block;
        }
    }

    // Mark the inode as in use
    update_bitmap(fp, 1, current_inode, KTFS_FILE_IN_USE);

    write_inode(fp, &inode);
    free(data_block_indices);
    fflush(fp);
    fclose(binary_fp);

    fprintf(stdout, "Added file %s to inode %d\n", binary_path, current_inode - 1);
    fprintf(stdout, "File size: %d bytes\n", file_size);
    fprintf(stdout, "File start data block index: %d\n", inode.block[0]);
    fprintf(stdout, "File start address: %d\n", (num_starter_blocks + inode.block[0]) * KTFS_BLKSZ);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s [filesystem_image] [disk_size in K, M, or G] [inode_count] [file1] [file2] ...\n", argv[0]);
        return 1;
    }

    const char *output_file = argv[1];
    uint32_t disk_size = parse_size(argv[2]);
    uint32_t inode_count = atoi(argv[3]);
    uint32_t block_count = disk_size / KTFS_BLKSZ;

    num_bitmap_blocks = ceil((double)block_count / (KTFS_BLKSZ * 8));
    num_inode_blocks = ceil((double)inode_count * KTFS_INOSZ / KTFS_BLKSZ);
    num_inode_bitmap_blocks = ceil((double)inode_count / KTFS_BLKSZ);

    num_starter_blocks = 1 + num_inode_bitmap_blocks + num_bitmap_blocks + num_inode_blocks;

    if (inode_count < argc - 4) {
        fprintf(stderr, "Not enough inodes for the provided files\n");
        return 1;
    } else if (block_count - num_starter_blocks < inode_count) {
        fprintf(stderr, "Disk size too small for number of inodes\n");
        return 1;
    }

    FILE *fp = fopen(output_file, "w+b");
    if (!fp) {
        perror("Failed to open output file");
        return 1;
    }

    // Write an empty disk-size file
    fseek(fp, disk_size - 1, SEEK_SET);
    fputc('\0', fp);

    // Write the superblock
    struct ktfs_superblock sb = {0};
    sb.block_count = block_count;
    sb.inode_bitmap_block_count = num_inode_bitmap_blocks;
    sb.inode_block_count = num_inode_blocks;
    sb.bitmap_block_count = num_bitmap_blocks;
    sb.root_directory_inode = 0; // Root directory inode is always 0
    fseek(fp, 0, SEEK_SET);
    if (!fwrite(&sb, sizeof(sb), 1, fp)) {
        perror("Failed to write");
    };

    // Write 0s to the bitmap blocks and inode blocks
    for (int i = 0; i < num_starter_blocks; i++) {
        uint8_t buffer[KTFS_BLKSZ] = {0};
        fwrite(buffer, KTFS_BLKSZ, 1, fp);
    }

    // Mark the superblock, bitmap blocks, and inode blocks as in use
    for (int i = 0; i < num_starter_blocks; i++) {
        update_bitmap(fp, 1 + num_inode_bitmap_blocks, i, KTFS_FILE_IN_USE);
    }  

    current_inode = 1;
    current_dentry = 0;
    current_data_block = 0;
    remaining_disk_size = disk_size - (num_starter_blocks * KTFS_BLKSZ);

    // Load each file given
    for (int i = 4; i < argc; i++) {
        load_binary(fp, argv[i]);
    }

    // Write the root directory inode
    fseek(fp, (1 + num_inode_bitmap_blocks + num_bitmap_blocks) * KTFS_BLKSZ, SEEK_SET);
    fwrite(&root_inode, KTFS_INOSZ, 1, fp);
    fflush(fp);

    // Mark the root directory inode as in use
    update_bitmap(fp, 1, 0, KTFS_FILE_IN_USE);

    // Mark all inodes above the max as in use
    for (int i = inode_count; i < num_inode_bitmap_blocks * KTFS_BLKSZ; i++) {
        update_bitmap(fp, 1, i, KTFS_FILE_IN_USE);
    }

    fclose(fp);
    fprintf(stdout, "Filesystem image created successfully: %s\n", output_file);
    fprintf(stdout, "Disk size: %d bytes\n", disk_size);
    return 0;
}
