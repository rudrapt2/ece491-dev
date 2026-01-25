#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "../../sys/fs/ngfs.h"

#define CEIL(n, k) (((n) + (k) - 1) / (k))
#define DENTRIES_PER_BLOCK      (NGFS_BLKSZ / sizeof(struct ngfs_dir_entry))

uint32_t num_fat_blocks;
struct ngfs_dir_entry root_dentry;

void read_block(FILE *image, void *buffer, uint32_t block_num) {
    fseek(image, block_num * NGFS_BLKSZ, SEEK_SET);
    fread(buffer, NGFS_BLKSZ, 1, image);
}

uint32_t get_next_block(FILE *image, uint32_t block) {
    uint32_t fat_index = block / NGFS_FAT_ENTRIES_PER_BLOCK;
    uint32_t block_idx = block % NGFS_FAT_ENTRIES_PER_BLOCK;

    uint32_t next;
    
    fseek(image, fat_index*NGFS_BLKSZ + block_idx*sizeof(uint32_t), SEEK_SET);
    fread(&next, sizeof(uint32_t), 1, image);

    return next;
}

void write_file(FILE *image, struct ngfs_dir_entry *entry, const char *filename) {
    FILE *output = fopen(filename, "wb");
    if (!output) {
        perror("Error opening output file");
        return;
    }

    uint32_t bytes_written = 0;
    uint8_t buffer[NGFS_BLKSZ];

    for (uint32_t block = entry->start_block; bytes_written < entry->size; block = get_next_block(image, block)) {
        if (block == NGFS_BLOCK_END) {
            fprintf(stderr, "File %s block number and size do not match\n", entry->name);
            return;
        }
        if (block == NGFS_BLOCK_FREE) {
            fprintf(stderr, "File %s is not well-formed\n", entry->name);
            return;
        }
        read_block(image, buffer, num_fat_blocks + block);
        uint32_t to_write = (entry->size - bytes_written < NGFS_BLKSZ) ? entry->size - bytes_written : NGFS_BLKSZ;
        fwrite(buffer, 1, to_write, output);
        bytes_written += to_write;
    }

    fprintf(stdout, "Extracted file: %s\n", filename);
    fprintf(stdout, "File size: %d bytes\n", entry->size);
    fclose(output);
}

void extract_directory(FILE *image, struct ngfs_dir_entry * directory, const char *path) {
    struct ngfs_dir_entry dir_buf[DENTRIES_PER_BLOCK];

    uint32_t dentry_data_block = directory->start_block;
    read_block(image, (void*)dir_buf, num_fat_blocks + dentry_data_block);

    // i = 0 is dir info
    for (uint32_t i = 1; i < directory->size / sizeof(struct ngfs_dir_entry); i++) {
        if (i % DENTRIES_PER_BLOCK == 0) {
            dentry_data_block = get_next_block(image, dentry_data_block);
            if (dentry_data_block == NGFS_BLOCK_END) {
                fprintf(stderr, "Root dentry size and num blocks disagree\n");
                return;
            }
            if (dentry_data_block == NGFS_BLOCK_FREE) {
                fprintf(stderr, "Root directory is not well-formed\n");
                return;
            }
            read_block(image, (void*)dir_buf, num_fat_blocks + dentry_data_block);
        }
        struct ngfs_dir_entry *entry = &dir_buf[i % DENTRIES_PER_BLOCK];

        fprintf(stdout, "entry %d has filename: %s\n", i, entry->name);

        if (strlen(entry->name) == 0) {
            fprintf(stderr, "File %d has an invalid name\n", i);
            continue;
        }
        char entry_path[strlen(path) + strlen(entry->name) + 2];
        sprintf(entry_path, "%s/%s", path, entry->name);

        write_file(image, entry, entry_path);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [path/to/output/dir] [filesystem_image]\n", argv[0]);
        return 1;
    }

    FILE *image = fopen(argv[2], "rb");
    if (!image) {
        perror("Error opening filesystem image");
        return 1;
    }

    fseek(image, 0L, SEEK_END);
    num_fat_blocks = CEIL((ftell(image) / NGFS_BLKSZ), NGFS_FAT_ENTRIES_PER_BLOCK);
    fseek(image, num_fat_blocks * NGFS_BLKSZ, SEEK_SET);
    fread(&root_dentry, sizeof(root_dentry), 1, image);

    if (strncmp(root_dentry.name, ".", NGFS_MAX_FILENAME_LEN) != 0 ||
        root_dentry.start_block != 0 ||
        root_dentry.size % sizeof(struct ngfs_dir_entry) != 0) {
            perror("Root dentry is not well formed");
            return -1;
        }

    mkdir(argv[1], 0755);
    extract_directory(image, &root_dentry, argv[1]);

    fclose(image);
    printf("Filesystem extraction complete.\n");
    return 0;
}
