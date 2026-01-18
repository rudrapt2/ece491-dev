// after_dark.h
//

#ifndef _AFTER_DARK_H_
#define _AFTER_DARK_H_

#include <stdint.h>

// Maximum number of toasters
#define TOASTERS_MAX 50

// Width of the screen (total number of columns in the frame buffer)
#define SCREEN_WIDTH 640

// Height of the screen (total number of rows in the frame buffer)
#define SCREEN_HEIGHT 480

struct toaster {
    uint8_t phase;
    int16_t x;
    int16_t y;
    char reserved[10];
};

struct toast {
    int16_t x;
    int16_t y;
    struct toast* next;
};

// Array of toasters
extern struct toaster toasters[TOASTERS_MAX];

// Number of toasters currently in the array
extern uint16_t toaster_cnt;

// Linked list of toast
extern struct toast* toast_list;

// Starting address of toaster images
// Each toaster image is a 64*64 pixel image stored in row-major order, with each pixel consisting of 3 bytes (red, green, blue)
// All four toaster images are stored consecutively in memory
extern char toaster_start[];

// Starting address of toast image
// The toast image is a 64*64 pixel image stored in row-major order, with each pixel consiting of 3 bytes (red, green, blue)
extern char toast_start[];


extern void draw_sprite(const char* sprite_rgb24, uint32_t* fbuf_rgbx32, int16_t x, int16_t y);

extern void draw_toaster(struct toaster* t, uint32_t* fbuf_rgbx32);

extern void draw_toast(struct toast* t, uint32_t* fbuf_rgbx32);

extern void move_toaster(struct toaster* t);

extern void move_toast(struct toast* t);

extern void add_toaster(int16_t x, int16_t y, uint8_t phase);

extern void add_toast(int16_t x, int16_t y);

extern void remove_toaster(int16_t x, int16_t y);

extern void remove_toast(int16_t x, int16_t y);

#endif // _AFTER_DARK_H_