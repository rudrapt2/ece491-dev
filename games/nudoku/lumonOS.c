#ifdef AEE32

#include "usr/string.h"
#include "usr/heap.h"
#include "usr/syscall.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "stdio.h"

extern int rtc_fd;

static unsigned long long rnd_state = 391;

void srand(unsigned int seed) {
    rnd_state = seed;
}

int rand(void) {
    rnd_state = rnd_state * 25214903917ULL + 11ULL;
    return (int)((rnd_state >> 12) & 0x7fffffffUL);
}

long time(long *t) {
    unsigned long long ts = 0;
    if (rtc_fd >= 0)
        _read(rtc_fd, &ts, sizeof(ts));
    long result = (long)ts;
    if (t) *t = result;
    return result;
}

char *getenv(const char *name) {
    (void)name;
    return (char *)0;
}

int atoi(const char *s) {
    int val = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

int stat(const char *path, void *buf) {
    (void)path; (void)buf;
    return -1;
}

int mkdir(const char *path, int mode) {
    (void)path; (void)mode;
    return -1;
}

int unlink(const char *path) {
    return _delete(path);
}

char *optarg = (char *)0;
int   optind = 1;
int   opterr = 1;
int   optopt = 0;

int getopt(int argc, char *argv[], const char *optstring) {
    static int optpos = 1;

    if (optind >= argc || !argv[optind]) return -1;
    if (argv[optind][0] != '-')          return -1;
    if (argv[optind][1] == '\0')         return -1;
    if (argv[optind][1] == '-' && argv[optind][2] == '\0') {
        optind++;
        return -1;
    }

    int c = (unsigned char)argv[optind][optpos];
    const char *p = strchr(optstring, c);

    if (!p) {
        optopt = c;
        if (++optpos, argv[optind][optpos] == '\0') { optind++; optpos = 1; }
        return '?';
    }

    if (p[1] == ':') {
        if (argv[optind][optpos + 1] != '\0') {
            optarg = argv[optind] + optpos + 1;
        } else {
            optind++;
            if (optind >= argc) { optopt = c; return '?'; }
            optarg = argv[optind];
        }
        optind++;
        optpos = 1;
    } else {
        optarg = (char *)0;
        optpos++;
        if (argv[optind][optpos] == '\0') { optind++; optpos = 1; }
    }
    return c;
}

void sleep(unsigned int s) {
    _usleep((unsigned long)s * 1000000UL);
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++) != '\0');
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++) != '\0');
    return dst;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = (int)vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    return n;
}

int __popcountdi2(unsigned long long x) {
    int n = 0;
    while (x) { x &= x - 1; n++; }
    return n;
}

FILE *fopen(const char *path, const char *mode) {
    if (!path) return (FILE *)0;
    char fspath[64];
    if (path[0] != '/') {
        snprintf(fspath, sizeof(fspath), "/c/%s", path);
        path = fspath;
    }
    if (mode && mode[0] == 'w')
        _create(path);
    int fd = _open(-1, path);
    if (fd < 0) return (FILE *)0;
    FILE *fp = malloc(sizeof(FILE));
    if (!fp) { _close(fd); return (FILE *)0; }
    fp->fd = fd;
    return fp;
}

int fclose(FILE *fp) {
    if (!fp) return EOF;
    _close(fp->fd);
    free(fp);
    return 0;
}

int fgetc(FILE *fp) {
    if (!fp) return EOF;
    unsigned char c;
    if (_read(fp->fd, &c, 1) != 1) return EOF;
    return (int)c;
}

int fprintf(FILE *fp, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = (int)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (fp && n > 0) _write(fp->fd, buf, (size_t)n);
    return n;
}

char *fgets(char *buf, int n, FILE *fp) {
    if (!buf || n <= 0 || !fp) return (char *)0;
    int i = 0;
    while (i < n - 1) {
        unsigned char c;
        if (_read(fp->fd, &c, 1) != 1) break;
        buf[i++] = (char)c;
        if (c == '\n') break;
    }
    if (i == 0) return (char *)0;
    buf[i] = '\0';
    return buf;
}

#endif // AEE32
