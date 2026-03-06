#include "../usr/string.h"
#include "../usr/heap.h"
#include "../usr/syscall.h"
#include "../usr/io.h"
#include "../usr/shell.h"
#include "after_dark.h"
#include <stdint.h>

struct pixel {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t padding;
};

// number of gameticks
static volatile uint64_t tickcnt;
// how many times per second tickcnt will increment
#define TICKS_PER_SECOND 30

// toaster should move every n gameticks
#define TICKS_PER_TOASTER_MOVE 1
// toast should move once every n gameticks
#define TICKS_PER_TOAST_MOVE 2
// add a new toaster every n gameticks
#define TICKS_PER_ADD_NEW_TOASTER 30
// add a new toast every n gameticks
#define TICKS_PER_ADD_NEW_TOAST 60

#define MTIME_FREQ 10000000

struct pixel* rgbx32_fbuf;

static uint64_t rand_state = 0;

void rand_init(void) {
    // Read from viorng to initialize
    int rng_fd = _open(-1, "dev/viorng0");

    if (rng_fd < 0) {
        printf("failed to open viorng: %d\n", rng_fd);
        _exit();
    }

    if (_read(rng_fd, &rand_state, sizeof(rand_state)) < 0) {
        printf("failed to read from viorng\n");
        _exit();
    }

    _close(rng_fd);
}

uint_fast32_t rand(void) {
    rand_state *= 25214903917UL;
    rand_state += 11;
    return (uint32_t)(rand_state >> 12);
}

// Returns a value chosen uniformly at random from the range [rmin,rmax].

int_fast32_t randrange(int_fast32_t rmin, int_fast32_t rmax) {
    return rmin + rand() % (rmax - rmin + 1);
}

void draw_all_toasters(void) {
    for (int i = 0; i < toaster_cnt; i++) {
        draw_toaster(&toasters[i], (void *)rgbx32_fbuf);
    }
}

void move_all_toasters(void) {
    for (int i = 0; i < toaster_cnt; i++) {
        move_toaster(&toasters[i]);
    }
}

void remove_out_of_bounds_toasters(void) {
    for (int i = 0; i < toaster_cnt; i++) {
        if ((toasters[i].x <= -64) || (toasters[i].y >= SCREEN_HEIGHT)) {
            remove_toaster(toasters[i].x, toasters[i].y);
        }
    }
}

void draw_all_toast(void) {
    struct toast* p = toast_list;
    while (p != NULL) {
        draw_toast(p, (void *)rgbx32_fbuf);
        p = p->next;
    }
}

void move_all_toast(void) {
    struct toast* p = toast_list;
    while (p != NULL) {
        move_toast(p);
        p = p->next;
    }
}

void remove_out_of_bounds_toast(void) {
    struct toast* p = toast_list;
    while (p != NULL) {
        if ((p->x <= -64) || (p->y >= SCREEN_HEIGHT)) {
            remove_toast(p->x, p->y);
        }
        p = p->next;
    }
}

int overlaps(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    // Check if two 64x64 rectangles at (x1,y1) and (x2,y2) overlap
    return !(x1 + 63 < x2 || x2 + 63 < x1 ||
             y1 + 63 < y2 || y2 + 63 < y1);
}

int overlaps_with_anything(int16_t x, int16_t y) {
    int retval = 0;
    for (int i = 0; i < toaster_cnt; i++) {
        if (overlaps(toasters[i].x, toasters[i].y, x, y)) {
            retval = 1;
        }
    }
    struct toast* p = toast_list;
    while (p != NULL) {
        if (overlaps(p->x, p->y, x, y)) {
            retval = 1;
        }
        p = p->next;
    }
    return retval;
}

static int can_add_new_toaster = 0;
static int can_add_new_toast = 0;

void demo_step(void) {
    if ((tickcnt % TICKS_PER_TOASTER_MOVE) == 0) {
        move_all_toasters();
    }
    if ((tickcnt % TICKS_PER_TOAST_MOVE) == 0) {
        move_all_toast();
    }
    if ((tickcnt % TICKS_PER_ADD_NEW_TOASTER) == 0) {
        can_add_new_toaster = 1;
    }
    if ((tickcnt % TICKS_PER_ADD_NEW_TOAST) == 0) {
        can_add_new_toast = 1;
    }

    if (can_add_new_toaster) {
        int16_t x = (int16_t)randrange(64, 900);
        if (!overlaps_with_anything(x, -64)) {
            uint8_t initial_phase = (uint8_t)randrange(0,3);
            add_toaster(x, -64, initial_phase);
            can_add_new_toaster = 0;
        }
    }
    if (can_add_new_toast) {
        int16_t x = (int16_t)randrange(64, 900);
        if (!overlaps_with_anything(x, -64)) {
            add_toast(x, -64);
            can_add_new_toast = 0;
        }
    }


    remove_out_of_bounds_toasters();
    remove_out_of_bounds_toast();

    // black out the screen and then draw over it
    memset(rgbx32_fbuf, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(struct pixel));
    draw_all_toasters();
    draw_all_toast();

    // iterate through pixels to fix color mapping
    for (struct pixel* p = rgbx32_fbuf; p < rgbx32_fbuf + (SCREEN_WIDTH * SCREEN_HEIGHT); p++) {
        p->blue = p->green;
        p->green = p->red;
        p->red = p->padding;
        p->padding = 0;
    }
}

struct toaster toasters[TOASTERS_MAX] = {0};
uint16_t toaster_cnt = 0;

struct toast* toast_list = NULL;

static unsigned long long inline get_time() {
    unsigned long long time;
    asm ("rdtime %0" : "=r"(time));
    return time;
}

void main(void) {
    // open gpu device
    int result, gpu_fd;

    unsigned long long sleepuntil = get_time();
    unsigned long long ctime;

    gpu_fd = _open(-1, "dev/viogpu0");
    if (gpu_fd < 0) {
        printf("failed to open gpu device: %d", gpu_fd);
        _exit();
    }

    result = _ioctl(gpu_fd, /*IOC_MAPBUF*/ 8, &rgbx32_fbuf);
    if (result < 0) {
        printf("failed to map frame buffer: %d", result);
        _exit();
    }

    rand_init();

    for (;;) {
        sleepuntil += (MTIME_FREQ / TICKS_PER_SECOND);
        ctime = get_time();
        if (sleepuntil > ctime) _usleep((sleepuntil - ctime) / (MTIME_FREQ / 1000000));

        tickcnt++;
        demo_step();

        // flush gpu
        result = _write(gpu_fd, NULL, 0);
        if (result < 0) {
            printf("failed to flush gpu");
            _exit();
        }
    }
}