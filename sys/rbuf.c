// rbuf.c - Ring buffer
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "rbuf.h"
#include "string.h"
#include "misc.h"

void rbuf_init(struct rbuf * rb, void * buf, unsigned int capacity) {
    assert (capacity != 0 && ISPOW2(capacity));
    assert (rb != NULL);
    assert (buf != NULL);

    memset(rb, 0, sizeof(*rb));
    rb->mask = capacity - 1;
    rb->buf = buf;
}

void rbuf_reset(struct rbuf * rb) {
    rb->wpos = 0;
    rb->rpos = 0;
}

unsigned int rbuf_capacity(const struct rbuf * rb) {
    return rb->mask + 1;
}

void * rbuf_bufmem(const struct rbuf * rb) {
    return rb->buf;
}

unsigned int rbuf_readable(const struct rbuf * rb) {
    return rb->wpos - rb->rpos;
}

unsigned int rbuf_writable(const struct rbuf * rb) {
    return rbuf_capacity(rb) - rbuf_readable(rb);
}

int rbuf_empty(const struct rbuf * rb) {
    return (rb->rpos == rb->wpos);
}

int rbuf_full(const struct rbuf * rb) {
    return (rbuf_readable(rb) == rbuf_capacity(rb));
}

int rbuf_getc(struct rbuf * rb) {
    if (!rbuf_empty(rb))
        return (unsigned char)rb->buf[rb->rpos++ & rb->mask];
    else
        return -1;
}

int rbuf_peek(struct rbuf * rb) {
    if (!rbuf_empty(rb))
        return (unsigned char)rb->buf[rb->rpos & rb->mask];
    else
        return -1;
}

void rbuf_putc(struct rbuf * rb, char c) {
    if (!rbuf_full(rb))
        rb->buf[rb->wpos++ & rb->mask] = c;
}

unsigned int rbuf_getb(struct rbuf * rb, void * buf, unsigned int bufsz) {
    unsigned int const ridx = rb->rpos & rb->mask;
    unsigned int const cap = rb->mask + 1;
    unsigned int len, m;

    len = MIN(rb->wpos - rb->rpos, bufsz);

    if (len == 0)
        return 0;
    
    m = cap - ridx;

    if (m < len) {
        memcpy(buf, rb->buf + ridx, m);
        memcpy(buf+m, rb->buf, m-len);
    } else
        memcpy(buf, rb->buf + ridx, len);
    
    rb->rpos += len;
    return len;
}

unsigned int rbuf_putb (
    struct rbuf * rb, const void * buf, unsigned int buflen)
{
    unsigned int const widx = rb->wpos & rb->mask;
    unsigned int const cap = rb->mask + 1;
    unsigned int len, m;

    len = MIN(cap - (rb->wpos - rb->rpos), buflen);

    if (len == 0)
        return 0;
    
    m = cap - widx;

    if (m < len) {
        memcpy(rb->buf + widx, buf, m);
        memcpy(rb->buf, buf+m, m-len);
    } else
        memcpy(rb->buf + widx, buf, len);
    
    rb->wpos += len;
    return len;
}

const void * rbuf_rptr(struct rbuf * rb, unsigned int * lenptr) {
    unsigned int const ridx = rb->rpos & rb->mask;
    unsigned int const widx = rb->wpos & rb->mask;
    unsigned int const cap = rb->mask + 1;

    assert (rb != NULL && lenptr != NULL);

    *lenptr = ((widx < ridx) ? cap : widx) - ridx;
    return rb->buf + ridx;
}

void rbuf_consumed(struct rbuf * rb, unsigned int n) {
    rb->rpos += n;
}

void * rbuf_wptr(struct rbuf * rb, unsigned int * lenptr) {
    unsigned int const ridx = rb->rpos & rb->mask;
    unsigned int const widx = rb->wpos & rb->mask;
    unsigned int const cap = rb->mask + 1;
    
    assert (rb != NULL && lenptr != NULL);

    *lenptr = ((ridx < widx) ? cap : ridx) - widx;
    return rb->buf + widx;
}

void rbuf_produced(struct rbuf * rb, unsigned int n) {
    rb->wpos += n;
}

unsigned int rbuf_move(struct rbuf * dst, struct rbuf * src, unsigned int n) {
    unsigned int m = 0;

    // TODO Rewrite this to not use getc/putc

    while (n != 0 && !rbuf_full(dst) && !rbuf_empty(src)) {
        rbuf_putc(dst, rbuf_getc(src));
        m += 1;
        n -= 1;
    }

    return m;
}