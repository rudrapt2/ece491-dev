/*! @file lffs.h
    @brief LFFS Header File. 
    @copyright Copyright (c) 2025-2026 University of Illinois

*/

#include <stdint.h>
#include "uioimpl.h"

#define LFFS_BLKSZ                  4096UL
#define LFFS_DENSZ                  32UL
#define LFFS_MAX_FILENAME_LEN       (LFFS_DENSZ  - sizeof(uint8_t) - 2*sizeof(uint32_t))
#define LFFS_FAT_ENTRIES_PER_BLOCK  (LFFS_BLKSZ / sizeof(uint32_t))
#define LFFS_ROOT_DATA_BLOCK 0

#define LFFS_BLOCK_FREE (uint32_t)0
#define LFFS_BLOCK_END (uint32_t)(-1)

/*
Overall filesystem image layout

+------------------+
|       FAT        |
+------------------+
|       FAT        |
+------------------+
|      ...         |
+------------------+
|  ROOT DIR BLK 0  |
+------------------+
|    Data Blk 1    |
+------------------+
|      ...         |
+------------------+

NOTE: The ((packed)) attribute is used to ensure that the struct is packed and 
there is no padding between the members or struct alignment requirements.
*/

struct lffs_dir_entry {
    char name[LFFS_MAX_FILENAME_LEN+sizeof(uint8_t)]; 
    uint32_t size;
    uint32_t start_block;
} __attribute__((packed));

struct lffs_data_block {
    uint8_t data[LFFS_BLKSZ];
}__attribute__((packed));

struct lffs_fat {
    uint32_t fat[LFFS_FAT_ENTRIES_PER_BLOCK];
}__attribute__((packed));
