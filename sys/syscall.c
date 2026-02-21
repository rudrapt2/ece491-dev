// syscall.c - System call handling
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include <stdint.h>
#ifdef SYSCALL_TRACE
#define TRACE
#endif

#ifdef SYSCALL_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "console.h"
#include "device.h"
#include "error.h"
#include "filesys.h"
#include "heap.h"
#include "intr.h"
#include "memory.h"
#include "misc.h"
#include "process.h"
#include "scnum.h"
#include "string.h"
#include "thread.h"
#include "timer.h"
#include "io.h"

#ifndef PATH_MAX
// Maximum path name length. Must be less than HEAP_ALLOC_MAX.
#define PATH_MAX 255
#endif

// EXPORTED FUNCTION DECLARATIONS
//

extern void handle_syscall(struct trap_frame *tfr);  // called from excp.c


// INTERNAL FUNCTION DECLARATIONS
//

static long syscall(const struct trap_frame *tfr);

static int sysexit(void);
static int sysexec(int fd, int argc, char ** argv);
static int sysfork(const struct trap_frame * tfr);
static int syswait(int tid);
static int sysprint(const char * msg);
static int sysusleep(unsigned long us);

static int sysdelete(const char * path);
static int syscreate(const char * path);

static int sysopen(int fd, const char * path);
static int sysclose(int fd);
static long sysread(int fd, void * buf, size_t bufsz);
static long syswrite(int fd, const void * buf, size_t len);
static int sysioctl(int fd, int cmd, uintptr_t arg_uma);
static int syspipe(int * wfd, int * rfd);
static int sysiodup(int oldfd, int newfd);

static int allocfd(struct process * proc, int reqfd, int notfd);

// EXPORTED FUNCTION DEFINITIONS
//

/**
 * @brief Initiates syscall present in trap frame struct and stores the return address into the sepc
 * @details sepc will be used to return back to program execution after interrupt is handled and
 * sret is called
 * @param tfr pointer to trap frame struct
 * @return void
 */

void handle_syscall(struct trap_frame * tfr) {
    tfr->sepc += 4;
    tfr->a0 = syscall(tfr);
    submit_running_thread();
}

// INTERNAL FUNCTION DEFINITIONS
//

/**
 * @brief Calls specified syscall and passes arguments
 * @details Function uses register a7 to determine syscall number and arguments are passed in from
 * a0-a5 depending on the function
 * @param tfr pointer to trap frame struct
 * @return result of syscall
 */

long syscall(const struct trap_frame * tfr) {
    switch (tfr->a7) {
    case SYSCALL_EXIT:
        return sysexit();
    case SYSCALL_EXEC:
        return sysexec((int)tfr->a0, (int)tfr->a1, (char **)tfr->a2);
    case SYSCALL_FORK:
        return sysfork(tfr);
    case SYSCALL_WAIT:
        return syswait(tfr->a0);
    case SYSCALL_PRINT:
        return sysprint((const char *)tfr->a0);
    case SYSCALL_USLEEP:
        return sysusleep(tfr->a0);
    case SYSCALL_OPEN:
        return sysopen(tfr->a0, (char *)tfr->a1);
    case SYSCALL_CLOSE:
        return sysclose(tfr->a0);
    case SYSCALL_READ:
        return sysread(tfr->a0, (void *)tfr->a1, tfr->a2);
    case SYSCALL_WRITE:
        return syswrite(tfr->a0, (void *)tfr->a1, tfr->a2);
    case SYSCALL_IOCTL:
        return sysioctl(tfr->a0, tfr->a1, tfr->a2);
    case SYSCALL_PIPE:
        return syspipe((int *)tfr->a0, (int *)tfr->a1);
    case SYSCALL_CREATE:
        return syscreate((char *)tfr->a0);
    case SYSCALL_DELETE:
        return sysdelete((char *)tfr->a0);
    case SYSCALL_IODUP:
        return sysiodup(tfr->a0, tfr->a1);
    default:
        return -ENOTSUP;
    }
}

int sysexit(void) {
    trace("%s()", __func__);
    process_exit();
}

