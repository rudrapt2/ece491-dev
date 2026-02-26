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
#define KEY_ENTER 28
#define KEY_D    32
#define KEY_J    36
#define KEY_K    37
#define KEY_L    38
#define KEY_SPACE 57
#define KEY_KPENTER 96
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
    case KEY_ENTER:
    case KEY_KPENTER:
        return '\n';
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

#endif // AEE32
