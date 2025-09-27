/*! @file syscall.c
    @brief system call handlers
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA
*/

#ifdef SYSCALL_TRACE
#define TRACE
#endif

#ifdef SYSCALL_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "scnum.h"
#include "process.h"
#include "memory.h"
#include "uio.h"
#include "device.h"
#include "filesys.h"
#include "intr.h"
#include "timer.h"
#include "error.h"
#include "thread.h"
#include "process.h"
#include "misc.h"
#include "console.h"

// EXPORTED FUNCTION DECLARATIONS
//

extern void handle_syscall(struct trap_frame *tfr); // called from excp.c

// INTERNAL FUNCTION DECLARATIONS
//

static int64_t syscall(const struct trap_frame *tfr);

static int sysexit(void);
static int sysexec(int fd, int argc, char **argv);
static int sysfork(const struct trap_frame *tfr);
static int syswait(int tid);
static int sysprint(const char *msg);
static int sysusleep(unsigned long us);

static int sysfsdelete(const char *name);
static int sysfscreate(const char *name);

static int sysopen(int fd, const char *name);
static int sysclose(int fd);
static long sysread(int fd, void *buf, size_t bufsz);
static long syswrite(int fd, const void *buf, size_t len);
static int sysioctl(int fd, int cmd, void *arg);
static int syspipe(int *wfdptr, int *rfdptr);
static int sysiodup(int oldfd, int newfd);

// EXPORTED FUNCTION DEFINITIONS
//

/**
 * @brief Initiates syscall present in trap frame struct and stores the return address into the sepc
 * @details sepc will be used to return back to program execution after interrupt is handled and sret is called
 * @param tfr pointer to trap frame struct
 * @return void
 */

void handle_syscall(struct trap_frame *tfr)
{
    tfr->sepc += 4;
    tfr->a0 = syscall(tfr);
}

// INTERNAL FUNCTION DEFINITIONS
//

/**
 * @brief Calls specified syscall and passes arguments
 * @details Function uses register a7 to determine syscall number and arguments are passed in from a0-a5 depending on the function
 * @param tfr pointer to trap frame struct
 * @return result of syscall
 */

int64_t syscall(const struct trap_frame *tfr)
{
    switch (tfr->a7)
    {
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
        return sysioctl(tfr->a0, tfr->a1, (void *)tfr->a2);
    case SYSCALL_PIPE:
        return syspipe((int *)tfr->a0, (int *)tfr->a1);
    case SYSCALL_FSCREATE:
        return sysfscreate((char *)tfr->a0);
    case SYSCALL_FSDELETE:
        return sysfsdelete((char *)tfr->a0);
    case SYSCALL_IODUP:
        return sysiodup(tfr->a0, tfr->a1);
    default:
        return -ENOTSUP;
    }
}

/**
 * @brief Calls process exit
 * @return void
 */

int sysexit(void)
{
    process_exit();
}

/**
 * @brief Executes new process given a executable and arguments
 * @details Valid fd checks, get current process struct, close fd being executed, finally calls process_exec with arguments and executable io "file"
 * @param fd file descripter idx
 * @param argc number of arguments in argv
 * @param argv array of arguments for multiple args
 * @return result of process_exec, else -EBADFD on invalid file descriptors
 */

int sysexec(int fd, int argc, char **argv)
{
    struct process *self;
    struct uio *exeio;

    trace("%s(%d)", __func__, fd);

    if (fd < 0 || PROCESS_IOMAX <= fd)
        return -EBADFD;

    self = current_process();

    if (self->uiotab[fd] == NULL)
        return -EBADFD;

    // We need to close the file descriptor being exec'd. We'll do that in
    // process_exec, but we need to clear iotab[fd] here.

    exeio = self->uiotab[fd];
    self->uiotab[fd] = 0;

    return process_exec(exeio, argc, argv);
}

/**
 * @brief Forks a new child process using process_fork
 * @param tfr pointer to the trap frame
 * @return result of process_fork
 */

int sysfork(const struct trap_frame *tfr)
{
    trace("%s()", __func__);
    return process_fork(tfr);
}

/**
 * @brief Sleeps till a specified child process completes
 * @details Calls thread_join with the thread id the process wishes to wait for
 * @param tid thread_id
 * @return result of thread_join else invalid on invalid thread id
 */

int syswait(int tid)
{
    trace("%s(%d)", __func__, tid);

    if (0 <= tid)
        return thread_join(tid);
    else
        return -EINVAL;
}

/**
 * @brief Prints to console via kprintf
 * @details Validates that msg string is valid via validate_vstr and pages are mapped, calls kprintf on current running process
 * @param msg string msg in userspace
 * @return 0 on sucess else error from validate_vstr
 */

int sysprint(const char *msg)
{
    int result;

    trace("%s(msg=%p)", __func__, msg);

    result = validate_vstr(msg, PTE_U);
    if (result != 0)
        return result;

    kprintf("<%s:%d> %s\n",
            thread_name(running_thread()), running_thread(), msg);

    return 0;
}

