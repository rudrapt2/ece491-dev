#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "../../sys/ktfs.h"

uint32_t num_starter_blocks;

void read_block(FILE *image, void *buffer, uint32_t block_num) {
    fseek(image, block_num * KTFS_BLKSZ, SEEK_SET);
    fread(buffer, KTFS_BLKSZ, 1, image);
}

void write_file(FILE *image, struct ktfs_inode *inode, const char *filename) {
    FILE *output = fopen(filename, "wb");
    if (!output) {
        perror("Error opening output file");
        return;
    }

    uint32_t bytes_written = 0;
    uint8_t buffer[KTFS_BLKSZ];

    // Write direct blocks
    for (int i = 0; i < KTFS_NUM_DIRECT_DATA_BLOCKS && bytes_written < inode->size; i++) {
        read_block(image, buffer, num_starter_blocks + inode->block[i]);
        uint32_t to_write = (inode->size - bytes_written < KTFS_BLKSZ) ? inode->size - bytes_written : KTFS_BLKSZ;
        fwrite(buffer, 1, to_write, output);
        bytes_written += to_write;
    }

    // Write indirect blocks
    if (bytes_written < inode->size && inode->indirect) {
        uint32_t indirect_blocks[KTFS_BLKSZ / sizeof(uint32_t)];
        read_block(image, indirect_blocks, num_starter_blocks + inode->indirect);
        for (int i = 0; i < KTFS_BLKSZ / sizeof(uint32_t) && bytes_written < inode->size; i++) {
            read_block(image, buffer, num_starter_blocks + indirect_blocks[i]);
            uint32_t to_write = (inode->size - bytes_written < KTFS_BLKSZ) ? inode->size - bytes_written : KTFS_BLKSZ;
            fwrite(buffer, 1, to_write, output);
            bytes_written += to_write;
        }
    }

    // Write doubly-indirect blocks
    for (int k = 0; k < KTFS_NUM_DINDIRECT_BLOCKS && bytes_written < inode->size; k++) {
        if (bytes_written < inode->size && inode->dindirect[k]) {
            uint32_t dindirect_blocks[KTFS_BLKSZ / sizeof(uint32_t)];
            read_block(image, dindirect_blocks, num_starter_blocks + inode->dindirect[k]);
            for (int i = 0; i < KTFS_BLKSZ / sizeof(uint32_t) && bytes_written < inode->size; i++) {
                uint32_t indirect_blocks[KTFS_BLKSZ / sizeof(uint32_t)];
                read_block(image, indirect_blocks, num_starter_blocks + dindirect_blocks[i]);
                for (int j = 0; j < KTFS_BLKSZ / sizeof(uint32_t) && bytes_written < inode->size; j++) {
                    read_block(image, buffer, num_starter_blocks + indirect_blocks[j]);
                    uint32_t to_write = (inode->size - bytes_written < KTFS_BLKSZ) ? inode->size - bytes_written : KTFS_BLKSZ;
                    fwrite(buffer, 1, to_write, output);
                    bytes_written += to_write;
                }
            }
        }
    }

    fprintf(stdout, "Extracted file: %s\n", filename);
    fprintf(stdout, "File size: %d bytes\n", inode->size);
    fclose(output);
}

void extract_directory(FILE *image, struct ktfs_superblock *sb, uint16_t dir_inode, const char *path) {
    uint8_t buf[KTFS_BLKSZ];
    // read the inode block
    read_block(image, &buf, 1 + sb->inode_bitmap_block_count + sb->bitmap_block_count + dir_inode / (KTFS_BLKSZ / KTFS_INOSZ));

    struct ktfs_inode root_inode;
    // copy the inode from the inode block to root_inode
    memcpy(&root_inode, &buf + dir_inode % (KTFS_BLKSZ / KTFS_INOSZ), KTFS_INOSZ);

    uint8_t dir_buf[KTFS_BLKSZ];
    for (uint32_t i = 0; i < root_inode.size / KTFS_DENSZ; i++) {
        // read the dictionary entry
        read_block(image, &dir_buf, num_starter_blocks + root_inode.block[i / (KTFS_BLKSZ / KTFS_DENSZ)]);
        struct ktfs_dir_entry *entry = (struct ktfs_dir_entry *)(dir_buf + (i % (KTFS_BLKSZ / KTFS_DENSZ)) * KTFS_DENSZ);

        char entry_path[strlen(path) + strlen(entry->name) + 2];
        sprintf(entry_path, "%s/%s", path, entry->name);

        struct ktfs_inode inode_buf[KTFS_BLKSZ / KTFS_INOSZ];
        read_block(image, &inode_buf, 1 + sb->inode_bitmap_block_count + sb->bitmap_block_count + entry->inode / (KTFS_BLKSZ / KTFS_INOSZ));

        struct ktfs_inode entry_inode;
        memcpy(&entry_inode, &inode_buf[entry->inode % (KTFS_BLKSZ / KTFS_INOSZ)], KTFS_INOSZ);

        if (entry_inode.size == 0) {
            mkdir(entry_path, 0755);
        } else {
            write_file(image, &entry_inode, entry_path);
        }
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

    struct ktfs_superblock sb;
    fread(&sb, sizeof(struct ktfs_superblock), 1, image);

    num_starter_blocks = 1 + sb.inode_bitmap_block_count + sb.bitmap_block_count + sb.inode_block_count;

    mkdir(argv[1], 0755);
    extract_directory(image, &sb, sb.root_directory_inode, argv[1]);

    fclose(image);
    printf("Filesystem extraction complete.\n");
    return 0;
}
