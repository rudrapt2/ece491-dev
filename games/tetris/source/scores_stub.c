#include <stdio.h>
#include <time.h>
#include "scores.h"

#ifdef AEE32
#include "usr/syscall.h"

#define HS_PATH "/c/tetris.dat"

struct hs_record {
    int best_total;
    int best_level;
    int best_score;
};

static struct hs_record cached = {0, 0, 0};
static int loaded;

extern int score;

static void load_highscore(void) {
    if (loaded)
        return;
    loaded = 1;

    int fd = _open(-1, HS_PATH);
    if (fd < 0)
        return;

    struct hs_record tmp;
    long n = _read(fd, &tmp, sizeof(tmp));
    _close(fd);
    if (n == (long)sizeof(tmp) && tmp.best_total >= 0)
        cached = tmp;
}

static void store_highscore(void) {
    _delete(HS_PATH);
    _create(HS_PATH);

    int fd = _open(-1, HS_PATH);
    if (fd < 0)
        return;
    (void)_write(fd, &cached, sizeof(cached));
    _close(fd);
}

int current_highscore_total(void) {
    load_highscore();
    return cached.best_total;
}
#endif

void savescore(int level) {
#ifdef AEE32
    load_highscore();
    int total = score * level;
    if (total > cached.best_total) {
        cached.best_total = total;
        cached.best_level = level;
        cached.best_score = score;
        store_highscore();
    }
#else
    (void)level;
#endif
}

void showscores(int level) {
#ifdef AEE32
    (void)level;
    load_highscore();
    if (cached.best_total <= 0) {
        printf("No high score yet.\n");
        return;
    }
    printf("Best score: %d points x level %d = %d\n",
           cached.best_score, cached.best_level, cached.best_total);
#else
    (void)level;
    printf("High scores are disabled in this port.\n");
#endif
}
