#ifndef TETRIS_COMPAT_H
#define TETRIS_COMPAT_H

#include <stddef.h>

typedef unsigned long nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN 0x0001
#ifndef INFTIM
#define INFTIM (-1)
#endif

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TIOCGWINSZ 0x5413

int ioctl(int fd, unsigned long req, void *arg);

typedef unsigned int tcflag_t;
typedef unsigned int speed_t;
typedef unsigned char cc_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[32];
};

#define ICANON 0x0002
#define ECHO 0x0008
#ifndef XTABS
#define XTABS 0x0004
#endif
#define VMIN 6
#define VTIME 5
#define TCSADRAIN 0

int tcgetattr(int fd, struct termios *tp);
int tcsetattr(int fd, int optional_actions, const struct termios *tp);
speed_t cfgetospeed(const struct termios *tp);

extern char PC;
extern char *BC;
extern char *UP;

int tgetent(char *bp, const char *name);
int tgetflag(const char *id);
int tgetnum(const char *id);
char *tgetstr(const char *id, char **area);
char *tgoto(const char *cm, int destcol, int destline);
int tputs(const char *str, int affcnt, int (*putc_fn)(int));

void warn(const char *fmt, ...);
void warnx(const char *fmt, ...);
void err(int eval, const char *fmt, ...);
void errx(int eval, const char *fmt, ...);

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

int getopt(int argc, char * const argv[], const char *optstring);

#endif