int sysexec(int fd, int argc, char ** argv) {
    struct process * self;
    struct io * exeio;

    trace("%s(%d)", __func__, fd);

    if (fd < 0 || PROC_IOMAX <= fd)
        return -EBADF;

    self = current_process();

    if (self->iotab[fd] == NULL)
        return -EBADF;

    // We need to close the file descriptor being exec'd. We'll do that in
    // process_exec, but we need to clear iotab[fd] here.

    exeio = self->iotab[fd];
    self->iotab[fd] = NULL;

    return process_exec(exeio, argc, argv);
}

int sysfork(const struct trap_frame * tfr) {
    trace("%s()", __func__);
    return process_fork(tfr);
}

int syswait(int tid) {
    trace("%s(%d)", __func__, tid);

    if (0 <= tid)
        return join_thread(tid);
    else
        return -EINVAL;
}

int sysprint(const char *msg) {
    int result;

    trace("%s(%p)", __func__, msg);

    result = validate_vstr(msg, PTE_U);

    if (result != 0)
        return result;

    trace("%s(\"%s\")", __func__, msg);

    kprintf("<%s:%d> USER: %s\n",
            thread_name(running_thread()), running_thread(), msg);

    return 0;
}

int sysusleep(unsigned long us) {
    sleep_us(us);
    return 0;
}

int syscreate(const char *path) {
    size_t pathlen;
    char * pathbuf;
    char * mpname;
    char * flname;
    int result;

   if (path == NULL)
        return -EINVAL;
    
    trace("%s(%p)", __func__, path);

    result = validate_vstr(path, PTE_U);

    if (result != 0)
        return result;

    // Print trace statement again with path string

    trace("%s(\"%s\")", __func__, path);

    pathlen = strlen(path);

    if (pathlen == 0 || PATH_MAX < pathlen)
        return -EINVAL;
    
    pathbuf = kmalloc(pathlen+1);
    memcpy(pathbuf, path, pathlen+1);
    parse_path(pathbuf, &mpname, &flname);

    if (*mpname == '\0')
        result = -EINVAL;

    if (result == 0)
        result = create_file(mpname, flname);

    kfree(pathbuf);
    return result;
}

int sysdelete(const char * path) {
    size_t pathlen;
    char * pathbuf;
    char * mpname;
    char * flname;
    int result;

    if (path == NULL)
        return -EINVAL;
    
    trace("%s(%p)", __func__, path);

    result = validate_vstr(path, PTE_U);

    if (result != 0)
        return result;

    // Print trace statement again with path string

    trace("%s(\"%s\")", __func__, path);

    pathlen = strlen(path);

    if (pathlen == 0 || PATH_MAX < pathlen)
        return -EINVAL;
    
    pathbuf = kmalloc(pathlen+1);
    memcpy(pathbuf, path, pathlen+1);
    parse_path(pathbuf, &mpname, &flname);

    if (*mpname == '\0')
        result = -EINVAL;

    if (result == 0)
        result = delete_file(mpname, flname);

    kfree(pathbuf);
    return result;
}

int sysopen(int fd, const char * path) {
    struct process * self;
    size_t pathlen;
    char * pathbuf;
    char * mpname;
    char * flname;
    struct io * io;
    int result;

    if (path == NULL)
        return -EINVAL;
    
    trace("%s(%d,%p)", __func__, fd, path);

    result = validate_vstr(path, PTE_U);

    if (result != 0)
        return result;

    // Print trace statement again with path string

    trace("%s(%d,\"%s\")", __func__, fd, path);

    self = current_process();

    fd = allocfd(self, fd, -1);

    if (fd < 0)
        return fd;

    pathlen = strlen(path);

    if (PATH_MAX < pathlen)
        return -EINVAL;
    
    pathbuf = kcalloc(pathlen+1, 1);
    memcpy(pathbuf, path, pathlen+1);
    parse_path(pathbuf, &mpname, &flname);

    result = open_file(mpname, flname, &io);

    if (result == 0)
        self->iotab[fd] = io;

    kfree(pathbuf);
    return (result != 0) ? result : fd;
}

int sysclose(int fd) {
    struct process * self;

    trace("%s(%d)", __func__, fd);

    if (fd < 0 || PROC_IOMAX <= fd)
        return -EBADF;

    self = current_process();

    if (self->iotab[fd] == NULL)
        return -EBADF;

    iodropref(self->iotab[fd]);
    self->iotab[fd] = NULL;
    return 0;
}

