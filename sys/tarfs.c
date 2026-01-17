// tarfs.c - A file system backed by a tar archive
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef TARFS_TRACE
#define TRACE
#endif

#ifdef TARFS_DEBUG
#define DEBUG
#endif

#include "tarfs.h"

#include "fsimpl.h"
#include "error.h"
#include "heap.h"
#include "string.h"
#include "limits.h"
#include "ioimpl.h"
#include "thread.h"
#include "cache.h"
#include "misc.h"

// INTERNAL CONSTANT DEFINITIONS
//

#define TAR_BLKSZ 512

// INTERNAL TYPE DEFINITIONS
//

struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char csum[8];
    char type;
    char link[100];
    char ustar[6];
    char version[2];
};

struct tarfs_file {
    struct tarfs_file * next;     // linked list next pointer
    unsigned long size;     // size of the file
    unsigned long blkno;    // block number of first block
    unsigned long refcnt;   // number of io objects for file (informational only)
    char name[];
};

struct tarfs_fileio {
    struct seekio io;
    struct tarfs_file * file;
};

struct tarfs_lsio {
    struct io io;
    struct tarfs_file * file;
};

// INTERNAL FUNCTION DECLARATIONS
//


static int tarfs_open_file(struct filesystem * fs, const char * name, struct io ** ioptr);

static void tarfs_file_reclaim(struct io * io);

static long tarfs_file_fetch(struct io * io, unsigned long long pos, void * buf, long bufsz);

static long tarfs_file_store(struct io * io, unsigned long long pos, const void * buf, long len);

static int tarfs_file_ioctl(struct io * io, int cmd, void * arg);

static int tarfs_open_listing(struct filesystem * fs, struct io ** ioptr);

static void tarfs_listing_reclaim(struct io * io);

static long tarfs_listing_read(struct io * io, void * buf, long bufsz);

static void tarfs_flush(struct filesystem * fs);

// INTERNAL GLOBAL VARIABLES
//

struct io * tario;                  // backing device
struct cache * tar_cache;           // block cache
static struct tarfs_file * files;         // linked list of files

static struct filesystem tarfs = {
    .open_file = &tarfs_open_file,
    .open_listing = &tarfs_open_listing,
    .flush = &tarfs_flush
};

static const struct iointf tarfs_file_intf = {
    .implname = "tarfs_fileio",
    .reclaim = &tarfs_file_reclaim,
    .ioctl = &tarfs_file_ioctl,
    .fetch = &tarfs_file_fetch,
    .store = &tarfs_file_store
};

// EXPORTED FUNCTION DEFINITIONS
//

int mount_tarfs(const char * mpname, struct io * io) {
    char blkbuf[TAR_BLKSZ]; // block buffer
    struct ustar_header * const hdr = (void*)blkbuf;
    char buf[16]; // for null-terminating strings
    unsigned int zblkcnt = 0; // no. of zero blocks
    char * end; // for strtoul
    unsigned long long devsz;
    struct tarfs_file * file;
    int result;
    long rlen;
    size_t namelen;
    char * namestr;
    unsigned long blkno;
    unsigned long blkcnt;
    int blksz;

    trace("%s(%s,%p)", __func__, mpname, io);

    tario = ioaddref(io);
    blksz = ioblksz(tario);
    assert (0 <= blksz);

    if (TAR_BLKSZ % blksz != 0) {
        debug("Incompatible device block size: %d", blksz);
        return -ENOTSUP;
    }

    if (CACHE_BLKSZ % TAR_BLKSZ != 0) {
        debug("Incompatible cache block size: %d", CACHE_BLKSZ);
        return -ENOTSUP;
    }

    result = ioctl(tario, IOC_GETEND, &devsz);

    if (result != 0) {
        debug("ioctl(IOCTL_GETEND) returned %d", result);
        return result;
    }

    blkno = 0;
    while (blkno < devsz / TAR_BLKSZ) {
        debug("Reading tar header in block %lu", blkno);
        rlen = iofetch(tario, 1ULL * blkno * TAR_BLKSZ, blkbuf, TAR_BLKSZ);
        blkno += 1;

        if (rlen < 0)
            return rlen;
        if (rlen != TAR_BLKSZ)
            return -EIO;
        
        if (hdr->name[0] == '\0') {
            // If name is blank, treat this as an all-zero block
            zblkcnt += 1;
            
            if (zblkcnt <= 1)
                continue;
            else
                break;
        } else
            zblkcnt = 0;
        
        hdr->name[100] = '\0';
        namestr = strrchr(hdr->name, '/');

        if (namestr == NULL)
            namestr = hdr->name;
        else
            namestr += 1;

        namelen = strlen(namestr);

        if (HEAP_ALLOC_MAX - sizeof(struct tarfs_file) - 1 < namelen) {
            debug("File name too long (%zu bytes)", namelen);
            return -EBADFMT;
        }

        file = kcalloc(1, sizeof(struct tarfs_file) + namelen + 1);
        strncpy(file->name, namestr, namelen+1);

        end = NULL;
        memset(buf, 0, sizeof(buf));
        memcpy(buf, hdr->size, sizeof(hdr->size));
        file->size = strtoul(buf, &end, 8);
        if (buf[0] == '\0' || end == NULL || *end != '\0')
            return -EBADFMT;
        
        debug("File %s has size %lu", file->name, file->size);

        file->blkno = blkno;
        blkcnt = (file->size + TAR_BLKSZ-1) / TAR_BLKSZ;

        if (ULONG_MAX - blkno < blkcnt) {
            debug("File %s too large", file->name);
            return -EBADFMT;
        }

        blkno += blkcnt;

        if (devsz / TAR_BLKSZ < blkno) {
            debug("File %s too large", file->name);
            return -EBADFMT;
        }

        file->next = files;
        files = file;
    }

    tar_cache = create_cache(tario);

    if (tar_cache != 0)
        panic("Failed to create cache");

    return attach_filesystem(mpname, &tarfs);
}

