// rbuf.h - Ring buffer
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _RBUF_H_
#define _RBUF_H_

struct rbuf {
    unsigned int wpos, rpos;
    unsigned int mask;
    char * buf;
};

extern void rbuf_init(struct rbuf * rb, void * buf, unsigned int capacity);

extern void rbuf_reset(struct rbuf * rb);

extern unsigned int rbuf_capacity(const struct rbuf * rb);
extern void * rbuf_bufmem(const struct rbuf * rb);
extern unsigned int rbuf_readable(const struct rbuf * rb);
extern unsigned int rbuf_writable(const struct rbuf * rb);

extern int rbuf_empty(const struct rbuf * rb);
extern int rbuf_full(const struct rbuf * rb);
extern int rbuf_getc(struct rbuf * rb);
extern void rbuf_putc(struct rbuf * rb, char c);
extern int rbuf_peek(struct rbuf * rb);

extern unsigned int rbuf_getb (
    struct rbuf * rb, void * buf, unsigned int bufsz);

extern unsigned int rbuf_putb (
    struct rbuf * rb, const void * buf, unsigned int buflen);

extern const void * rbuf_rptr(struct rbuf * rb, unsigned int * lenptr);
extern void rbuf_consumed(struct rbuf * rb, unsigned int n);

extern void * rbuf_wptr(struct rbuf * rb, unsigned int * lenptr);
extern void rbuf_produced(struct rbuf * rb, unsigned int n);

extern unsigned int rbuf_move (
    struct rbuf * dst, struct rbuf * src, unsigned int n);

#endif // _RBUF_H_