/**
 * @brief Sleeps process till specificed amount of time has passed
 * @details Creates alarm struct, inits struct with name usleep, which sets the current time via the rd_time() function, taking values from the csr, makes frequency calcuation to determine us has passed before waking process
 * @param us time in us for process to sleep
 * @return 0
 */

int sysusleep(unsigned long us)
{
    struct alarm alarm;

    alarm_init(&alarm, "usleep");
    alarm_sleep_us(&alarm, us);
    return 0;
}

int sysfscreate(const char *name)
{
    int result = validate_vstr(name, PTE_U);
    char *mpname;
    char *flname;

    if (result != 0)
        return result;

    result = parse_path(name, &mpname, &flname);

    if (result != 0)
        return result;

    return create_file(mpname, flname);
}

int sysfsdelete(const char *name)
{
    int result = validate_vstr(name, PTE_U);
    char *mpname;
    char *flname;

    if (result != 0)
        return result;

    result = parse_path(name, &mpname, &flname);

    if (result != 0)
        return result;

    return delete_file(mpname, flname);
}

/**
 * @brief Opens a file or device of specified fd for given process
 * @details gets current process, allocates file descriptor (if fd = -1) or uses valid file descriptor given, validates name of device/file, calls open_file
 * @param fd file descriptor number
 * @param name string name of device or file
 * @return fd number if sucessful else return error that occured -EMFILE or -EBADFD
 */

int sysopen(int fd, const char *name)
{
    struct process *proc;
    struct uio *uio;
    int result;
    char *mpname;
    char *flname;

    trace("%s(fd=%d,name=%p)", __func__, fd, name);

    if (PROCESS_IOMAX <= fd)
        return -EBADFD;

    proc = current_process();

    if (fd < 0)
    {
        fd = 0;
        while (fd < PROCESS_IOMAX)
            if (proc->uiotab[fd] == NULL)
                goto sysopen_fd_ok;
            else
                fd += 1;
        return -EMFILE;
    }
    else if (proc->uiotab[fd] != NULL)
        return -EBADFD;

sysopen_fd_ok:

    result = validate_vstr(name, PTE_U);

    if (result != 0)
        return result;

    result = parse_path(name, &mpname, &flname);

    if (result != 0)
        return result;

    result = open_file(mpname, flname, &uio);

    if (result < 0)
        return result;

    proc->uiotab[fd] = uio;
    return fd;
}

/**
 * @brief Closes file or device of specified fd for given process
 * @details gets current process, calls close function of the io, deallocates the file descriptor
 * @param fd file descriptor
 * @return 0 on success, error on invalid file descriptor or empty file descriptor
 */

int sysclose(int fd)
{
    struct process *self;

    trace("%s(%d)", __func__, fd);

    if (fd < 0 || PROCESS_IOMAX <= fd)
        return -EBADFD;

    self = current_process();

    if (self->uiotab[fd] == NULL)
        return -EBADFD;

    uio_close(self->uiotab[fd]);
    self->uiotab[fd] = NULL;
    return 0;
}

/**
 * @brief Calls read function of file io on given buffer
 * @details get current process, valid file descriptor checks, find io struct via file descriptor, validate buffer, call ioread with given buffer
 * @param fd file descriptor number
 * @param buf pointer to buffer
 * @param bufsz number of bytes to be read
 * @return number of bytes read
 */

long sysread(int fd, void *buf, size_t bufsz)
{
    struct process *self;
    int validate_result;

    trace("%s(%d,%p,%zu)", __func__, fd, buf, bufsz);

    if (fd < 0 || PROCESS_IOMAX <= fd)
        return -EBADFD;

    self = current_process();

    if (self->uiotab[fd] == NULL)
        return -EBADFD;

    // Ensure memory region is user-writable

    validate_result = validate_vptr(buf, bufsz, PTE_W | PTE_U);

    if (validate_result != 0)
        return validate_result;

    return uio_read(self->uiotab[fd], buf, bufsz);
}

/**
 * @brief Calls write function of file io on given buffer
 * @details get current process, valid file descriptor checks, find io struct via file descriptor, validate buffer, call iowrite with given buffer
 * @param fd file descriptor number
 * @param buf pointer to buffer
 * @param len number of bytes to be written
 * @return number of bytes written
 */

long syswrite(int fd, const void *buf, size_t len)
{
    struct process *self;
    int result;

    trace("%s(%d,%p,%zu)", __func__, fd, buf, len);

    if (fd < 0 || PROCESS_IOMAX <= fd)
        return -EBADFD;

    self = current_process();

    if (self->uiotab[fd] == NULL)
        return -EBADFD;

    // Ensure memory region is user-writable

    result = validate_vptr(buf, len, PTE_R | PTE_U);

    if (result != 0)
        return result;

    return uio_write(self->uiotab[fd], buf, len);
}

