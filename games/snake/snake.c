#include "usr/syscall.h"
#include "usr/string.h"
#include "usr/viohi.h"
#include "usr/error.h"
#include "usr/heap.h"
#include "usr/io.h"

#include <stdint.h>

#define HZ 60
#define US_PER_S 1000000

#define BG_COLOR 0x00080808
#define P_COLOR 0x0000F800
#define F_COLOR 0x00F80808

struct body {
    int x;
    int y;
    struct body* next;
    struct body* prev;
};

enum dir {
    IDLE,
    UP,
    LEFT,
    DOWN,
    RIGHT
};

void * USER_FBUF;
int gpu_fd;

void check_error(int err, char * msg) {
    if (err >= 0) return;
    printf("%s (%s)\n", msg, error_desc(err));
    _exit();
}

void clear_screen(uint32_t color) {
    uint32_t* p = USER_FBUF;
    for (int i=0; i<640*480; i++) 
        p[i] = color;
}

void draw_cell(int x, int y, uint32_t color) {
    uint32_t* p = USER_FBUF;
    for (int yd = 16*y+1; yd < 16*(y+1)-1; yd++) {
        for (int xd = 16*x+1; xd < 16*(x+1)-1; xd++) {
            p[xd + 640*yd] = color;
        }
    }
}

void cleanup(int score) {
    clear_screen(0x00);
    _write(gpu_fd, NULL, 0);
    printf("Congrats, you got a score of %d\r\n", score);
    _exit();
}

uint32_t prng(uint32_t* X) {
    *X = (1103515245*(*X) + 12345) % (1<<31);
    return *X;
}

uint32_t generate_food(struct body* head, uint32_t* seed) {
    uint32_t food = prng(seed) % (30*40);
    int fx = food % 40;
    int fy = food / 40;
    struct body* check = head;
    while (check != NULL) {
        if (check->x == fx && check->y == fy) return generate_food(head, seed);
        check = check->next;
    }
    return food;
}

int check_collision(struct body * h) {
    struct body * n = h->next;
    while (n != NULL) {
        if (n->x == h->x && n->y==h->y) 
            return 1;
        n = n->next;
    }
    return 0;
}

void sleep_thread(int wpipe_fd) {
    uint32_t key = VKEY_RESERVED;
    while (_write(wpipe_fd, &key, sizeof(key)) == sizeof(key))
        _usleep(US_PER_S / HZ);
    _exit();
}

void main(void) {
    int x,y;
    uint32_t key;
    int score = 1;

    enum dir curdir = IDLE;
    struct body* head = malloc(sizeof(struct body));
    head->x = 20;
    head->y = 15;
    head->next = NULL;
    head->prev = NULL;
    struct body* tail = head;
    
    int ticks = 0;

    uint32_t seed;
    int rng_fd = _open(-1, "/dev/viorng0");
    check_error(rng_fd, "Failed to open rng device");
    check_error(_read(rng_fd, &seed, sizeof(seed)), 
        "Failed to read rng device");
    _close(rng_fd);

    int rpipe_fd = -1;
    int wpipe_fd = -1;

    check_error(_pipe(&wpipe_fd, &rpipe_fd), "Failed to pipe");
    int tid = _fork();
    check_error(tid, "Failed to fork");
    if (tid) {
        int hi_fd;
        struct viohi_event evt;
        int stid = _fork();
        check_error(stid, "Failed to fork");
        _close(rpipe_fd);

        if (stid == 0) sleep_thread(wpipe_fd); // no return

        hi_fd = _open(-1, "/dev/viohi1");
        if (hi_fd < 0) {
            key = VKEY_Q;
            _write(wpipe_fd, &key, sizeof(key));
            check_error(hi_fd, "Failed to open input device");
        }

        do {
            check_error(_read(hi_fd, &evt, sizeof(evt)), 
                "Failed to read from input device");

            if (evt.type != EV_KEY) continue;
            if (evt.value) continue; // release

            key = evt.code;
            if (_write(wpipe_fd, &key, sizeof(key)) < 0)
                _exit();
        } while (key != VKEY_Q);

        _wait(0);
        _wait(0);
        _exit();
    }

    int speed = 15; // lower is faster
    gpu_fd = _open(-1, "/dev/viogpu0");
    check_error(gpu_fd, "Failed to open gpu device");
    check_error(_ioctl(gpu_fd, IOC_MAPBUF, &USER_FBUF), "Failed to map fbuf");

    printf("wasd to move, q to quit\r\n");
    clear_screen(BG_COLOR);
    draw_cell(head->x, head->y, P_COLOR);

    uint32_t foodpos = generate_food(head, &seed);
    struct body food = {.x=foodpos%40, .y=foodpos/40};
    draw_cell(food.x, food.y, F_COLOR);

    uint32_t v = VKEY_RESERVED;

    for (;;) {
        do {
            key = v;
            _read(rpipe_fd, &v, sizeof(v));
        } while (v != VKEY_RESERVED);
        switch (key) {
        case VKEY_W: 
        case VKEY_UP:
            curdir=UP; 
            break;
        case VKEY_S:
        case VKEY_DOWN: 
            curdir=DOWN; 
            break;
        case VKEY_A:
        case VKEY_LEFT: 
            curdir=LEFT; 
            break;
        case VKEY_D:
        case VKEY_RIGHT: 
            curdir=RIGHT; 
            break;
        case VKEY_Q: 
            cleanup(score); 
            break;
        default: 
            break;
        }
        if (ticks++ % speed == 0) {
            x = head->x;
            y = head->y;
            switch (curdir) {
                case UP: 
                    y = (y == 0) ? 29 : y-1; 
                    break;
                case DOWN: 
                    y = (y+1)%30; 
                    break;
                case LEFT: 
                    x = (x == 0) ? 39 : x-1; 
                    break;
                case RIGHT: 
                    x = (x+1)%40; 
                    break;
                default: 
                    break;
            }

            if (x==food.x && y==food.y) {
                struct body* growth = malloc(sizeof(struct body));
                memcpy(growth, tail, sizeof(struct body));
                growth->prev = tail;
                tail->next = growth;
                tail = growth;

                foodpos = generate_food(head, &seed);
                food.x = foodpos%40;
                food.y = foodpos/40;
                draw_cell(food.x, food.y, F_COLOR);
                score++;
                if (speed > 2 && score % 2 == 0) speed--;
            }
            else draw_cell(tail->x, tail->y, BG_COLOR);

            tail->x = x;
            tail->y = y;
            tail->next = head;
            head->prev = tail;
            head = tail;

            if (tail->prev) tail = tail->prev;
            head->prev = NULL;
            tail->next = NULL;

            draw_cell(head->x, head->y, P_COLOR);

            if (curdir != IDLE && check_collision(head)) {
                cleanup(score);
            }
        }
        _write(gpu_fd, NULL, 0);
    }
}
