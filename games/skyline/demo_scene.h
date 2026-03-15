/*! @file demo_scene.c
    @brief Creation of the starry night/skyline screen
    @copyright Copyright (c) 2024-2026 University of Illinois
    @license SPDX-License-identifier: NCSA
*/

#ifndef DEMO_SCENE_H_
#define DEMO_SCENE_H_

#include <stdint.h>
#include <stddef.h>

#include "skyline.h"

typedef void (*demo_add_star_fn)(int32_t x, int32_t y, uint32_t color);
typedef void (*demo_remove_star_fn)(int32_t x, int32_t y);
typedef void (*demo_add_window_fn)(int32_t x, int32_t y, uint8_t w, uint8_t h, uint32_t color);
typedef void (*demo_remove_window_fn)(int32_t x, int32_t y);
typedef void (*demo_start_beacon_fn)(
    const uint32_t *img,
    int32_t x,
    int32_t y,
    uint8_t dia,
    uint16_t period,
    uint16_t ontime);
typedef void (*demo_draw_star_fn)(uint32_t *fbuf, const struct skyline_star *star);
typedef void (*demo_draw_window_fn)(uint32_t *fbuf, const struct skyline_window *win);
typedef void (*demo_draw_beacon_fn)(uint32_t *fbuf, uint64_t t, const struct skyline_beacon *bcn);
typedef void (*demo_fillscreen_fn)(uint32_t *fbuf, uint32_t color);

#ifndef SKYLINE_FREQ
#define SKYLINE_FREQ 10
#endif

#ifndef DEMO_BLDG_CNT
#define DEMO_BLDG_CNT 30
#endif

#define DEMO_BEACON_START (3 * SKYLINE_FREQ)

#ifndef DEMO_BEACON_FLASHES_PER_MIN
#define DEMO_BEACON_FLASHES_PER_MIN 8
#endif

#define DEMO_BFCNT_MIN 6
#define DEMO_BFCNT_MAX 40

#define DEMO_FWCNT_MIN 6
#define DEMO_FWCNT_MAX 10

#define DEMO_FLOOR_HEIGHT_MIN 3
#define DEMO_FLOOR_HEIGHT_MAX 4

#define DEMO_WIN_TOGGLE_WAIT 4
#define DEMO_SKY_COLOR 0x00000000

#define DEMO_INIT_LIT_WIN_PER256 32
#define DEMO_WIN_STEADY_TARGET_PER256 200
#define DEMO_WIN_STEADY_BAND 48
#define DEMO_WIN_UPDATES_PER_TICK 4

#define DEMO_WIN_MAX (DEMO_BLDG_CNT * DEMO_BFCNT_MAX * DEMO_FWCNT_MIN)

#define DEMO_STARS_MAX 1000
#define DEMO_STAR_STEADY_TARGET 900
#define DEMO_STAR_STEADY_BAND 30
#define DEMO_STAR_TWINKLES_PER_TICK 1

#define COL32(r, g, b) ( \
    (0xFFu << 24) | \
    ((uint32_t)(r) << 16) | \
    ((uint32_t)(g) << 8) | \
    ((uint32_t)(b) << 0) \
)

struct demo_window {
    int32_t x;
    int32_t y;
    uint8_t w;
    uint8_t h;
    uint32_t color;
};

struct demo_win_spec {
    uint8_t fht;
    uint8_t hwp;
    uint8_t w;
    uint8_t h;
};

struct demo_building {
    int16_t xmin;
    int16_t xmax;
    int16_t yroof;
    uint16_t w;
    uint16_t h;
    uint8_t bfcnt;
    uint8_t fwcnt;
    const struct demo_win_spec *wspec;
};

extern int32_t demo_star_cnt;
extern struct skyline_star demo_stars[DEMO_STARS_MAX];
extern int32_t demo_beacon_xpos;
extern int32_t demo_beacon_ypos;
extern int demo_beacon_started;

extern struct demo_building demo_bldgs[DEMO_BLDG_CNT];
extern struct demo_window demo_windows[DEMO_WIN_MAX];
extern int8_t demo_win_state[DEMO_WIN_MAX];
extern uint32_t demo_win_cnt;
extern uint32_t demo_win_lit_cnt;

extern void * kmalloc(size_t size);
extern void kfree(void * ptr);

void demo_add_rand_star(demo_add_star_fn add_cb);
void demo_remove_rand_star(demo_remove_star_fn remove_cb);
void demo_create_initial_stars(demo_add_star_fn add_cb);
void demo_create_bldgs(void);
void demo_set_building_palette(const uint32_t *colors, size_t count);
void demo_seed_initial_windows(demo_add_window_fn add_cb, demo_remove_window_fn remove_cb);
void demo_update_windows(demo_add_window_fn add_cb, demo_remove_window_fn remove_cb);
void demo_create_beacon(demo_start_beacon_fn start_cb);
void demo_render_scene(
    uint32_t *fbuf,
    uint64_t tickcnt,
    uint32_t sky_color,
    demo_fillscreen_fn fillscreen_cb,
    demo_draw_star_fn draw_star_cb,
    demo_draw_window_fn draw_window_cb,
    demo_draw_beacon_fn draw_beacon_cb);

#endif // DEMO_SCENE_H_
