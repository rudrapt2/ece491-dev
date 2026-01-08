// glue.c - Stuff to make rogue work with our bare-metal OS
// 

#include "glue.h"
#include "string.h"
#include "syscall.h"
#include <stdarg.h>
#include <stddef.h>
#include <limits.h>

#define STARTING_FD     3 // stdin, stdout, and uart are opened by default
#define RAND_DEFAULT    391

static int rtc_fd;
static int rng_fd;
static uint8_t open_devices = 0;

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

static int time(uint64_t *timebuf);
static void rtc_init(void);
static void rand_init(void);

void main(int argc, char *argv[]) {
    extern int rogue_main(int argc, char *argv[]);
    rtc_init();
    rand_init();
    rogue_main(argc, argv);
    exit();
}

void exit() {
    
    // close devices
    if (rtc_fd >= 0) {
        _close(rtc_fd);
    }
    
    if (rng_fd >= 0) {
        _close(rng_fd);
    }
    
    _exit();
}

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

#define NANOSECONDS_PER_SECOND  1000000000

int fopen(const char *fname) {
    int result;
    _fscreate(fname);
    if ((result = _open(STARTING_FD + open_devices, fname)) >= 0) {
        open_devices++;
    }
    return result;
}

int fclose(int fd) {
    open_devices--;
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
    uint64_t time_start;
    uint64_t time_now;

    // Very hack

    time(&time_start);
    do {
        time(&time_now);
    } while (time_now < time_start + (uint64_t)cnt * NANOSECONDS_PER_SECOND);
}

int putchar(int c) {
    putc(c);
    return c;
}

int getchar(void) {
    return getc();
}

void rtc_init(void) {
    if ((rtc_fd = _open(STARTING_FD + open_devices, "dev/rtc0")) < 0) {
        printf("RTC failed to open\n");
    } else {
        open_devices++;
    }
}

void rand_init(void) {
    if ((rng_fd = _open(STARTING_FD + open_devices, "dev/viorng0")) < 0) {
        printf("Failed to open viorng\n");
    } else {
        open_devices++;
    }
}

int random_seed(void) {
    int r;
    int result;
    uint64_t t;

    if (rng_fd >= 0) {
        if ((result = _read(rng_fd, &r, sizeof(int))) >= 0) {
            return r;
        } else {
            printf("Error getting random number");
            return result;
        }
    } else {
    #ifndef RAND_SEED
        if (rtc_fd >= 0 && time(&t) > 0) {
            return t << 16;
        } else {
            printf("Error getting random seed, defaulting to 391\n");
            return RAND_DEFAULT;
        }
    #else 
        return RAND_SEED;
    #endif
    }
    
}

int time(uint64_t *timebuf) {
    if (rtc_fd > 0) {
        return _read(rtc_fd, timebuf, 8);
    } else {
        printf("Error: RTC not opened\n");
    }
    return -1;
}
