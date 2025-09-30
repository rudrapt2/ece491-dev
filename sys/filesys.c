// fs.c - Default empty file system
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "filesys.h"
#include "fsimpl.h"
#include "error.h"
#include "string.h"
#include "misc.h"
#include "heap.h"

#include <stddef.h>

// INTERNAL TYPE DEFINITIONS
//

struct mountpoint {
    struct mountpoint * next;
    struct filesystem * fs;
    char name[];
};

// INTERNAL FUNCTION PROTOTYPES
//

static struct filesystem * getfs(const char * mpname);
static int fsopen(struct filesystem * fs, const char * flname, struct uio ** uioptr);
static int fscreat(struct filesystem * fs, const char * flname);
static int fsdelete(struct filesystem * fs, const char * flname);
static void fsflush(struct filesystem * fs);

static int nullfs_open(struct filesystem * fs, const char * flname, struct uio ** uioptr);
static void nullfs_flush(struct filesystem * fs);

// INTERNAL GLOBAL VARIABLES
//

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

void fsmgr_flushall(void) {
    struct mountpoint * mp;

    for (mp = mplist; mp != NULL; mp = mp->next)
        fsflush(mp->fs);
}

int open_file(const char * mpname, const char * flname, struct uio ** uioptr) {
    struct filesystem * fs;

    trace("%s(%s/%s)", __func__, mpname, flname);

    assert (mpname != NULL && flname != NULL);

    fs = getfs(mpname);

    return (fs != NULL) ? fsopen(fs, flname, uioptr) : -ENOENT;
}

int create_file(const char * mpname, const char * flname) {
    struct filesystem * fs;

    assert (mpname != NULL && flname != NULL);    
    
    fs = getfs(mpname);

    return (fs != NULL) ? fscreat(fs, flname) : -ENOENT;
}

int delete_file(const char * mpname, const char * flname) {
    struct filesystem * fs;

    assert (mpname != NULL && flname != NULL);
    
    fs = getfs(mpname);

    return (fs != NULL) ? fsdelete(fs, flname) : -ENOENT;
}

int mount_nullfs(const char * name) {
    return attach_filesystem(name, (struct filesystem*)&nullfs);
}

int attach_filesystem(const char * mpname, struct filesystem * fs) {
    struct mountpoint ** mpptr;
    struct mountpoint * mp;
    size_t namelen;

    mpptr = &mplist;
    while ((mp = *mpptr) != NULL) {
        if (strcmp(mp->name, mpname) == 0)
            return -EEXIST;
        mpptr = &mp->next;
    }

    namelen = strlen(mpname);
    mp = kmalloc(sizeof(*mp) + namelen+1);
    memset(mp, 0, sizeof(*mp));

    strncpy(mp->name,mpname, namelen+1);
    mp->fs = fs;
    *mpptr = mp;

    return 0;
}

// INTERNAL FUNCTION DEFINITIONS
//

struct filesystem * getfs(const char * mpname) {
    struct mountpoint * mp;

    for (mp = mplist; mp != NULL; mp = mp->next) {
        if (strcmp(mp->name, mpname) == 0)
            return mp->fs;
    }

    return NULL;
}

int fsopen(struct filesystem * fs, const char * flname, struct uio ** uioptr) {
    if (fs->open != NULL)
        return fs->open(fs, flname, uioptr);
    else
        return -ENOTSUP;
}

int fscreat(struct filesystem * fs, const char * flname) {
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
    if (fs->flush != NULL)
        fs->flush(fs);
}


int nullfs_open (
    struct filesystem * fs __attribute__ ((unused)),
    const char * flname,
    struct uio ** uioptr)
{
    return -ENOENT;
}

void nullfs_flush(struct filesystem * fs __attribute__ ((unused))) {
    // nothing
}


int parse_path(char * path, char ** mpnameptr, char ** flnameptr){
    if (path == NULL || mpnameptr == NULL || flnameptr == NULL) {
        return -EINVAL; // invalid args
    }

    if (*path == '/') path++; // ignore leading slash

    char *slash = strchr(path, '/');
    if (slash == NULL) {
        return -EINVAL;
    }

    *slash = '\0';

    *mpnameptr = path;
    *flnameptr = slash + 1;

    return 0; // success
}