long sysread(int fd, void * buf, size_t bufsz) {
    struct process * self;
    int result;

    trace("%s(%d,%p,%zu)", __func__, fd, buf, bufsz);

    if (fd < 0 || PROC_IOMAX <= fd)
        return -EBADF;

    self = current_process();

    if (self->iotab[fd] == NULL)
        return -EBADF;

    // Ensure memory region is user-writable

    result = validate_vptr(buf, bufsz, PTE_W | PTE_U);

    if (result != 0)
        return result;

    return ioread(self->iotab[fd], buf, bufsz);
}

long syswrite(int fd, const void *buf, size_t len) {
    struct process * self;
    int result;

    trace("%s(%d,%p,%zu)", __func__, fd, buf, len);

    if (fd < 0 || PROC_IOMAX <= fd)
        return -EBADF;

    self = current_process();

    if (self->iotab[fd] == NULL)
        return -EBADF;

    // Ensure memory region is user-readable

    result = validate_vptr(buf, len, PTE_R | PTE_U);

    if (result != 0)
        return result;

    return iowrite(self->iotab[fd], buf, len);
}

int sysioctl(int fd, int op, uintptr_t arg_uma) {
    struct process * self;
    uint8_t flags = PTE_U;
    int result;

    trace("%s(%d,%d,%p)", __func__, fd, op, arg_uma);

    if (fd < 0 || PROC_IOMAX <= fd)
        return -EBADF;

    self = current_process();

    if (self->iotab[fd] == NULL)
        return -EBADF;

    switch (op) {
    case IOC_SETPOS:
    case IOC_SETEND:
        flags |= PTE_W;

    // fall through
    case IOC_GETPOS:
    case IOC_GETEND:
        flags |= PTE_R;

        result = validate_vptr(
            (void *)arg_uma, sizeof(unsigned long long), flags);
        if (result != 0)
            return result;
        
    // fall through
    case IOC_GETBLKSZ:
        return ioctl(self->iotab[fd], op, (void *)arg_uma);

    default: // unknown arg, let device handle
        return ioctl_u(self->iotab[fd], op, arg_uma);
    }
}

int syspipe(int * wfdptr, int * rfdptr) {
    struct process * self;
    int wfd, rfd;
    int result;

    trace("%s(wfd=%d,rfd=%d)", __func__, wfd, rfd);

    // Ensure memory region is user-writable

    result = validate_vptr(wfdptr, sizeof(int), PTE_W | PTE_U);

    if (result != 0)
        return result;

    result = validate_vptr(rfdptr, sizeof(int), PTE_W | PTE_U);

    if (result != 0)
        return result;
    
    self = current_process();

    // Find two free file descriptor slots

    wfd = allocfd(self, /* reqfd */ *wfdptr, /* notfd */ -1);

    if (wfd < 0)
        return wfd;
    
    rfd = allocfd(self, /* reqfd */ *rfdptr, /* notfd */ wfd);

    if (rfd < 0)
        return rfd;

    *wfdptr = wfd;
    *rfdptr = rfd;

    create_iopipe(&self->iotab[wfd], &self->iotab[rfd]);

    return 0;
}

int sysiodup(int oldfd, int newfd) {
    struct process *self;

    trace("%s(oldfd=%d,newfd=%d)", __func__, oldfd, newfd);

    if (oldfd < 0 || PROC_IOMAX <= oldfd)
        return -EBADF;

    if (PROC_IOMAX <= newfd)
        return -EBADF;

    self = current_process();

    if (self->iotab[oldfd] == NULL)
        return -EBADF;

    newfd = allocfd(self, newfd, oldfd);

    self->iotab[newfd] = ioaddref(self->iotab[oldfd]);
    return newfd;
}

int allocfd(struct process * proc, int reqfd, int notfd) {
    int fd;

    assert (proc != NULL);

    if (0 <= reqfd) {
        if (reqfd != notfd && proc->iotab[reqfd] == NULL)
            return reqfd;
        else
            return -EBADF;
    }

    for (fd = 0; fd < PROC_IOMAX; fd++) {
        if (fd != notfd && proc->iotab[fd] == NULL)
            return fd;
    }

    return -EMFILE;
}