/**
 * @brief Calls device input output commands for a given device instance
 * @details get current process, valid file descriptor checks, find io struct via file descriptor, ensure that ioctl type exists, validate argument pointer, issue ioctl
 * @param fd file descriptor number
 * @param cmd selection of ioctl
 * @param arg pointer to arguments
 * @return number of bytes written
 */

int sysioctl(int fd, int cmd, void *arg)
{
    static const struct
    {
        uint8_t size;  ///< size of argument
        uint8_t flags; ///< permissions
    } argdef[] = {     ///< definition of arguments for each ioctal
                  // [IOCTL_GETBLKSZ] = {0, 0},
                  [FCNTL_GETPOS] = {sizeof(unsigned long long), PTE_W},
                  [FCNTL_SETPOS] = {sizeof(unsigned long long), PTE_R},
                  [FCNTL_GETEND] = {sizeof(unsigned long long), PTE_W},
                  [FCNTL_SETEND] = {sizeof(unsigned long long), PTE_R},
                  [FCNTL_MMAP] = {sizeof(unsigned long long), PTE_W}};

    struct process *self;
    int result;

    trace("%s(%d,%d,%p)", __func__, fd, cmd, arg);

    if (fd < 0 || PROCESS_IOMAX <= fd)
        return -EBADFD;

    self = current_process();

    if (self->uiotab[fd] == NULL)
        return -EBADFD;

    // Negative command values means the driver validates the argument. Positive
    // command values are checked here so make drivers simpler.

    if (cmd < 0)
        return uio_cntl(self->uiotab[fd], cmd, arg);

    // Check if we know the IOCTL type
    if (sizeof(argdef) / sizeof(argdef[0]) <= cmd)
        return -ENOTSUP;

    // Check if the argument pointer has the right access

    result = validate_vptr(
        arg, argdef[cmd].size, argdef[cmd].flags | PTE_U);

    if (result != 0)
        return result;

    // Issue the ioctl

    return uio_cntl(self->uiotab[fd], cmd, arg);
}

/**
 * @brief Creates a pipe for the current process
 * @details The function retrieves the current process. If either the write or read descriptor pointer stores a negative value, an unused descriptor is assigned. If both file descriptors are unused and valid, the function connects them via create_pipe function.
 * @param wfdptr pointer to write file descriptor
 * @param rfdptr pointer to read file descriptor
 * @return 0 on success. Else, negative error code on invalid file descriptor, or if a file descriptor is already in use, or if no descriptors are found available.
 */
int syspipe(int *wfdptr, int *rfdptr)
{
    struct process *self;

    trace("%s(wfd=%d,rfd=%d)", __func__, *wfdptr, *rfdptr);

    if (PROCESS_IOMAX <= *wfdptr)
        return -EBADFD;

    if (PROCESS_IOMAX <= *rfdptr)
        return -EBADFD;

    self = current_process();

    if (*wfdptr < 0)
    {
        *wfdptr = 0;
        while (*wfdptr < PROCESS_IOMAX)
            if (self->uiotab[*wfdptr] == NULL)
                goto syspipeopen_wfd_ok;
            else
                *wfdptr += 1;
        return -EMFILE;
    }
    else if (self->uiotab[*wfdptr] != NULL)
        return -EBADFD;

syspipeopen_wfd_ok:

    if (*rfdptr < 0)
    {
        *rfdptr = 0;
        while (*rfdptr < PROCESS_IOMAX)
            if (self->uiotab[*rfdptr] == NULL && *rfdptr != *wfdptr)
                goto syspipeopen_rfd_ok;
            else
                *rfdptr += 1;
        return -EMFILE;
    }
    else if (self->uiotab[*rfdptr] != NULL && *rfdptr != *wfdptr)
        return -EBADFD;

syspipeopen_rfd_ok:

    create_pipe(&self->uiotab[*wfdptr], &self->uiotab[*rfdptr]);
    return 0;
}

/**
 * @brief Duplicates a file description
 * @details Allocates a new file descriptor that refers to the same open file description as the descriptor _oldfd_.
 * @param oldfd old file descriptor number
 * @param newfd new file descriptor number
 * @return fd number if sucessful else return error on invalid file descriptor or empty file descriptor
 */

int sysiodup(int oldfd, int newfd)
{
    struct process *self;
    trace("%s(oldfd=%d,newfd=%d)", __func__, oldfd, newfd);

    if (oldfd < 0 || PROCESS_IOMAX <= oldfd)
        return -EBADFD;

    if (PROCESS_IOMAX <= newfd)
        return -EBADFD;

    self = current_process();

    if (self->uiotab[oldfd] == NULL)
        return -EBADFD;

    if (newfd < 0)
    {
        newfd = 0;
        while (newfd < PROCESS_IOMAX)
            if (self->uiotab[newfd] == NULL)
                goto sysdup_fd_ok;
            else
                newfd += 1;
        return -EMFILE;
    }
    else if (self->uiotab[newfd] != NULL)
        return -EBADFD;

sysdup_fd_ok:

    uio_addref(self->uiotab[oldfd]);
    self->uiotab[newfd] = self->uiotab[oldfd];
    return newfd;
}