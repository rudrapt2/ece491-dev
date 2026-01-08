#include "after_dark.h"
#include "../heap.h"

extern char toaster_start[];
extern char toast_start[];

extern struct toaster toasters[TOASTERS_MAX];
extern uint16_t toaster_cnt;

extern struct toast* toast_list;

// NOTE THAT YOU SHOULD NOT PROVIDE GOLD.C TO STUDENTS; RENAME GOLD_OBF.O TO GOLD.O AND GIVE THAT TO STUDENTS


void draw_sprite(const char* sprite_rgb24, uint32_t* fbuf_rgbx32, int16_t x, int16_t y) {
    if (sprite_rgb24 == NULL) {
        return;
    }
    if (fbuf_rgbx32 == NULL) {
        return;
    }
    const int16_t SPRITE_WIDTH = 64;
    const int16_t SPRITE_HEIGHT = 64;
    for (int16_t x_iter = 0; x_iter < SPRITE_WIDTH; x_iter++) {
        if (((x + x_iter) >= SCREEN_WIDTH) || ((x + x_iter) < 0)) {
            continue;
        }
        for (int16_t y_iter = 0; y_iter < SPRITE_HEIGHT; y_iter++) {
            if (((y + y_iter) >= SCREEN_HEIGHT) || ((y + y_iter) < 0)) {
                continue;
            }
            char r = sprite_rgb24[3*(y_iter * SPRITE_WIDTH + x_iter)];
            char g = sprite_rgb24[3*(y_iter * SPRITE_WIDTH + x_iter) + 1];
            char b = sprite_rgb24[3*(y_iter * SPRITE_WIDTH + x_iter) + 2];

            // if we find the magenta xor (0xFF00FF) we should not draw
            if ((r != (char)0xFF) || (g != (char)0x00) || (b != (char)0xFF)) {
                fbuf_rgbx32[(y + y_iter) * SCREEN_WIDTH + (x + x_iter)] = (r << 24) + (g << 16) + (b << 8);
            }
        }
    }
}

void draw_toaster(struct toaster* t, uint32_t* fbuf_rgbx32) {
    if (t == NULL) {
        return;
    }
    if (fbuf_rgbx32 == NULL) {
        return;
    }
    draw_sprite(toaster_start + t->phase * 64 * 64 * 3, fbuf_rgbx32, t->x, t->y);
}

void draw_toast(struct toast* t, uint32_t* fbuf_rgbx32) {
    if (t == NULL) {
        return;
    }
    if (fbuf_rgbx32 == NULL) {
        return;
    }
    draw_sprite(toast_start, fbuf_rgbx32, t->x, t->y);
}

void move_toaster(struct toaster* t) {
    if (t == NULL) {
        return;
    }
    t->x -= 1;
    t->y += 1;
    t->phase += 1;
    t->phase = t->phase % 4;
}

void move_toast(struct toast* t) {
    if (t == NULL) {
        return;
    }
    t->x -= 1;
    t->y += 1;
}

// create a toaster at position (x, y)
void add_toaster(int16_t x, int16_t y, uint8_t phase) {
    if (toaster_cnt == TOASTERS_MAX) {
        return;
    }
    toasters[toaster_cnt] = (struct toaster){.x = x, .y = y, .phase = phase};
    toaster_cnt++;
}

void add_toast(int16_t x, int16_t y) {
    struct toast* tmp = toast_list;
    toast_list = malloc(sizeof(struct toast));
    toast_list->x = x;
    toast_list->y = y;
    toast_list->next = tmp;
}

void remove_toaster(int16_t x, int16_t y) {
    int index = 0;
    while (index < toaster_cnt) {
        struct toaster cur = toasters[index];
        if (cur.x == x && cur.y == y) {
            toasters[index] = toasters[toaster_cnt - 1];
            toaster_cnt -= 1;
        }
        index++;
    }
}

void remove_toast(int16_t x, int16_t y) {
    struct toast* t = toast_list;

    // if toast_list is empty return
    if (t == NULL) {
        return;
    }

    // if we find the correct toast at the head of the list
    if (t->x == x && t->y == y) {
        toast_list = t->next;
        free(t);
        return;
    }

    // while the next pointer element exists and the next element is not the correct toast
    while(t->next != NULL && (t->next->x != x || t->next->y != y)) {
        t = t->next;
    }
    if (t->next->x == x && t->next->y == y) {
        free(t->next);
        t->next = t->next->next;
        return;
    }
}