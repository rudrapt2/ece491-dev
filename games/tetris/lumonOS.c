#ifdef AEE32

#include "usr/syscall.h"
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "source/compat.h"
#include "source/stdio.h"

extern size_t vsnprintf(char * buf, size_t bufsz, const char * fmt, va_list ap);
extern size_t snprintf(char * buf, size_t bufsz, const char * fmt, ...);

static unsigned long long rnd_state = 391ULL;

void srandom(unsigned int seed) { rnd_state = seed ? seed : 391ULL; }
long random(void) {
    rnd_state = rnd_state * 25214903917ULL + 11ULL;
    return (long)((rnd_state >> 12) & 0x7fffffffUL);
}

int setegid(int gid) { (void)gid; return 0; }
int getegid(void) { return 0; }
int getgid(void) { return 0; }
int getuid(void) { return 0; }
int getpid(void) { return 1; }
char *getlogin(void) { return "player"; }
char *getenv(const char *name) {
    if (name && !strcmp(name, "TERM")) return "ansi";
    return (char *)0;
}

int atoi(const char *s) {
    int val = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++) != '\0');
    return dst;
}

int *__errno(void) {
    static int e;
    return &e;
}

int kill(int pid, int sig) { (void)pid; (void)sig; return 0; }

typedef void (*sighandler_t)(int);
sighandler_t signal(int sig, sighandler_t handler) {
    (void)sig;
    return handler;
}

typedef unsigned int sigset_t;
int sigemptyset(sigset_t *set) { if (set) *set = 0; return 0; }
int sigaddset(sigset_t *set, int signo) { if (set) *set |= (1u << (signo & 31)); return 0; }
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    (void)how; (void)set; if (oldset) *oldset = 0; return 0;
}

// getopt compatibility
char *optarg = (char *)0;
int   optind = 1;
int   opterr = 1;
int   optopt = 0;

int getopt(int argc, char * const argv[], const char *optstring) {
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

// termios/ioctl stubs
speed_t cfgetospeed(const struct termios *t) { (void)t; return 0; }
int tcgetattr(int fd, struct termios *t) {
    (void)fd;
    if (!t) return -1;
    t->c_lflag = 0;
    t->c_oflag = 0;
    return 0;
}
int tcsetattr(int fd, int opt, const struct termios *t) {
    (void)fd; (void)opt; (void)t; return 0;
}

int ioctl(int fd, unsigned long req, void *arg) {
    (void)fd;
    if (req == TIOCGWINSZ && arg) {
        struct winsize *ws = (struct winsize *)arg;
        ws->ws_row = 30;
        ws->ws_col = 90;
        ws->ws_xpixel = ws->ws_ypixel = 0;
        return 0;
    }
    return -1;
}

// err.h compatibility
static void vmsg(const char *pfx, const char *fmt, va_list ap) {
    if (pfx) {
        _write(2, pfx, strlen(pfx));
    }
    if (fmt) {
        char buf[512];
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        if (n > 0) _write(2, buf, (size_t)n);
    }
    _write(2, "\n", 1);
}

void warn(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vmsg("warning: ", fmt, ap); va_end(ap);
}
void warnx(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vmsg("warning: ", fmt, ap); va_end(ap);
}
void err(int eval, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vmsg("error: ", fmt, ap); va_end(ap); (void)eval; _exit();
}
void errx(int eval, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vmsg("error: ", fmt, ap); va_end(ap); (void)eval; _exit();
}

// termcap compatibility
char PC = 0;
char *BC = "\b";
char *UP = "\033[A";

int tgetent(char *bp, const char *name) {
    (void)bp; (void)name;
    return 1;
}

int tgetflag(const char *id) {
    if (!id) return 0;
    if (id[0] == 'b' && id[1] == 's') return 1;
    if (id[0] == 'm' && id[1] == 's') return 1;
    return 0;
}

int tgetnum(const char *id) {
    if (!id) return -1;
    if (id[0] == 'c' && id[1] == 'o') return 90;
    if (id[0] == 'l' && id[1] == 'i') return 30;
    if (id[0] == 's' && id[1] == 'g') return -1;
    return -1;
}

char *tgetstr(const char *id, char **area) {
    (void)area;
    if (!id) return (char *)0;
    if (id[0]=='c' && id[1]=='l') return "\033[H\033[J";
    if (id[0]=='c' && id[1]=='e') return "\033[K";
    if (id[0]=='c' && id[1]=='m') return "cm";
    if (id[0]=='l' && id[1]=='e') return "\b";
    if (id[0]=='u' && id[1]=='p') return "\033[A";
    if (id[0]=='s' && id[1]=='o') return "\033[7m";
    if (id[0]=='s' && id[1]=='e') return "\033[0m";
    if (id[0]=='t' && id[1]=='i') return "\033[?25l";
    if (id[0]=='t' && id[1]=='e') return "\033[0m\033[?25h";
    if (id[0]=='h' && id[1]=='o') return "\033[H";
    if (id[0]=='l' && id[1]=='l') return "\033[999;1H";
    if (id[0]=='p' && id[1]=='c') return "";
    if (id[0]=='b' && id[1]=='c') return "\b";
    return (char *)0;
}

char *tgoto(const char *cm, int destcol, int destline) {
    static char buf[32];
    (void)cm;
    snprintf(buf, sizeof(buf), "\033[%d;%dH", destline + 1, destcol + 1);
    return buf;
}

int tputs(const char *str, int affcnt, int (*putc_fn)(int)) {
    (void)affcnt;
    if (!str || !putc_fn) return 0;
    while (*str) putc_fn((unsigned char)*str++);
    return 0;
}

#endif // AEE32
