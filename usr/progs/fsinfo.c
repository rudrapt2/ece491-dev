#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include "../error.h"
#include "../io.h"
#include <stddef.h>
#include <stdint.h>
#include "../../sys/fs/ngfs.h"

#define DENTRYSZ sizeof(struct ngfs_dir_entry)

int bkgfd;
char data[NGFS_BLKSZ];

void setpos(unsigned long long pos) {
    int ret = _ioctl(bkgfd, IOC_SETPOS, &pos);
    if (ret < 0) {
        printf("Failed to set position to %llu (%s)\n", pos, error_desc(ret));
        _exit();
    }
}

void read_data() {
    int ret = _read(bkgfd, data, NGFS_BLKSZ);
    if (ret < 0) {
        unsigned long long pos;
        _ioctl(bkgfd, IOC_GETPOS, &pos);
        printf("Failed to read at pos %llu with bufsz (%s)\n", pos, error_desc(ret));
        _exit();
    }
}

void main (int argc, char** argv)
{
    unsigned long long bkgsz;
    uint32_t num_fat_blocks;
    struct ngfs_dir_entry root_dentry;
    uint32_t free_blocks, end_blocks;
    struct ngfs_fat * fat = (void *)data;

    if (argc >= 2)
        bkgfd = _open(-1, argv[1]);
    else
        bkgfd = _open(-1, "/dev/vioblk0");

    _ioctl(bkgfd, IOC_GETEND, &bkgsz);
    num_fat_blocks = (bkgsz / NGFS_BLKSZ) / NGFS_FAT_ENTRIES_PER_BLOCK;

    setpos(num_fat_blocks * NGFS_BLKSZ);
    read_data();
    memcpy(&root_dentry, data, DENTRYSZ);

    if (strcmp(root_dentry.name, ".") == 0 || 
        root_dentry.size % DENTRYSZ != 0 ||
        root_dentry.start_block != 0) 
    {
        printf("ngfs is not well-formed\n");
        return;
    }

    setpos(0);
    for (int i = 0; i < bkgsz / NGFS_BLKSZ; i++) {
        if (i % NGFS_FAT_ENTRIES_PER_BLOCK) read_data();
        switch (fat->fat[i % NGFS_FAT_ENTRIES_PER_BLOCK]) {
        case NGFS_BLOCK_FREE:
            free_blocks++;
            continue;
        case NGFS_BLOCK_END:
            end_blocks++;
        }
    }

    dprintf(STDOUT, 
        "\n\
        NGFS FILE INFORMATION:\n\
        FILE SYSTEM SIZE:\t%llu\n\
        NUMBER OF BLOCKS:\t%u\n\
        NUM FREE BLOCKS:\t%u\n\
        NUM END BLOCKS:\t%u\n\
        NUMBER OF FILES:\t%u\n",
    bkgsz, bkgsz / NGFS_BLKSZ, free_blocks, end_blocks, root_dentry.size / DENTRYSZ);
}