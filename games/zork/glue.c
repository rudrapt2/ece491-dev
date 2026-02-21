// glue.c - Stuff to make rogue work with our bare-metal OS
// 

#define AEE32
#include "glue.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifndef RAND_MAX
#define RAND_MAX 0x7fffffff
#endif

static uint64_t rndst;

static void rtc_init(void);
static void rand_init(void);
static void wait_for_enter(void);

#ifdef AEE31
#include "kernel/io.h"
#include "kernel/thread.h"
#include "kernel/device.h"
#include "kernel/filesys.h"

struct io * file = NULL;
struct io * rtc_io = NULL;
struct io * term_io;
// struct io * file_list[] = {NULL, NULL}; // zork only opens two files

int main(struct io * termio) {
    extern void zork_start_game(void);
    
    term_io = termio;
    rtc_init();
    rand_init();
    wait_for_enter();  
    zork_start_game();
    return 0;
}

void exit(int result) {
    wait_for_enter(); // let user read exit message
    exit_running_thread();
}

int fopen(const char *fname) {
    create_file("c", fname);
    return open_file("c", fname, &file);
}

int ftell(int fd) {
    int result;
    unsigned long long fpos;
    ioctl(file, IOC_GETPOS, &fpos);
    if ((result = ioctl(file, IOC_GETPOS, &fpos)) < 0) {
        return result;
    }
    return (int) fpos;
}

int fseek(int fd, uint64_t pos) {
    return ioctl(file, IOC_SETPOS, &pos);
}

int fgetc(int fd) {
    int result;
    char c;
    if ((result = ioread(file, &c, 1)) < 0) {
        printf("Error reading file: %d\n");
        exit(result);
    }
    return c;
}

int fclose(int fd) {
    iodropref(file);
    return 0;
}

int fwrite(const void *ptr, size_t size, size_t nmemb, int fd) {
    return iowrite(file, ptr, size * nmemb);
}

int fread(void *ptr, size_t size, size_t nmemb, int fd) {
    return ioread(file, ptr, size * nmemb);
}

int time(uint64_t *timebuf) {
    return ioread(rtc_io, timebuf, sizeof(uint64_t));
}

void rtc_init(void) {
    if (open_device("rtc", &rtc_io) < 0) {
        printf("RTC failed to open\n");
    }
}

void rand_init(void) {
    uint64_t t = 0;
    struct io * rng_fd;
    if (open_device("viorng0", &rng_fd) == 0)
        if (ioread(rng_fd, &rndst, sizeof(rndst)) == sizeof(rndst)) 
            return;

    printf("Failed to read from viorng, getting random seed\n");
#ifndef RAND_SEED
    if (rtc_io != NULL && time(&t) > 0) {
        rndst ^= t << 16;
    } else {
        printf("Error getting random seed, defaulting to 391\n");
        rndst = 391;
    }
#else 
    rndst = RAND_SEED;
#endif
}

int rand(void) {
    rndst *= 25214903917UL;
    rndst += 11;
    return (rndst >> 12) % ((uint64_t)RAND_MAX + 1);
}

void wait_for_enter(void) {
    char buf[1];
    getsn(buf, sizeof(buf));
}
#endif // AEE31

#ifdef AEE32
#include "usr/string.h"
#include "usr/syscall.h"
#include "usr/io.h"

static int rtc_fd;

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
    _exit();
}

int fopen(const char *fname) {
    int result;
    char path[32];
    snprintf(path, 32, "/c/%s", fname);
    _create(path);
    return _open(-1, path);
}

int ftell(int fd) {
    int result;
    unsigned long long fpos;
    if ((result = _ioctl(fd, IOC_GETPOS, &fpos)) < 0) {
        return result;
    }
    return (int) fpos;
}

int fseek(int fd, uint64_t pos) {
    return _ioctl(fd, IOC_SETPOS, &pos);
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
    return _close(fd);
}

int fwrite(const void *ptr, size_t size, size_t nmemb, int fd) {
    return _write(fd, ptr, size * nmemb);
}

int fread(void *ptr, size_t size, size_t nmemb, int fd) {
    return _read(fd, ptr, size * nmemb);
}

int time(uint64_t *timebuf) {
    return _read(rtc_fd, timebuf, sizeof(uint64_t));
}

void rtc_init(void) {
    if ((rtc_fd = _open(-1, "rtc")) < 0) {
        printf("RTC failed to open\n");
    }
}

void rand_init(void) {
    uint64_t t = 0;
    int rng_fd = _open(-1, "viorng0");
    if (rng_fd > 0) {
        if (_read(rng_fd, &rndst, sizeof(rndst)) == sizeof(rndst)) 
            return;
    }

    printf("Failed to read from viorng, getting random seed\n");
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

int rand(void) {
    rndst *= 25214903917UL;
    rndst += 11;
    return (rndst >> 12) % ((uint64_t)RAND_MAX + 1);
}

void wait_for_enter(void) {
    char buf[1];
    getsn(buf, sizeof(buf));
}
#endif
