#include <stddef.h>
#include <stdint.h>

extern void * memset(void * s, int c, size_t n); // string.c

#define CH0 '.'
#define CH1 '@'
#define NCOL 80

static void init(char * prev, char * next);
static void step(const char * prev, char * next);
static void draw(const char * line);
static void wait(void);

static void rule30_main(void) {
    static char buf[2][NCOL+2];
    char * prev = buf[0];
    char * next = buf[1];
    char * tmp;

    init(prev, next);
    draw(prev);

    for (;;) {
        step(prev, next);
        draw(next);
        
        tmp = next;
        next = prev;
        prev = tmp;
        wait();
    }
}

void init(char * prev, char * next) {
    memset(prev, CH0, NCOL);
    memset(next, CH0, NCOL);
    prev[NCOL/2] = CH1;
    prev[NCOL+0] = '\r';
    prev[NCOL+1] = '\n';
    next[NCOL+0] = '\r';
    next[NCOL+1] = '\n';
}

void step(const char * prev, char * next) {
    static const char rule[] = {
        CH0, CH1, CH1, CH1, CH1, CH0, CH0, CH0
    };

    int idx;
    int i;

    for (i = 0; i < NCOL; i++) {
        idx = (prev[(i+1) % NCOL] != CH0);
        idx = (idx << 1) | (prev[i] != CH0);
        idx = (idx << 1) | (prev[(i-1) % NCOL] != CH0);
        next[i] = rule[idx];
    }
}

#if defined(AEE2)

struct serial; // external object
static struct serial * rule30_term;

// from device.c
extern int serial_recv(struct serial * ser, void * buf, unsigned int bufsz);
extern int serial_send(struct serial * ser, const void * buf, unsigned int len);

// from timer.c
extern void sleep_ms(unsigned long ms);

void draw(const char * line) {
    serial_send(rule30_term, line, NCOL+2);
}

void wait(void) {
    sleep_ms(100);
}

void rule30_start(struct serial * term) {
    rule30_term = term;
    rule30_main();
}

#endif