// glue.c - Stuff to make doom work with our OS
// 

#include "glue.h"
#include "usr/shell.h"
#include "usr/error.h"
#include "usr/string.h"
#include "usr/syscall.h"
#include <string.h>
#include <sys/types.h>

int errno;
static FILE _stdin = {.fd = STDIN};
static FILE _stdout = {.fd = STDOUT};
static FILE _stderr = {.fd = CONSOLEOUT};
FILE* stdin = &_stdin;
FILE* stdout = &_stdout;
FILE* stderr = &_stderr;

int debug(const char * fmt, ...) {
    // char buf[256];
    // va_list ap;
    // size_t n;

    // va_start(ap, fmt);
    // n = vsnprintf(buf, 256, fmt, ap);
    // va_end(ap);

    // _print(buf);
    // return n;
    return 0;
}

extern FILE* fopen(const char *filename, const char *mode) {
    debug("%s(filename=%s)", __func__, filename);
    if (mode == NULL) return NULL;

    if (mode[0] == 'w') 
        _create(filename);

    int fd = _open(-1, filename);
    if (mode[0] == 'w') {
        unsigned long long end = 0;
        _ioctl(fd, IOC_SETEND, &end);
    }

    if (fd < 0) {
        errno = fd;
        return NULL;
    }
    FILE * f = malloc(sizeof(FILE));
    f->fd = fd;
    return f;
}

extern int fseek(FILE *stream, long offset, int whence) {
    unsigned long long pos = 0;
    if (whence == SEEK_END)
        _ioctl(stream->fd, IOC_GETEND, &pos);
    pos += offset;
    debug("%s(fd=%d, pos=%llu)", __func__, stream->fd, pos);
    return _ioctl(stream->fd, IOC_SETPOS, &pos);
}

extern int fclose(FILE *stream) {
    int res = _close(stream->fd);
    debug("%s(fd=%d)", __func__, stream->fd);
    free(stream);
    return res;
}

extern long ftell(FILE *stream) {
    unsigned long long pos;
    int res = _ioctl(stream->fd, IOC_GETPOS, &pos);
    debug("%s(fd=%d): res=%d", __func__, stream->fd, (res < 0) ? res : pos);
    return (res < 0) ? res : pos;
}

void dvprintf_putc(char c, void * aux) {
    dputc(*((int*)aux), c);
}

extern int fprintf(FILE *stream, const char * fmt, ...) {
    debug("%s(fmt=%s)", __func__, fmt);
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vgprintf(dvprintf_putc, &stream->fd, fmt, ap);
    va_end(ap);
    return n;
}

extern int vfprintf(FILE *stream, const char *format, va_list arg) {
    debug("%s(fd=%d, fmt=%s)", __func__, stream->fd, format);
    return vgprintf(dvprintf_putc, &stream->fd, format, arg);
}

extern int remove(const char *filename) {
    debug("%s(filename=%s)", __func__, filename);
    return _delete(filename);
}

extern int rename(const char *old_filename, const char *new_filename) {
    int n, ofd, nfd;
    char buf[256];
    debug("%s(old_filename=%s, new_filename=%s)", __func__, old_filename, new_filename);

    ofd = _open(-1, old_filename);
    if (ofd < 0) {
        debug("failed to open %s (%s)", old_filename, error_desc(ofd));
        return ofd;
    }

    _create(new_filename);
    nfd = _open(-1, new_filename);
    if (nfd < 0) {
        debug("failed to open %s (%s)", new_filename, error_desc(nfd));
        return nfd;
    }

    do {
        n = _read(ofd, buf, 256);
        if (n < 0) return n;
        n = _write(nfd, buf, n);
        if (n < 0) return n;
    } while (n > 0);

    return n;
}

extern size_t fread(void *buffer, size_t size, size_t count, FILE *stream) {
    debug("%s(fd=%d, n=%ld)", __func__, stream->fd, size*count);
    return _read(stream->fd, buffer, size*count);
}

extern size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    debug("%s(fd=%d, n=%ld)", __func__, stream->fd, size*count);
    return _write(stream->fd, ptr, size*count);
}

extern int fflush(FILE *stream) {
    (void)stream;
    debug("%s()", __func__);
    return 0; // nothing to do
}

extern int putchar(int character) {
    dputc(STDOUT, character);
    return character;
}

extern int sscanf(const char *str, const char *format, int * result) {
    char * end;
    *result = strtoul(str, &end, 10);
    debug("%s(str=%s, fmt=%s): res=%d", __func__, str, format, *result);
    if (end == str) return -1;
    return 0;
}

extern void *realloc(void *ptr, size_t size) {
    debug("%s(size=%ld)", __func__, size);
    // if we had a smarter heap we could do this better
    void * nptr = malloc(size);
    if (ptr) {
        memcpy(nptr, ptr, size);
        free(ptr);
    }
    return nptr;
}

extern int atoi(const char * str) {
    debug("%s(str=%ld)", __func__, str);
    return strtoul(str, NULL, 10);
}

extern void exit(int code) {
    debug("%s(code=%ld)", __func__, code);
    _exit();
}

extern inline int abs(int x) {
    debug("%s(int=%d)", __func__, x);
    return (x<0) ? -x : x;
}

// UNSUPPORTED FUNCTIONS

extern int system(const char* command) {
    debug("%s(cmd=%s)", __func__, command);
    return -ENOTSUP;
}

extern int mkdir(const char *path, mode_t mode) {
    debug("%s(cmd=%s)", __func__, path);
    return -ENOTSUP;
}

extern inline double fabs(double x) {
    debug("%s(x=%f)", __func__, x);
    return (x<0) ? -x : x;
}

extern double atof(const char * str) {
    debug("%s(str=%ld)", __func__, str);
    return -ENOTSUP;
}
