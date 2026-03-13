/*! @file demo.c
    @brief Demo runner for student code
    @copyright Copyright (c) 2024-2026 University of Illinois
    @license SPDX-License-identifier: NCSA
*/

#include "usr/error.h"
#include "usr/string.h"
#include "usr/syscall.h"
#include "skyline.h"
#include "demo_scene.h"

#include <stdint.h>

extern void rand_init();
extern uint64_t rand();
extern int_fast32_t rand1(int_fast32_t rmin, int_fast32_t rmax);
extern int_fast32_t rand2(int_fast32_t rmin, int_fast32_t rmax);
extern int_fast32_t rand3(int_fast32_t rmin, int_fast32_t rmax);

extern void gpu_init();
extern long gpu_draw();
extern void gpu_attach_buffer(void * fbuf_ptr);

// INTERNAL GLOBAL VARIABLE DEFINITIONS
// 

static volatile uint64_t tickcnt;

// INTERNAL FUNCTION DECLARATIONS
//

static void demo_init(void);
static void demo_step(void);

static void wait(void); // wait for timer interrupt
static void fillscreen(uint32_t * fbuf, uint32_t color);

// EXPORTED FUNCTION DEFINITIONS
//

void add_star(int32_t x, int32_t y, uint32_t color)
    __attribute__((weak, alias("pb_add_star")));

void remove_star(int32_t x, int32_t y)
    __attribute__((weak, alias("pb_remove_star")));

void draw_star(uint32_t * fbuf, const struct skyline_star * star)
    __attribute__((weak, alias("pb_draw_star")));

void add_window (
    int32_t x, int32_t y,
    uint8_t w, uint8_t h,
    uint32_t color)
    __attribute__((weak, alias("pb_add_window")));

void remove_window(int32_t x, int32_t y)
    __attribute__((weak, alias("pb_remove_window")));

void draw_window(uint32_t * fbuf, const struct skyline_window * win)
    __attribute__((weak, alias("pb_draw_window")));

void start_beacon (
    const uint32_t * img,
    int32_t x,
    int32_t y,
    uint8_t dia,
    uint16_t period,
    uint16_t ontime)
    __attribute__((weak, alias("pb_start_beacon")));

void draw_beacon (
    uint32_t * fbuf,
    uint64_t t, // current time
    const struct skyline_beacon * bcn)
    __attribute__((weak, alias("pb_draw_beacon")));

static uint32_t * FBUF;

void main(void) {
    gpu_init();
    rand_init();
    gpu_attach_buffer(&FBUF);

    demo_init();

    for (;;) {
        wait();
        demo_step();
    }
}

void demo_init(void) {
    tickcnt = 0;
    demo_win_cnt = 0;
    demo_win_lit_cnt = 0;
    memset(demo_win_state, 0, sizeof(demo_win_state));
    fillscreen(FBUF, COL32(0x00, 0x00, 0x00));
    demo_create_bldgs();
    demo_seed_initial_windows(add_window, remove_window);
    demo_create_initial_stars(add_star);
}

void demo_step(void) {
    int i;

    // Star updates: keep a steady population but constantly twinkle

    for (i = 0; i < DEMO_STAR_TWINKLES_PER_TICK; i++) {
        if (demo_star_cnt < DEMO_STAR_STEADY_TARGET - DEMO_STAR_STEADY_BAND)
            demo_add_rand_star(add_star);
        else if (demo_star_cnt > DEMO_STAR_STEADY_TARGET + DEMO_STAR_STEADY_BAND)
            demo_remove_rand_star(remove_star);
        else if ((rand() & 1) == 0)
            demo_add_rand_star(add_star);
        else
            demo_remove_rand_star(remove_star);
    }

    // Window updates

    if ((tickcnt % DEMO_WIN_TOGGLE_WAIT) == 0)
        demo_update_windows(add_window, remove_window);

    // // Beacon updates

    if (DEMO_BEACON_START <= tickcnt) {
        if (!demo_beacon_started)
            demo_create_beacon(start_beacon);
    }

    // Blank the screen

    demo_render_scene(
        FBUF,
        tickcnt,
        DEMO_SKY_COLOR,
        fillscreen,
        draw_star,
        draw_window,
        draw_beacon);

    gpu_draw();
}

// MISC INTERNAL FUNCTION DEFINITIONS

void fillscreen(uint32_t * fbuf, uint32_t color) {
    for (int i = 0; i < SKYLINE_WIDTH * SKYLINE_HEIGHT; i++)
        fbuf[i] = color;
}

static void wait(void) {
    _usleep(1000*1000/SKYLINE_FREQ);
    tickcnt++;
}

void pb_add_star(int32_t x, int32_t y, uint32_t color) {
    return;
}

void pb_remove_star(int32_t x, int32_t y) {
    return;
}

void pb_draw_star(uint32_t * fbuf, const struct skyline_star * star) {
    return;
}

void pb_add_window (
    int32_t x, int32_t y,
    uint8_t w, uint8_t h,
    uint32_t color) {
    return;
}

void pb_remove_window(int32_t x, int32_t y) {
    return;
}

void pb_draw_window(uint32_t * fbuf, const struct skyline_window * win) {
    return;
}

void pb_start_beacon (
    const uint32_t * img,
    int32_t x,
    int32_t y,
    uint8_t dia,
    uint16_t period,
    uint16_t ontime) {
    return;
}

void pb_draw_beacon (
    uint32_t * fbuf,
    uint64_t t, // current time
    const struct skyline_beacon * bcn) {
    return;
}