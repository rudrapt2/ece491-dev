// glue.c - Stuff to make rogue work with our bare-metal OS
// 

#include "glue.h"
#include "string.h"
#include "syscall.h"
#include "uio.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifndef RAND_MAX
#define RAND_MAX 0x7fffffff
#endif

static uint64_t rndst;
static int rtc_fd;
static int rng_fd;
static uint8_t open_devices = 0;

static void rtc_init(void);
static void rand_init(void);
static void wait_for_enter(void);

int main(void) {
    extern void zork_start_game(void);
    
    rtc_init();
    rand_init();
    wait_for_enter();  
    zork_start_game();
    return 0;
}

void exit(int result) {
    getc(); // let user read exit message
    
    // close devices
    if (rtc_fd >= 0) {
        _close(rtc_fd);
    }
    
    if (rng_fd >= 0) {
        _close(rng_fd);
    }
    
    _exit();
}

#define MTIME_FREQ 10000000 // from QEMU include/hw/intc/riscv_aclint.h

int fopen(const char *fname) {
    int result;
    char path[32];
    snprintf(path, 32, "c/%s", fname);
    if ((result =_open(STARTING_FD + open_devices, path)) >= 0) {
        open_devices++;
    }
    if (result < 0) {
        if (_fscreate(path) == 0) 
            return _open(STARTING_FD + open_devices++, path);
    }
    return result;
}

int devopen(const char *name, int instno) {
    int result;
    char path[32];
    snprintf(path, 32, "dev/%s%d", name, instno);
    if ((result = _open(STARTING_FD + open_devices, path)) >= 0) {
        open_devices++;
    }
    return result;
}

int ftell(int fd) {
    int result;
    unsigned long long fpos;
    if ((result = _fcntl(fd, FCNTL_GETPOS, &fpos)) < 0) {
        return result;
    }
    return (int) fpos;
}

int fseek(int fd, uint64_t pos) {
    return _fcntl(fd, FCNTL_SETPOS, &pos);
}

int fgetc(int fd) {
    int result;
    char c;
    if ((result = _read(fd, &c, 1)) < 0) {
        printf("Error reading file: %d\n");
        exit(result);
    }
    return c;
}

int fclose(int fd) {
    open_devices--;
    return _close(fd);
}

int fwrite(const void *ptr, size_t size, size_t nmemb, int fd) {
    return _write(fd, ptr, size * nmemb);
}

int fread(void *ptr, size_t size, size_t nmemb, int fd) {
    return _read(fd, ptr, size * nmemb);
}

int time(uint64_t *timebuf) {
    return _read(rtc_fd, timebuf, 8);
}

void rtc_init(void) {
    if ((rtc_fd = devopen("rtc", 0)) < 0) {
        printf("RTC failed to open\n");
    }
}

void rand_init(void) {
    uint64_t t;
    if ((rng_fd = devopen("viorng", 0)) < 0) {
        printf("Failed to open viorng, getting random seed\n");
    #ifndef RAND_SEED
        if (rtc_fd >= 0 && time(&t) > 0) {
            rndst ^= t << 16;
        } else {
            printf("Error getting random seed, defaulting to 391\n");
            rndst = 391;
        }
    #else 
        rndst = RAND_SEED;
    #endif
    }
}

int rand(void) {
    int r;
    int result;
    if (rng_fd > 0) {
        if ((result = _read(rng_fd, &r, sizeof(int))) >= 0) {
            return r;
        } else {
            printf("Error getting random number");
            return result;
        } 
    } else {
        rndst *= 25214903917UL;
        rndst += 11;
        return (rndst >> 12) % ((uint64_t)RAND_MAX + 1);
    }
}

void wait_for_enter(void) {
    char buf[1];
    getsn(buf, sizeof(buf));
}
