/*! @file filesys.c
    @brief File system interface
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#include "filesys.h"

#include <stddef.h>

#include "error.h"
#include "fsimpl.h"
#include "heap.h"
#include "misc.h"
#include "string.h"
#include "uioimpl.h"
#include "ioimpl.h"

// INTERNAL TYPE DEFINITIONS
//

/**
 * @brief Defines the mountpoints within the root file system.
 */
struct mountpoint {
    struct mountpoint * next;  ///< Next mountpoint in linked list
    struct filesystem * fs;    ///< Filesystem function interface
    char name[];              ///< Path alias for mountpoint
};

// INTERNAL FUNCTION PROTOTYPES
//

static struct filesystem * getfs(const char * mpname);
static int fsopen(struct filesystem * fs, const char * flname, struct uio ** uioptr);
static int fscreate(struct filesystem * fs, const char * flname);
static int fsdelete(struct filesystem * fs, const char * flname);
static void fsflush(struct filesystem * fs);

static int fs_open_listing(struct uio ** uioptr);
static void fs_listing_close(struct uio * uio);
static long fs_listing_read(struct uio * uio, void * buf, unsigned long bufsz);

static int nullfs_open(struct filesystem * fs, const char * flname, struct uio ** uioptr);
static void nullfs_flush(struct filesystem * fs);


// INTERNAL GLOBAL VARIABLES
//

struct root_lsio {
    struct io io;
    const struct mountpoint * fs;
};

static const struct iointf root_lsio_intf = {
    .implname = "root_ls"
    .reclaim = &fs_listing_close,
    .read = &fs_listing_read
};

static const struct filesystem nullfs = {
    .open = &nullfs_open,
    .flush = &nullfs_flush
};

// Linked list of mounted filesystems

static struct mountpoint * mplist;

// EXPORTED GLOBAL VARIABLES
//

char fsmgr_initialized = 0;

// EXTERNAL FUNCTION DEFINITIONS
//

int fsmgr_init(void) {
    fsmgr_initialized = 1;
    return 0;
}

/**
 * @brief Flushes all mounted filesystems
 */
void fsmgr_flushall(void) {
    struct mountpoint * mp;

    for (mp = mplist; mp != NULL; mp = mp->next)
        fsflush(mp->fs);
}

int open_file(const char * mpname, const char * flname, struct uio ** uioptr) {
    struct filesystem * fs;

    trace("%s(%s/%s)", __func__, mpname, flname);

    assert(mpname != NULL || flname == NULL);

    if (mpname == NULL || *mpname == '\0')
        return fs_open_listing(uioptr);

    fs = getfs(mpname);

    return (fs != NULL) ? fsopen(fs, flname, uioptr) : -ENOENT;
}

int create_file(const char * mpname, const char * flname) {
    struct filesystem * fs;

    if (mpname == NULL || flname == NULL) return -EINVAL;

    fs = getfs(mpname);

    return (fs != NULL) ? fscreate(fs, flname) : -ENOENT;
}

int delete_file(const char * mpname, const char * flname) {
    struct filesystem * fs;

    if (mpname == NULL || flname == NULL) return -EINVAL;

    fs = getfs(mpname);

    return (fs != NULL) ? fsdelete(fs, flname) : -ENOENT;
}

int fs_open_listing(struct uio ** uioptr) {
    struct fs_listing_uio * ls;

    ls = kcalloc(1, sizeof(*ls));
    ls->fs = mplist;

    // Note: The listing uio_intf is at index DEV_UNDEF in /devfs_uio_intfs/.
    *uioptr = uio_init1(&ls->base, &fs_listing_uio_intf);

    return 0;
}

void fs_listing_close(struct uio * uio) {
    struct fs_listing_uio * const ls = (struct fs_listing_uio *)uio;
    kfree(ls);
}

long fs_listing_read(struct uio * uio, void * buf, unsigned long bufsz) {
    struct fs_listing_uio * const ls = (struct fs_listing_uio *)uio;
    size_t len;

    if (ls->fs != NULL) {
        len = strlen(ls->fs->name);
        strncpy(buf, ls->fs->name, bufsz);
        ls->fs = ls->fs->next;
        return (len < bufsz) ? len : bufsz;
    } else
        return 0;
}

int mount_nullfs(const char * name) {
    return attach_filesystem(name, (struct filesystem *)&nullfs);
}

int attach_filesystem(const char * mpname, struct filesystem * fs) {
    struct mountpoint ** mpptr;
    struct mountpoint * mp;
    size_t namelen;

    mpptr = &mplist;
    while ((mp = *mpptr) != NULL) {
        if (strcmp(mp->name, mpname) == 0) return -EEXIST;
        mpptr = &mp->next;
    }

    namelen = strlen(mpname);
    mp = kmalloc(sizeof(*mp) + namelen + 1);
    memset(mp, 0, sizeof(*mp));

    strncpy(mp->name, mpname, namelen + 1);
    mp->fs = fs;
    *mpptr = mp;

    return 0;
}

// INTERNAL FUNCTION DEFINITIONS
//

struct filesystem * getfs(const char * mpname) {
    struct mountpoint * mp;

    for (mp = mplist; mp != NULL; mp = mp->next) {
        if (strcmp(mp->name, mpname) == 0) return mp->fs;
    }

    return NULL;
}

int fsopen(struct filesystem * fs, const char * flname, struct uio ** uioptr) {
    if (fs->open != NULL)
        return fs->open(fs, flname, uioptr);
    else
        return -ENOTSUP;
}

int fscreate(struct filesystem * fs, const char * flname) {
    if (fs->create != NULL)
        return fs->create(fs, flname);
    else
        return -ENOTSUP;
}

int fsdelete(struct filesystem * fs, const char * flname) {
    if (fs->delete != NULL)
        return fs->delete(fs, flname);
    else
        return -ENOTSUP;
}

void fsflush(struct filesystem * fs) {
    if (fs->flush != NULL) fs->flush(fs);
}

int nullfs_open(struct filesystem * fs __attribute__((unused)), const char * flname,
                struct uio ** uioptr) {
    return -ENOENT;
}

void nullfs_flush(struct filesystem * fs __attribute__((unused))) {
    // nothing
}

int parse_path(char * path, char ** mpnameptr, char ** flnameptr){
    if (path == NULL || mpnameptr == NULL || flnameptr == NULL) {
        return -EINVAL; // invalid args
    }

    while (*path == '/') path++; // ignore all leading slashes

    char *slash = strchr(path, '/');
    if (slash != NULL) {
        *flnameptr = slash + 1;
        *slash = '\0';
    } else // no slashes indicates mp only
        *flnameptr = NULL;

    *mpnameptr = path;

    return 0; // success
}
