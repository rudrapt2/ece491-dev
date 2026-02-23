// glue.c - Runtime/syscall wrapper for nudoku (AEE32 / MP3 CP2)
//
// Provides POSIX-like runtime functions not available in the bare-metal
// environment. Keep this file focused on process/runtime adaptation,
// syscalls, and compatibility shims.
// ncurses implementation details live in curses.c.
//

#ifdef AEE32

#include "usr/string.h"     // strlen, snprintf, vsnprintf, printf, memcpy, ...
#include "usr/heap.h"       // malloc, free
#include "usr/syscall.h"    // _exit, _open, _close, _read, _write, _delete, _usleep
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// ── Internal RTC fd ───────────────────────────────────────────────────────────

static int rtc_fd = -1;

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    extern int nudoku_main(int argc, char *argv[]);

    // Open RTC for time()/srand seeding
    rtc_fd = _open(-1, "/dev/rtc");

    // Seed RNG from hardware entropy; fall back to RTC or constant
    {
        unsigned long long seed = 0;
        int rng = _open(-1, "/dev/viorng0");
        if (rng >= 0) {
            _read(rng, &seed, sizeof(seed));
            _close(rng);
        } else if (rtc_fd >= 0) {
            _read(rtc_fd, &seed, sizeof(seed));
        } else {
            seed = 391;
        }
        // Store in rand state (srand will be called by nudoku, so just prime it)
        extern void srand(unsigned int);
        srand((unsigned int)(seed ^ (seed >> 32)));
    }

    return nudoku_main(argc, argv);
}

// ── exit / abort ──────────────────────────────────────────────────────────────

// atexit handler table
#define ATEXIT_MAX 8
static void (*atexit_fns[ATEXIT_MAX])(void);
static int   atexit_cnt = 0;

int atexit(void (*fn)(void)) {
    if (atexit_cnt >= ATEXIT_MAX) return -1;
    atexit_fns[atexit_cnt++] = fn;
    return 0;
}

void exit(int status) {
    (void)status;
    // Run registered atexit handlers in LIFO order
    for (int i = atexit_cnt - 1; i >= 0; i--)
        atexit_fns[i]();
    _exit();
}

void abort(void) { _exit(); }

// ── Random number generator ───────────────────────────────────────────────────

static unsigned long long rnd_state = 391;

void srand(unsigned int seed) {
    rnd_state = seed;
}

int rand(void) {
    // LCG parameters from Knuth
    rnd_state = rnd_state * 25214903917ULL + 11ULL;
    return (int)((rnd_state >> 12) & 0x7fffffffUL);
}

// ── time() ────────────────────────────────────────────────────────────────────
// Returns seconds-since-epoch from the RTC (or 0 on failure).
// Signature matches newlib time_t = long int on riscv64.

long time(long *t) {
    unsigned long long ts = 0;
    if (rtc_fd >= 0)
        _read(rtc_fd, &ts, sizeof(ts));
    long result = (long)ts;
    if (t) *t = result;
    return result;
}

// ── getenv ────────────────────────────────────────────────────────────────────
// No environment in bare-metal; save-game path logic in nudoku falls back
// to $HOME which will be NULL.

char *getenv(const char *name) {
    (void)name;
    return (char *)0;
}

// ── atoi ─────────────────────────────────────────────────────────────────────

int atoi(const char *s) {
    int val = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

// ── Filesystem stubs ─────────────────────────────────────────────────────────
// save/restore of games requires a writable filesystem; stub these out so
// that the code compiles and fails gracefully at runtime.

// stat(path, struct stat *buf) → -1 always
int stat(const char *path, void *buf) {
    (void)path; (void)buf;
    return -1;
}

// mkdir(path, mode) → -1 always
int mkdir(const char *path, int mode) {
    (void)path; (void)mode;
    return -1;
}

// unlink → _delete syscall
int unlink(const char *path) {
    return _delete(path);
}

// ── getopt ────────────────────────────────────────────────────────────────────
// Full POSIX getopt for completeness (nudoku called with argc=1 so this
// returns -1 on the very first call in practice).

char *optarg = (char *)0;
int   optind = 1;
int   opterr = 1;
int   optopt = 0;

int getopt(int argc, char *argv[], const char *optstring) {
    static int optpos = 1;

    // No more arguments or not an option
    if (optind >= argc || !argv[optind]) return -1;
    if (argv[optind][0] != '-')          return -1;
    if (argv[optind][1] == '\0')         return -1;
    // "--" terminates option parsing
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
        // option requires an argument
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

// ── sleep ─────────────────────────────────────────────────────────────────────

void sleep(unsigned int s) {
    _usleep((unsigned long)s * 1000000UL);
}

// ── String helpers not in usr/string.c ───────────────────────────────────────

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
    int n = (int)vsnprintf(buf, 1024, fmt, ap); // nudoku's buffers are ≤ 512 B
    va_end(ap);
    return n;
}

// ── GCC runtime stub ──────────────────────────────────────────────────────────
// sudoku.c uses bit operations that GCC lowers to __popcountdi2 on rv64g;
// provide a software fallback since we don't link libgcc.

int __popcountdi2(unsigned long long x) {
    int n = 0;
    while (x) { x &= x - 1; n++; }
    return n;
}

// ── FILE-based I/O wrappers (for nudoku save/restore) ────────────────────────
// Save-game paths come from getenv() which returns NULL → fopen gets NULL and
// returns NULL cleanly.

#include "stdio.h"   // our local stub for FILE typedef

FILE *fopen(const char *path, const char *mode) {
    if (!path) return (FILE *)0;
    // Prepend /c/ if relative path
    char fspath[64];
    if (path[0] != '/') {
        snprintf(fspath, sizeof(fspath), "/c/%s", path);
        path = fspath;
    }
    if (mode && mode[0] == 'w')
        _create(path);  // create (or truncate) for writing
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
