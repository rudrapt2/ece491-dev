#ifdef AEE32

#include "usr/heap.h"
#include "usr/syscall.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "source/compat.h"
#include "source/stdio.h"

extern size_t vsnprintf(char * buf, size_t bufsz, const char * fmt, va_list ap);
extern size_t snprintf(char * buf, size_t bufsz, const char * fmt, ...);

// Minimal runtime glue

static unsigned long long rnd_state = 391ULL;
static int rtc_fd = -1;

// ---- output buffer to reduce syscalls and prevent cursor flicker ----
#define OUTBUF_SZ 2048
static char outbuf[OUTBUF_SZ];
static int  outbuf_len = 0;

static void outbuf_flush(void) {
    if (outbuf_len > 0) {
        _write(1, outbuf, (size_t)outbuf_len);
        outbuf_len = 0;
    }
}

static void outbuf_putc(char c) {
    if (outbuf_len >= OUTBUF_SZ) outbuf_flush();
    outbuf[outbuf_len++] = c;
}

static void outbuf_puts(const char *s, int n) {
    while (n > 0) {
        int space = OUTBUF_SZ - outbuf_len;
        int chunk = (n < space) ? n : space;
        memcpy(outbuf + outbuf_len, s, (size_t)chunk);
        outbuf_len += chunk;
        s += chunk;
        n -= chunk;
        if (outbuf_len >= OUTBUF_SZ) outbuf_flush();
    }
}

#define IOC_GETEND 4

struct viohi_event {
    unsigned short type;
    unsigned short code;
    unsigned int value;
};

#define EV_KEY 1

#define KEY_ESC   1
#define KEY_Q    16
#define KEY_P    25
#define KEY_D    32
#define KEY_J    36
#define KEY_K    37
#define KEY_L    38
#define KEY_SPACE 57
#define KEY_UP   103
#define KEY_LEFT 105
#define KEY_RIGHT 106
#define KEY_DOWN 108

static int map_vkey_to_char(unsigned short code) {
    switch (code) {
    case KEY_J:
    case KEY_LEFT:
        return 'j';
    case KEY_K:
    case KEY_UP:
        return 'k';
    case KEY_L:
    case KEY_RIGHT:
        return 'l';
    case KEY_SPACE:
    case KEY_DOWN:
        return ' ';
    case KEY_P:
        return 'p';
    case KEY_D:
        return 'd';
    case KEY_Q:
    case KEY_ESC:
        return 'q';
    default:
        return -1;
    }
}

static void setup_input_relay(void) {
    int wfd = -1, rfd = -1;
    if (_pipe(&wfd, &rfd) < 0) return;

    int relay_pid = _fork();
    if (relay_pid == 0) {
        _close(rfd);
        int kfd = _open(-1, "/dev/viohi0");
        if (kfd < 0)
            kfd = _open(-1, "/dev/viohi");

        if (kfd >= 0) {
            for (;;) {
                struct viohi_event ev;
                long n = _read(kfd, &ev, sizeof(ev));
                if (n != (long)sizeof(ev)) _exit();
                if (ev.type != EV_KEY || ev.value == 0)
                    continue;
                int key = map_vkey_to_char(ev.code);
                if (key < 0)
                    continue;
                char ch = (char)key;
                if (_write(wfd, &ch, 1) != 1) _exit();
            }
        }

        for (;;) {
            char ch;
            long n = _read(0, &ch, 1);
            if (n != 1) _exit();
            if (_write(wfd, &ch, 1) != 1) _exit();
        }
    }

    int tick_pid = _fork();
    if (tick_pid == 0) {
        _close(rfd);
        for (;;) {
            char tick = '\0';
            if (_write(wfd, &tick, 1) != 1) _exit();
                _usleep(300000);
        }
    }

    _close(wfd);
    _close(0);
    _iodup(rfd, 0);
    _close(rfd);
}

FILE __stdin_file = {0};
FILE __stdout_file = {1};
FILE __stderr_file = {2};
FILE *stdin = &__stdin_file;
FILE *stdout = &__stdout_file;
FILE *stderr = &__stderr_file;

static void reset_terminal_state(void) {
    static const char reset_seq[] = "\033[0m\033[?25h\033[H\033[J";
    _write(1, reset_seq, sizeof(reset_seq) - 1);
}

int main(int argc, char **argv) {
    extern int tetris_main(int, char **);
    rtc_fd = _open(-1, "/dev/rtc");
    setup_input_relay();
    return tetris_main(argc, argv);
}

void exit(int status) {
    (void)status;
    reset_terminal_state();
    _exit();
}

void abort(void) {
    reset_terminal_state();
    _exit();
}

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

int open(const char *path, int flags, ...) {
    (void)flags;
    return _open(-1, path);
}

int close(int fd) { return _close(fd); }

long read(int fd, void *buf, size_t n) {
    return _read(fd, buf, n);
}

long write(int fd, const void *buf, size_t n) {
    return _write(fd, buf, n);
}

int putchar(int c) {
    outbuf_putc((char)c);
    return c;
}

int getchar(void) {
    unsigned char ch;
    return (_read(0, &ch, 1) == 1) ? (int)ch : EOF;
}

int fputs(const char *s, FILE *fp) {
    if (!s) return EOF;
    int n = (int)strlen(s);
    int fd = fp ? fp->fd : 1;
    if (fd == 1) {
        outbuf_puts(s, n);
    } else {
        _write(fd, s, (size_t)n);
    }
    return n;
}

int fflush(FILE *fp) {
    (void)fp;
    outbuf_flush();
    return 0;
}

int fprintf(FILE *fp, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = (int)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    int fd = fp ? fp->fd : 1;
    if (n > 0) {
        if (fd == 1)
            outbuf_puts(buf, n);
        else
            _write(fd, buf, (size_t)n);
    }
    return n;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = (int)vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    return n;
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

struct timeval { long tv_sec; long tv_usec; };

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    unsigned long long ts = 0;
    if (rtc_fd >= 0) _read(rtc_fd, &ts, sizeof(ts));
    if (tv) {
        tv->tv_sec = (long)ts;
        tv->tv_usec = 0;
    }
    return 0;
}

struct pollfd;
#define IOC_GETPOS 6

static int poll_ready(struct pollfd *fds, nfds_t nfds) {
    int ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) continue;
        if ((fds[i].events & POLLIN) == 0) continue;

        unsigned long long pos = 0, end = 0;
        if (_ioctl(fds[i].fd, IOC_GETPOS, &pos) == 0 &&
            _ioctl(fds[i].fd, IOC_GETEND, &end) == 0 &&
            end > pos) {
            fds[i].revents |= POLLIN;
            ready++;
        }
    }
    return ready;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    int ready = poll_ready(fds, nfds);
    if (ready > 0) return ready;

    if (timeout == 0) return 0;

    if (timeout < 0) {
        for (;;) {
            _usleep(1000UL);
            ready = poll_ready(fds, nfds);
            if (ready > 0) return ready;
        }
    }

    _usleep((unsigned long)timeout * 1000UL);
    return poll_ready(fds, nfds);
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
