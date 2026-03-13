/*! @file demo_scene.c
    @brief Creation of the starry night/skyline screen
    @copyright Copyright (c) 2024-2026 University of Illinois
    @license SPDX-License-identifier: NCSA
*/

#include "usr/error.h"
#include "usr/syscall.h"
#include "usr/string.h"
#include "usr/heap.h"
#include "demo_scene.h"

#include <stddef.h>
#include <stdint.h>

extern uint64_t rand(void);
extern int_fast32_t rand1(int_fast32_t rmin, int_fast32_t rmax);
extern int_fast32_t rand2(int_fast32_t rmin, int_fast32_t rmax);

// INTERNAL GLOBALS ---------------------------------------------------------

int32_t demo_star_cnt;
struct skyline_star demo_stars[DEMO_STARS_MAX];
int32_t demo_beacon_xpos;
int32_t demo_beacon_ypos;
int demo_beacon_started;

struct demo_building demo_bldgs[DEMO_BLDG_CNT];
struct demo_window demo_windows[DEMO_WIN_MAX];
int8_t demo_win_state[DEMO_WIN_MAX];
uint32_t demo_win_cnt;
uint32_t demo_win_lit_cnt;

struct skyline_star *skyline_star_list;
struct skyline_window skyline_windows[SKYLINE_WIN_MAX];
uint16_t skyline_win_cnt;
struct skyline_beacon skyline_beacon;

static const uint32_t demo_star_colors[] = {
    COL32(0xFF, 0xFF, 0xFF),
    COL32(0xFF, 0xFF, 0xFF),
    COL32(0xFF, 0xFF, 0xFF),
    COL32(0xF4, 0xF6, 0xEB),
    COL32(0xF6, 0xEB, 0xEC),
    COL32(0xF0, 0xF0, 0xFD)
};

static const struct demo_win_spec win_specs[] = {
    {.fht = 4, .hwp = 4, .w = 3, .h = 3},
    {.fht = 3, .hwp = 3, .w = 2, .h = 3},
    {.fht = 3, .hwp = 3, .w = 2, .h = 2},
};

static const uint32_t default_building_colors[] = {
    COL32(0xFF, 0xFF, 0xFF),
    COL32(0xFC, 0xFC, 0xFC),
    COL32(0xFA, 0xFA, 0xFA),
    COL32(0xE0, 0xE0, 0xFF),
    COL32(0xE8, 0xFF, 0xFF),
    COL32(0xFC, 0xFC, 0xE0),
    COL32(0xF6, 0xF6, 0xD0)
};

static const uint32_t *demo_building_colors = default_building_colors;
static size_t demo_building_color_cnt = sizeof(default_building_colors) / sizeof(default_building_colors[0]);

static void demo_set_window_state(uint32_t idx, int on, demo_add_window_fn add_cb, demo_remove_window_fn remove_cb);
static int demo_pick_window_with_state(int state);
static int verify_star_inputs(uint16_t x, uint16_t y, const char *ctx);
static int verify_star_struct(const struct skyline_star *star, const char *ctx);
static int verify_window_inputs(uint16_t x, uint16_t y, uint8_t w, uint8_t h, const char *ctx);
static int verify_window_struct(const struct skyline_window *win, const char *ctx);
static int verify_beacon_inputs(uint16_t x, uint16_t y, uint8_t dia, uint16_t period, uint16_t ontime, const char *ctx);
static int verify_beacon_struct(const struct skyline_beacon *bcn, const char *ctx);
static void __attribute__ ((noreturn)) panic_actual(const char* filename, int lineno, const char* msg);
static void assert_failed(const char* filename, int lineno, const char* stmt);

#define panic(msg)                               \
    do {                                         \
        panic_actual(__FILE__, __LINE__, (msg)); \
    } while (0)