int tarfs_open_file(struct filesystem * fs, const char * name, struct io ** ioptr) {
    trace("%s(\"%s\")", __func__, name);

    struct tarfs_fileio * fio;
    struct tarfs_file * file;

    for (file = files; file != NULL; file = file->next)
        if (strcmp(name, file->name) == 0)
            break;
    
    if (file == NULL)
        return -ENOENT;
    
    fio = kmalloc(sizeof(*fio));
    fio->file = file;

    *ioptr = seekio_init(&fio->io, &tarfs_file_intf, file->size, 1, 1);

    return 0;
}

void tarfs_flush(struct filesystem * fs) {
    cache_flush(tar_cache);
}


void tarfs_file_reclaim(struct io * io) {
    struct tarfs_fileio * const fio =
        (void*)io - offsetof(struct tarfs_fileio, io);
    trace("%s(io=%p)", __func__, io);
    kfree(fio);
}

long tarfs_file_fetch(struct io * io, unsigned long long pos, void * buf, long bufsz) {
    struct tarfs_fileio * const fio =
        (void*)io - offsetof(struct tarfs_fileio, io);
    unsigned long blkno;
    unsigned long off;
    size_t cpycnt;
    void * bufp;
    void * blk;
    int result;

    trace("%s(io=%p, buf=%p, bufsz=%zu)", __func__, io, buf, bufsz);

    // Trim request so we don't try to read past end of file

    if (fio->file->size - pos < bufsz)
        bufsz = fio->file->size - pos;

    bufp = buf; // pointer to place in buffer where we will write next byte
        
    while (bufsz > 0) {
        blkno = (fio->file->blkno * TAR_BLKSZ + pos) / CACHE_BLKSZ;
        off = pos % CACHE_BLKSZ;   // offset from where we want data

        result = cache_fetch(tar_cache, blkno * CACHE_BLKSZ, &blk);
        
        if (result < 0)
            return result;

        if (CACHE_BLKSZ-off < bufsz)
            cpycnt = CACHE_BLKSZ-off;
        else
            cpycnt = bufsz;
        
        memcpy(bufp, blk+off, cpycnt);
        cache_release(tar_cache, blk, /* dirty */ 0);

        pos += cpycnt;
        bufp += cpycnt;
        bufsz -= cpycnt;
    }

    return bufp - buf;
}

long tarfs_file_store(struct io * io, unsigned long long pos, const void * buf, long len) {
    struct tarfs_fileio * const fio =
        (void*)io - offsetof(struct tarfs_fileio, io);
    unsigned long blkno;
    unsigned long off;
    size_t cpycnt;
    const void * bufp;
    void * blk;
    int result;

    // Trim request so we don't write past the end of the file. With a little
    // more work, we could allow writes up to the end of the last block,
    // however, we would then need to update the file header too.

    if (fio->file->size - pos < len)
        len = fio->file->size - pos;

    bufp = buf; // pointer to place in buffer from where we will read next byte
        
    while (len > 0) {
        blkno = (fio->file->blkno * TAR_BLKSZ + pos) / CACHE_BLKSZ;
        off = pos % TAR_BLKSZ;   // offset from where we want data

        result = cache_fetch(tar_cache, blkno * CACHE_BLKSZ, &blk);

        if (result < 0)
            return result;

        if (CACHE_BLKSZ-off < len)
            cpycnt = CACHE_BLKSZ-off;
        else
            cpycnt = len;
        
        memcpy(blk+off, bufp, cpycnt);

       cache_release(tar_cache, blk, /* dirty */ 1);

        pos += cpycnt;
        bufp += cpycnt;
        len -= cpycnt;
    }

    return bufp - buf;
}

int tarfs_file_ioctl(struct io * io, int cmd, void * arg) {
    struct tarfs_fileio * const fio =
        (void*)io - offsetof(struct tarfs_fileio, io);
    unsigned long long * ullarg = arg;

    switch (cmd) {
    case IOC_GETEND:
        *ullarg = fio->file->size;
        return 0;
    default:
        return -ENOTSUP;
    }
}

int tarfs_open_listing(struct filesystem * fs, struct io ** ioptr) {
    static const struct iointf listing_intf = {
        .reclaim = &tarfs_listing_reclaim,
        .read = &tarfs_listing_read
    };

    struct tarfs_lsio * lsio;

    lsio = kcalloc(1, sizeof(struct tarfs_lsio));
    lsio->file = files;
    *ioptr = ioinit(&lsio->io, &listing_intf, 1, 1);
    return 0;
}

void tarfs_listing_reclaim(struct io * io) {
    struct tarfs_lsio * const lsio =
        (void*)io - offsetof(struct tarfs_lsio, io);
    kfree(lsio);
}

long tarfs_listing_read(struct io * io, void * buf, long bufsz) {
    struct tarfs_lsio * const lsio =
        (void*)io - offsetof(struct tarfs_lsio, io);
    char * const cbuf = buf;
    struct tarfs_file * file;
    unsigned long i;
    int c;

    if (bufsz == 0)
        return 0;

    if (lsio->file != NULL) {
        file = lsio->file;
        lsio->file = file->next;
        for (i = 0; i < bufsz; i++) {
            c = file->name[i];
            cbuf[i] = c;
            if (c == '\0')
                return i+1;
        }

        cbuf[bufsz-1] = '\0';
        return bufsz;
    } else
        return 0;
}
