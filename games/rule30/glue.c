// glue.c - Glue for Rule 30 across environments

#include <stddef.h>
#include <stdarg.h>

#include "glue.h"

#if defined AEE0 + defined AEE1 + defined AEE2 + defined AEE31 + defined AEE32 > 1
#error "Exactly one of AEE0, AEE1, AEE2, AEE31, or AEE32 must be defined"
#endif

#if defined AEE0 + defined AEE1 + defined AEE2 + defined AEE31 + defined AEE32 < 1
#error "Exactly one of AEE0, AEE1, AEE2, AEE31, or AEE32 must be defined"
#endif

// rule30.c exposes these for glue to implement/drive
void rule30_main(void);
void draw(const char * line);
void wait(void);

#if defined(AEE0)

extern void kputc(char c);

void draw(const char * line) {
    // Write until and including the first '\n'
    const char *p = line;
    for (;;) {
        char c = *p++;
        kputc(c);
        if (c == '\n') break;
    }
}

void wait(void) {
    // Crude delay when no timer API is available
    volatile unsigned long i;
    for (i = 0; i < 100000; i++) { }
}

void rule30_start(void) {
    rule30_main();
}

#endif

#if defined(AEE2)

struct io; // external object
static struct io * rule30_io;

extern long iowrite(struct io * io, const void * buf, long len);

// from timer.c
extern void sleep_ms(unsigned long ms);

// Write until and including first '\n'
void draw(const char * line) {
    const char *p = line;
    for (;;) {
        const char c = *p++;
        if (iowrite(rule30_io, &c, 1) != 1)
            break;
        if (c == '\n') break;
    }
}

void wait(void) {
    sleep_ms(100);
}

void rule30_start(struct io * tio) {
    rule30_io = tio;
    rule30_main();
}

#endif

#if defined (AEE31)

struct uio;
extern void uio_write(struct uio *u, const void *buf, unsigned long len);

static struct uio *r30_uio;

void draw(const char * line) {
    // Determine length up to and including first '\n'
    unsigned long len = 0;
    while (line[len] != '\n') len++;
    len++;
    uio_write(r30_uio, line, len);
}

void wait(void) {
    volatile unsigned long i;
    for (i = 0; i < 100000; i++) { }
}

void rule30_start(struct uio *uio) {
    r30_uio = uio;
    rule30_main();
}

#endif

#if defined(AEE32)
#include "usr/syscall.h"

extern void putc(char c);

void draw(const char * line) {
    const char *p = line;
    for (;;) {
        char c = *p++;
        putc(c);
        if (c == '\r' || c == '\n') break;
    }
}

void wait(void) {
    _usleep(100000);
}

void main(void) {
    rule30_main();
}

#endif