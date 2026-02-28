#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>

#include "../../sys/fs/ngfs.h"

// Change this to turn off randomizing data block indices. This is provided for debugging purposes only.
// Your filesystem driver must be able to handle non-sequential data blocks.
char RANDOMIZE_ON = 0;

#define MAX_DATA_BLOCKS_PER_FILE ((1 << 30) - 1)
#define MAX_DISK_SIZE (1 << 30)

struct ngfs_dir_entry root_dir = {0};
uint32_t num_fat_blocks;

// Forward declarations
void shuffle(uint32_t* arr, size_t n);

uint32_t remaining_disk_size;

// Random allocation state
static uint32_t* data_block_order = NULL;
static uint32_t data_block_order_size = 0;
static uint32_t data_block_order_pos = 0;

static void init_allocators(uint32_t data_block_count) {
    // Data blocks available to allocate: [0, data_block_count-1]
    data_block_order_size = data_block_count;
    data_block_order = (uint32_t*)malloc(sizeof(uint32_t) * data_block_order_size);
    for (uint32_t i = 0; i < data_block_order_size; i++) data_block_order[i] = i;
    if (RANDOMIZE_ON) shuffle(data_block_order, data_block_order_size);
}

void shuffle(uint32_t* arr, size_t n)
{
  for (int i = n - 1; i > 1; i--) {
    int j = rand() % i + 1;
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

    if (size % NGFS_BLKSZ != 0) {
        fprintf(stderr, "Disk size must be a multiple of %lu bytes\n", NGFS_BLKSZ);
        exit(1);
    }

    if (size > MAX_DISK_SIZE) {
        fprintf(stderr, "Disk size exceeds maximum allowed size: %d bytes\n", MAX_DISK_SIZE);
        exit(1);
    }

    return size;
}

static uint32_t get_next_block(FILE* fp, uint32_t current_block) {
    uint32_t next_block;
    uint32_t table_no = current_block / NGFS_FAT_ENTRIES_PER_BLOCK;
    uint32_t table_offset = current_block % NGFS_FAT_ENTRIES_PER_BLOCK;
    uint64_t file_offset = table_no * NGFS_BLKSZ + table_offset * sizeof(uint32_t);

    fseek(fp, file_offset, SEEK_SET);
    fread(&next_block, sizeof(uint32_t), 1, fp);

    return next_block;
}

static void set_next_block(FILE* fp, uint32_t current_block, uint32_t next_block) {
    uint32_t table_no = current_block / NGFS_FAT_ENTRIES_PER_BLOCK;
    uint32_t table_offset = current_block % NGFS_FAT_ENTRIES_PER_BLOCK;
    uint64_t file_offset = table_no * NGFS_BLKSZ + table_offset * sizeof(uint32_t);

    fseek(fp, file_offset, SEEK_SET);
    fwrite(&next_block, sizeof(uint32_t), 1, fp);
}

static uint32_t get_last_block(FILE* fp, uint32_t start_block) {
    if (start_block == NGFS_BLOCK_END) return NGFS_BLOCK_END;
    uint32_t current_block = start_block;
    uint32_t next_block = get_next_block(fp, start_block);

    while (next_block != NGFS_BLOCK_END) {
        current_block = next_block;
        next_block = get_next_block(fp, current_block);
    }

    return current_block;
}

static uint32_t alloc_block(FILE* fp) {
    if (data_block_order_pos >= data_block_order_size) {
        fprintf(stderr, "Ran out of data blocks to allocate\n");
        exit(1);
    }
    uint32_t idx = data_block_order[data_block_order_pos++];
    set_next_block(fp, idx, NGFS_BLOCK_END);
    return idx;
}

static uint32_t append_to_block(FILE* fp, uint32_t block) {
    uint32_t new_block = alloc_block(fp);
    set_next_block(fp, block, new_block);
    return new_block;
}

static uint32_t set_start_block(FILE* fp, struct ngfs_dir_entry* entry) {
    uint32_t new_block = alloc_block(fp);
    entry->start_block = new_block;
    return new_block;
}

// Generic writer that appends 'size' bytes from 'data' into the given file.
// It allocates data and pointer blocks as needed and writes data to disk.
static void wdata_to_file(FILE* fp, struct ngfs_dir_entry* dir_entry, const uint8_t* data, uint32_t size) {
    if (size == 0) return;
    uint32_t written = 0;

    uint32_t last_block = get_last_block(fp, dir_entry->start_block);
    
    while (written < size) {
        uint32_t cur_off = dir_entry->size;
        uint32_t off_in_blk = cur_off % NGFS_BLKSZ;
        if (off_in_blk == 0) 
            last_block = (last_block == NGFS_BLOCK_END) ? 
                set_start_block(fp, dir_entry) :
                append_to_block(fp, last_block);

        uint32_t can_write = NGFS_BLKSZ - off_in_blk;
        uint32_t remain = size - written;
        uint32_t chunk = (remain < can_write) ? remain : can_write;

        fseek(fp, (num_fat_blocks + last_block) * NGFS_BLKSZ + off_in_blk, SEEK_SET);
        fwrite(data + written, 1, chunk, fp);

        written += chunk;
        dir_entry->size += chunk;
    }
}

// less generic writer that reads from a binary and copies it into the file
// runs O(N) vs rather than O(N^2)
static void copy_file(FILE* fp, struct ngfs_dir_entry * dentry, FILE * binary_fp) {
    uint8_t buffer[NGFS_BLKSZ];
    uint32_t block = dentry->start_block;
    uint32_t n = fread(buffer, 1, NGFS_BLKSZ, binary_fp);

    for (; n > 0; n = fread(buffer, 1, NGFS_BLKSZ, binary_fp)) {
        block = (block == NGFS_BLOCK_END) ? 
            set_start_block(fp, dentry) : append_to_block(fp, block);

        fseek(fp, (num_fat_blocks + block) * NGFS_BLKSZ, SEEK_SET);
        fwrite(buffer, 1, n, fp);
        dentry->size += n;
    }
}

int write_dentry(FILE* fp, struct ngfs_dir_entry* dentry) {
    wdata_to_file(fp, &root_dir, (const uint8_t*)dentry, sizeof(struct ngfs_dir_entry));
    fflush(fp);
    return 0;
}

static void update_root_dentry(FILE* fp) {
    // root dentry is always the first dentry
    fseek(fp, num_fat_blocks * NGFS_BLKSZ, SEEK_SET);
    fwrite((const uint8_t*)&root_dir, sizeof(root_dir), 1, fp);
    fflush(fp);
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

    if (file_size > remaining_disk_size) {
        fprintf(stderr, "Not enough space remaining on disk to store file %s\n", binary_path);
        fclose(binary_fp);
        return;
    }

    struct ngfs_dir_entry dentry = {0};
    dentry.start_block = NGFS_BLOCK_END;

    memset(dentry.name, 0, NGFS_MAX_FILENAME_LEN);
    strncpy(dentry.name, get_filename(binary_path), NGFS_MAX_FILENAME_LEN);

    copy_file(fp, &dentry, binary_fp);

    write_dentry(fp, &dentry);

    fflush(fp);
    fclose(binary_fp);

    fprintf(stdout, "Added file %s\n", binary_path);
    fprintf(stdout, "File size: %d bytes\n", file_size);
    fprintf(stdout, "File start data block index: %d\n", dentry.start_block);
    fprintf(stdout, "File start address: %lu\n", (num_fat_blocks + dentry.start_block) * NGFS_BLKSZ);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s [-R] [filesystem_image] [disk_size in K, M, or G] [file1] [file2] ...\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-R") == 0) {
        // Enable RANDOMIZE_ON
        RANDOMIZE_ON = 1;
        // Shift arguments
        argv++;
        argc--;
    }

    const char *output_file = argv[1];
    uint32_t disk_size = parse_size(argv[2]);
    uint32_t block_count = disk_size / NGFS_BLKSZ;

    num_fat_blocks = (block_count + NGFS_FAT_ENTRIES_PER_BLOCK - 1) / NGFS_FAT_ENTRIES_PER_BLOCK;

    FILE *fp = fopen(output_file, "w+b");
    if (!fp) {
        perror("Failed to open output file");
        return 1;
    }

    // Write an empty disk-size file
    fseek(fp, disk_size - 1, SEEK_SET);
    fputc('\0', fp);

    // Write 0s to the FAT
    fseek(fp, 0, SEEK_SET);
    for (int i = 0; i < num_fat_blocks; i++) {
        uint8_t buffer[NGFS_BLKSZ] = {0};
        fwrite(buffer, NGFS_BLKSZ, 1, fp);
    }

    // Indicate out-of-scope entries in FAT
    fseek(fp, (block_count - num_fat_blocks) * sizeof(uint32_t), SEEK_SET);
    for (int i = block_count - num_fat_blocks; i < num_fat_blocks; i++) {
        uint32_t buffer = NGFS_BLOCK_END;
        fwrite(&buffer, sizeof(buffer), 1, fp);
    }

    remaining_disk_size = disk_size - (num_fat_blocks * NGFS_BLKSZ);

    // Initialize random seed and allocators
    srand(time(NULL));
    init_allocators((block_count - num_fat_blocks - 1));

    // Write the root dir entry
    root_dir.name[0] = '.'; // "." for self
    root_dir.start_block = NGFS_BLOCK_END;
    write_dentry(fp, &root_dir);

    // Load each file given
    for (int i = 3; i < argc; i++) {
        load_binary(fp, argv[i]);
    }

    // Now we know the size, update root dentry
    update_root_dentry(fp);

    fclose(fp);
    fprintf(stdout, "Filesystem image created successfully: %s\n", output_file);
    fprintf(stdout, "Disk size: %d bytes\n", disk_size);
    return 0;
}
