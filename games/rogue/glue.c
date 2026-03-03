// glue.c - Stuff to make rogue work with our bare-metal OS
// 

#include "glue.h"
#include <stdarg.h>
#include <stddef.h>
#include <limits.h>

const char * const termcap = ""
"vt100|vt100-am|dec vt100 (w/advanced video):"
":am:bs:ms:xn:xo:"
":co#80:it#8:li#24:vt#3:"
":@8=\\EOM:DO=\\E[%dB:K1=\\EOq:K2=\\EOr:K3=\\EOs:K4=\\EOp:K5=\\EOn:"
":LE=\\E[%dD:RA=\\E[?7l:RI=\\E[%dC:SA=\\E[?7h:UP=\\E[%dA:"
":ac=``aaffggjjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~:"
":ae=\\E(B:as=\\E(0:bl=^G:cb=\\E[1K:cd=\\E[J:ce=\\E[K:cl=\\E[H\\E[J:"
":cm=\\E[%i%d;%dH:cr=^M:cs=\\E[%i%d;%dr:ct=\\E[3g:do=\\E[B:"
":eA=\\E(B:ho=\\E[H:k0=\\EOy:k1=\\EOP:k2=\\EOQ:k3=\\EOR:"
":k4=\\EOS:k5=\\EOt:k6=\\EOu:k7=\\EOv:k8=\\EOl:k9=\\EOw:k;=\\EOx:"
":kb=^H:kd=\\EOB:ke=\\E[?1l\\E>:kl=\\EOD:kr=\\EOC:ks=\\E[?1h\\E=:"
":ku=\\EOA:le=^H:mb=\\E[5m:md=\\E[1m:me=\\E[m\\017:mr=\\E[7m:"
":nd=\\E[C:r2=\\E>\\E[?3l\\E[?4l\\E[?5l\\E[?7h\\E[?8h:rc=\\E8:"
":sc=\\E7:se=\\E[m:sf=^J:so=\\E[7m:sr=\\EM:st=\\EH:ta=^I:ue=\\E[m:"
":up=\\E[A:us=\\E[4m";

#define MICROSECONDS_PER_SECOND  1000000
#define RAND_DEFAULT    391

#ifdef AEE31
#include "kernel/string.h"
#include "kernel/setjmp.h"
#include "usr/io.h"

static struct io_term rogue_term;
static struct io * rng_io = NULL;
static jmp_buf exit_jmp;

void main(struct io *termio, struct io *rand_io) {
    extern int rogue_main(int argc, char *argv[]);
    char *argv[] = {"rogue", NULL};

    ioterm_init(&rogue_term, termio);
    rng_io = rand_io;

    if (setjmp(exit_jmp) == 0)
        rogue_main(1, argv);
}

void exit(void) {
    longjmp(exit_jmp, 1);
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    iovprintf(&rogue_term.io, fmt, ap);
    va_end(ap);
}

int sprintf(char *buf, const char *fmt, ...) {
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buf, 256, fmt, ap); // fake buffer size
    va_end(ap);
    return n;
}

char *strcpy(char *dst, const char *src) {
    return strncpy(dst, src, 256); // surely ...
}

char *strcat(char *dst, const char *src) {
    return strncpy(dst + strlen(dst), src, 256);
}

int fopen(const char *fname) {
    return 0; // no filesystem in CP1
}

int fclose(int fd) {
    return 0;
}

void fputs(const char *str, int fd) {
    iowrite(&rogue_term.io, str, strlen(str));
}

void fputc(int fd, char c) {
    iowrite(&rogue_term.io, &c, 1);
}

int fwrite(const void *ptr, size_t size, size_t nmemb, int fd) {
    return 0; // no filesystem in CP1
}

int fread(void *ptr, size_t size, size_t nmemb, int fd) {
    return 0; // no filesystem in CP1
}

void sleep(unsigned int cnt) {
    (void)cnt; // no timer in CP1
}

int putchar(int c) {
    ioputc(&rogue_term.io, (char)c);
    return c;
}

int getchar(void) {
    char c;
    ioread(&rogue_term.io, &c, 1);
    return (int)(unsigned char)c;
}

// Bump allocator for the small termcap string copies in curses.c.
// curses.c calls malloc() exactly once per termcap field (~10 fields, each
// a few bytes).  A 512-byte static pool is more than sufficient.
static char malloc_pool[512];
static int  malloc_used = 0;

void *malloc(size_t sz) {
    // align to 8 bytes
    sz = (sz + 7) & ~(size_t)7;
    if (malloc_used + (int)sz > (int)sizeof(malloc_pool))
        return (void *)0;
    void *p = malloc_pool + malloc_used;
    malloc_used += (int)sz;
    return p;
}

int random_seed(void) {
    int r;

    if (rng_io != NULL) {
        if (ioread(rng_io, &r, sizeof(int)) == sizeof(int))
            return r;
    }

    printf("Error reading random number, defaulting to set value...\n");
#ifndef RAND_SEED
    return RAND_DEFAULT;
#else
    return RAND_SEED;
#endif
}

#endif // AEE31

#ifdef AEE32
int sprintf(char * buf, const char * fmt, ... ) {
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buf, 256, fmt, ap); // fake buffer size
    va_end(ap);
    return n;
}
   
char * strcpy(char * dst, const char * src) {
    return strncpy(dst, src, 256); // surely ...
}

extern char * strcat(char * dst, const char * src) {
    return strncpy(dst + strlen(dst), src, 256);
}

static int rng_fd;
static void rand_init(void);

void main(int argc, char *argv[]) {
    extern int rogue_main(int argc, char *argv[]);
    rand_init();
    rogue_main(argc, argv);
    exit();
}

inline void exit() {
    _exit();
}

int fopen(const char *fname) {
    int result;
    _create(fname);
    return _open(-1, fname);
}

int fclose(int fd) {
    return _close(fd);
}

void fputs(const char *str, int fd) {
    dputs(fd, str);
}

void fputc(int fd, char c) {
    dputc(fd, c);
}

int fwrite(const void *ptr, size_t size, size_t nmemb, int fd) {
    return _write(fd, ptr, size * nmemb);
}

int fread(void *ptr, size_t size, size_t nmemb, int fd) {
    return _read(fd, ptr, size * nmemb);
}

void sleep(unsigned int cnt) {
    _usleep(cnt * MICROSECONDS_PER_SECOND);
}

int putchar(int c) {
    putc(c);
    return c;
}

int getchar(void) {
    return getc();
}

void rand_init(void) {
    if ((rng_fd = _open(-1, "/dev/viorng0")) < 0) {
        printf("Failed to open viorng\n");
    }
}

int random_seed(void) {
    int r;

    if (rng_fd >= 0) {
        if (_read(rng_fd, &r, sizeof(int)) == sizeof(int)) 
            return r;
    }

    printf("Error reading random number, defaulting to set value...\n");
#ifndef RAND_SEED
    return RAND_DEFAULT;
#else 
    return RAND_SEED;
#endif
    
}

#endif // AEE32