#define assert(c)                                  \
    do {                                           \
        if (!(c)) {                                \
            assert_failed(__FILE__, __LINE__, #c); \
        }                                          \
    } while (0)

#define MIN(a,b) ((a<b) ? a : b)

void *kmalloc(size_t size) { 
    return malloc(size); 
}

void kfree(void *ptr) { 
    free(ptr); 
}

// INTERNAL HELPERS ---------------------------------------------------------

static int demo_star_exists(uint_fast16_t x, uint_fast16_t y) {
    for (int i = 0; i < demo_star_cnt; i++) {
        if (demo_stars[i].x == x && demo_stars[i].y == y)
            return 1;
    }
    return 0;
}

static void panic_actual(const char* filename, int lineno, const char* msg) {
    if (msg != NULL && *msg != '\0')
        printf("PANIC at %s:%d: %s\n", filename, lineno, msg);
    else
        printf("PANIC at %s:%d\n", filename, lineno);

    _exit();
}

static void assert_failed(const char* filename, int lineno, const char* stmt) {
    printf("ASSERT FAILED at %s:%d (%s)\n", filename, lineno, stmt);
    _exit();
}

// EXPORTED BUILDING / STAR / BEACON LOGIC ---------------------------------

void demo_add_rand_star(demo_add_star_fn add_cb) {
    if (DEMO_STARS_MAX <= demo_star_cnt)
        return;

    int_fast16_t x;
    int_fast16_t y;
    int occl;

    do {
        x = rand1(2, SKYLINE_WIDTH - 3);
        y = rand1(2, SKYLINE_HEIGHT - 3);
        occl = 0;

        for (int k = 0; k < DEMO_BLDG_CNT; k++) {
            if (x < demo_bldgs[k].xmin || demo_bldgs[k].xmax < x)
                continue;
            if (y < demo_bldgs[k].yroof)
                continue;
            occl = 1;
        }
    } while (occl);

    if (demo_star_exists((uint_fast16_t)x, (uint_fast16_t)y))
        return;

    uint32_t color = demo_star_colors[rand() % (sizeof(demo_star_colors) / sizeof(demo_star_colors[0]))];

    if (verify_star_inputs((uint16_t)x, (uint16_t)y, "demo_scene: add_star() received invalid coordinates") < 0)
        return;

    struct skyline_star *star = demo_stars + demo_star_cnt++;

    star->x = (uint16_t)x;
    star->y = (uint16_t)y;
    star->color = color;

    if (add_cb)
        add_cb(star->x, star->y, star->color);
}

void demo_remove_rand_star(demo_remove_star_fn remove_cb) {
    if (demo_star_cnt == 0)
        return;

    struct skyline_star *star = demo_stars + (rand() % demo_star_cnt);
    if (remove_cb) {
        if (verify_star_inputs(star->x, star->y, "demo_scene: remove_star() received invalid coordinates") < 0)
            return;
        remove_cb(star->x, star->y);
    }

    demo_star_cnt -= 1;
    struct skyline_star *last = demo_stars + demo_star_cnt;
    if (star != last)
        memcpy(star, last, sizeof(struct skyline_star));
}

void demo_create_initial_stars(demo_add_star_fn add_cb) {
    int target = MIN(DEMO_STAR_STEADY_TARGET, DEMO_STARS_MAX);
    int attempts = 0;

    while (demo_star_cnt < target && attempts < target * 4) {
        demo_add_rand_star(add_cb);
        attempts++;
    }
}

void demo_create_bldgs(void) {
    struct demo_building *const bldgs = demo_bldgs;
    uint_fast8_t tidx = 0;

    for (int k = 0; k < DEMO_BLDG_CNT; k++) {
        bldgs[k].bfcnt = rand1(DEMO_BFCNT_MIN, DEMO_BFCNT_MAX);
        bldgs[k].fwcnt = rand2(DEMO_FWCNT_MIN, DEMO_FWCNT_MAX);
        bldgs[k].wspec = win_specs + rand() % (sizeof(win_specs) / sizeof(win_specs[0]));

        bldgs[k].h = bldgs[k].wspec->fht * bldgs[k].bfcnt + 4;
        bldgs[k].w = bldgs[k].wspec->hwp * bldgs[k].fwcnt + 4;
        bldgs[k].yroof = SKYLINE_HEIGHT - 1 - bldgs[k].h;
        bldgs[k].xmin = (int)k * SKYLINE_WIDTH / DEMO_BLDG_CNT + rand2(0, SKYLINE_WIDTH / DEMO_BLDG_CNT);
        if (bldgs[k].xmin < 0)
            bldgs[k].xmin = 0;

        bldgs[k].xmax = bldgs[k].xmin + bldgs[k].w;
        if (SKYLINE_WIDTH <= bldgs[k].xmax) {
            bldgs[k].xmax = SKYLINE_WIDTH - rand1(1, SKYLINE_WIDTH / DEMO_BLDG_CNT / 2);
            bldgs[k].xmin = bldgs[k].xmax - bldgs[k].w;
        }

        assert(0 < bldgs[k].yroof);
        assert(bldgs[k].yroof < SKYLINE_HEIGHT);
        assert(0 <= bldgs[k].xmin);
        assert(bldgs[k].xmin < bldgs[k].xmax);
        assert(bldgs[k].xmax < SKYLINE_WIDTH);

        if (bldgs[k].h > bldgs[tidx].h)
            tidx = (uint_fast8_t)k;
    }

    demo_win_cnt = 0;

    for (int k = 0; k < DEMO_BLDG_CNT; k++) {
        const int w = bldgs[k].wspec->w;
        const int h = bldgs[k].wspec->h;
        const int hwp = bldgs[k].wspec->hwp;
        const int fht = bldgs[k].wspec->fht;

        int xoff = bldgs[k].w - hwp * bldgs[k].fwcnt + (hwp - w);
        xoff /= 2;
        int yoff = 2 + (fht - h);
        yoff /= 2;

        assert(0 <= xoff);
        assert(xoff < bldgs[k].w);
        assert(0 <= yoff);
        assert(yoff < bldgs[k].h);

        size_t palette_cnt = demo_building_color_cnt ? demo_building_color_cnt : 1;
        uint_fast8_t cidx = (uint_fast8_t)(rand() % palette_cnt);

        for (int i = 0; i < bldgs[k].bfcnt; i++) {
            int ymin = bldgs[k].yroof + yoff + i * fht;
            int ymax = ymin + h - 1;

            assert(ymin < ymax);
            assert(0 <= ymin);
            assert(ymax < SKYLINE_HEIGHT);

            for (int j = 0; j < bldgs[k].fwcnt; j++) {
                int xmin = bldgs[k].xmin + xoff + j * hwp;
                int xmax = xmin + w - 1;

                assert(xmin < xmax);
                assert(0 <= xmin);
                assert(xmax < SKYLINE_WIDTH);

                int occl = 0;
                for (int kk = 0; kk < k; kk++) {
                    if (bldgs[kk].xmax <= xmin || xmax <= bldgs[kk].xmin)
                        continue;
                    if (ymax <= bldgs[kk].yroof)
                        continue;
                    occl = 1;
                }

                if (!occl) {
                    struct demo_window *win = demo_windows + demo_win_cnt++;
                    win->color = demo_building_colors[cidx];
                    win->x = (uint16_t)xmin;
                    win->y = (uint16_t)ymin;
                    win->w = (uint8_t)w;
                    win->h = (uint8_t)h;
                    demo_win_state[win - demo_windows] = 0;
                }
            }
        }
    }

    demo_beacon_xpos = (bldgs[tidx].xmin + bldgs[tidx].xmax) / 2;
    demo_beacon_ypos = bldgs[tidx].yroof - 16;
}

void demo_set_building_palette(const uint32_t *colors, size_t count) {
    if (colors != NULL && count > 0) {
        demo_building_colors = colors;
        demo_building_color_cnt = count;
    } else {
        demo_building_colors = default_building_colors;
        demo_building_color_cnt = sizeof(default_building_colors) / sizeof(default_building_colors[0]);
    }
}

void demo_seed_initial_windows(demo_add_window_fn add_cb, demo_remove_window_fn remove_cb) {
    if (demo_win_cnt == 0 || add_cb == NULL)
        return;

    int target = (demo_win_cnt * DEMO_INIT_LIT_WIN_PER256 + 255) / 256;
    if (target <= 0)
        return;

    int lit = 0;
    while (lit < target) {
        uint32_t idx = (uint32_t)(rand() % demo_win_cnt);
        if (demo_win_state[idx])
            continue;
        demo_set_window_state(idx, 1, add_cb, remove_cb);
        lit++;
    }
}

void demo_update_windows(demo_add_window_fn add_cb, demo_remove_window_fn remove_cb) {
    if (demo_win_cnt == 0 || add_cb == NULL || remove_cb == NULL)
        return;

    int desired = (demo_win_cnt * DEMO_WIN_STEADY_TARGET_PER256 + 255) / 256;
    int lower = desired - DEMO_WIN_STEADY_BAND;
    int upper = desired + DEMO_WIN_STEADY_BAND;

    if (lower < 0)
        lower = 0;
    if (upper > (int)demo_win_cnt)
        upper = (int)demo_win_cnt;

    int actions = DEMO_WIN_UPDATES_PER_TICK;
    while (actions-- > 0) {
        if ((int)demo_win_lit_cnt < lower) {
            int idx = demo_pick_window_with_state(0);
            if (idx < 0)
                break;
            demo_set_window_state((uint32_t)idx, 1, add_cb, remove_cb);
        } else if ((int)demo_win_lit_cnt > upper) {
            int idx = demo_pick_window_with_state(1);
            if (idx < 0)
                break;
            demo_set_window_state((uint32_t)idx, 0, add_cb, remove_cb);
        } else {
            int desired_state = rand() & 1;
            int idx = demo_pick_window_with_state(desired_state);
            if (idx < 0)
                break;
            demo_set_window_state((uint32_t)idx, !desired_state, add_cb, remove_cb);
        }
    }
}

void demo_create_beacon(demo_start_beacon_fn start_cb) {
    static const uint32_t beacon_img[4 * 4] = {
        COL32(0x5f, 0, 0), COL32(0xE1, 0, 0), COL32(0xE1, 0, 0), COL32(0x5f, 0, 0),
        COL32(0xE1, 0, 0), COL32(0xFF, 0, 0), COL32(0xFF, 0, 0), COL32(0xE1, 0, 0),
        COL32(0xE1, 0, 0), COL32(0xFF, 0, 0), COL32(0xFF, 0, 0), COL32(0xE1, 0, 0),
        COL32(0x5f, 0, 0), COL32(0xE1, 0, 0), COL32(0xE1, 0, 0), COL32(0x5f, 0, 0),
    };

    uint_fast16_t period;
    if (DEMO_BEACON_FLASHES_PER_MIN <= 0)
        period = SKYLINE_FREQ * 3;
    else {
        period = (SKYLINE_FREQ * 60 + DEMO_BEACON_FLASHES_PER_MIN / 2) / DEMO_BEACON_FLASHES_PER_MIN;
        if (period == 0)
            period = 1;
    }
    uint_fast16_t ontime = period * 2 / 3;

    if (start_cb) {
        if (verify_beacon_inputs(demo_beacon_xpos, demo_beacon_ypos, 4, period, ontime, "demo_scene: start_beacon() received invalid parameters") < 0)
            return;
        start_cb(beacon_img, demo_beacon_xpos, demo_beacon_ypos, 4, period, ontime);
    }
    demo_beacon_started = 1;
}

// INTERNAL HELPERS ---------------------------------------------------------

static void demo_set_window_state(uint32_t idx, int on, demo_add_window_fn add_cb, demo_remove_window_fn remove_cb) {
    if (idx >= demo_win_cnt)
        return;

    if ((demo_win_state[idx] != 0) == (on != 0))
        return;

    struct demo_window *win = demo_windows + idx;

    if (on) {
        if (verify_window_inputs(win->x, win->y, win->w, win->h, "demo_scene: add_window() received invalid rectangle") < 0)
            return;
        if (add_cb)
            add_cb(win->x, win->y, win->w, win->h, win->color);
        demo_win_state[idx] = 1;
        demo_win_lit_cnt++;
    } else {
        if (verify_star_inputs(win->x, win->y, "demo_scene: remove_window() received invalid origin") < 0)
            return;
        if (remove_cb)
            remove_cb(win->x, win->y);
        demo_win_state[idx] = 0;
        if (demo_win_lit_cnt > 0)
            demo_win_lit_cnt--;
    }
}

static int demo_pick_window_with_state(int state) {
    if (demo_win_cnt == 0)
        return -1;

    int normalized = state ? 1 : 0;
    for (int tries = 0; tries < 64; tries++) {
        uint32_t idx = (uint32_t)(rand() % demo_win_cnt);
        if ((demo_win_state[idx] != 0) == normalized)
            return (int)idx;
    }

    for (uint32_t idx = 0; idx < demo_win_cnt; idx++)
        if ((demo_win_state[idx] != 0) == normalized)
            return (int)idx;

    return -1;
}

// VALIDATION HELPERS ------------------------------------------------------
static int demo_validation_fail(const char *ctx) {
    (void)ctx;
    return -EINVAL;
}

static int verify_star_inputs(uint16_t x, uint16_t y, const char *ctx) {
    if (x >= SKYLINE_WIDTH || y >= SKYLINE_HEIGHT)
        return demo_validation_fail(ctx);
    return 0;
}

static int verify_star_struct(const struct skyline_star *star, const char *ctx) {
    if (star == NULL)
        return demo_validation_fail(ctx);
    return verify_star_inputs(star->x, star->y, ctx);
}

static int verify_window_inputs(uint16_t x, uint16_t y, uint8_t w, uint8_t h, const char *ctx) {
    if (w == 0 || h == 0)
        return demo_validation_fail(ctx);
    if (x >= SKYLINE_WIDTH || y >= SKYLINE_HEIGHT)
        return demo_validation_fail(ctx);
    if ((uint32_t)x + w > SKYLINE_WIDTH || (uint32_t)y + h > SKYLINE_HEIGHT)
        return demo_validation_fail(ctx);
    return 0;
}

static int verify_window_struct(const struct skyline_window *win, const char *ctx) {
    if (win == NULL)
        return demo_validation_fail(ctx);
    return verify_window_inputs(win->x, win->y, win->w, win->h, ctx);
}

static int verify_beacon_inputs(uint16_t x, uint16_t y, uint8_t dia, uint16_t period, uint16_t ontime, const char *ctx) {
    if (dia == 0 || period == 0)
        return demo_validation_fail(ctx);
    if (ontime > period)
        return demo_validation_fail(ctx);
    if (x >= SKYLINE_WIDTH || y >= SKYLINE_HEIGHT)
        return demo_validation_fail(ctx);
    if ((uint32_t)x + dia > SKYLINE_WIDTH || (uint32_t)y + dia > SKYLINE_HEIGHT)
        return demo_validation_fail(ctx);
    return 0;
}

static int verify_beacon_struct(const struct skyline_beacon *bcn, const char *ctx) {
    if (bcn == NULL || bcn->img == NULL)
        return demo_validation_fail(ctx);
    return verify_beacon_inputs(bcn->x, bcn->y, bcn->dia, bcn->period, bcn->ontime, ctx);
}

// RENDERING ----------------------------------------------------------------

void demo_render_scene(
    uint32_t *fbuf,
    uint64_t tickcnt,
    uint32_t sky_color,
    demo_fillscreen_fn fillscreen_cb,
    demo_draw_star_fn draw_star_cb,
    demo_draw_window_fn draw_window_cb,
    demo_draw_beacon_fn draw_beacon_cb)
{
    if (fillscreen_cb)
        fillscreen_cb(fbuf, sky_color);

    int starcnt = 0;
    for (struct skyline_star *star = skyline_star_list; star != NULL; star = star->next) {
        if (++starcnt > SKYLINE_WIDTH * SKYLINE_HEIGHT)
            panic("Corrupt window linked list");
        if (verify_star_struct(star, "demo_scene: draw_star() received invalid star") < 0)
            continue;
        if (draw_star_cb)
            draw_star_cb(fbuf, star);
    }

    if (DEMO_WIN_MAX < skyline_win_cnt)
        panic("Invalid skyline_win_cnt");

    if (draw_window_cb)
        for (uint16_t i = 0; i < skyline_win_cnt; i++) {
            const struct skyline_window *win = skyline_windows + i;
            if (verify_window_struct(win, "demo_scene: draw_window() received invalid window") < 0)
                continue;
            draw_window_cb(fbuf, win);
        }

    if (demo_beacon_started && draw_beacon_cb) {
        if (verify_beacon_struct(&skyline_beacon, "demo_scene: draw_beacon() received invalid beacon") >= 0)
            draw_beacon_cb(fbuf, tickcnt, &skyline_beacon);
    }
}
