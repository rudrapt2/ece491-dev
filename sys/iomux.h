// iomux.h - I/O Multiplexing
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "rbuf.h"
#include "ioimpl.h"
#include "thread.h"
#include "misc.h"
#include "string.h"

struct iomux4_chan {
    struct io io;
    struct rbuf rxbuf;
};

enum iomux4_state {
    IOM4_UNSYNC = 0,
    IOM4_SYNC = 1,
    IOM4_ERR = -1
};

struct iomux4 {
    struct io * bkgio;
    unsigned char txen;
    unsigned char rxbusy;
    signed char state;
    unsigned char remcnt;
    unsigned short unsync_bytecnt;
    short err; // negative or 1=EOF
    struct rwlock txlock;
    struct condition rxupdt;
    struct rbuf rxbuf;
    struct iomux4_chan ch[4];
};

void iomux4_init(struct iomux4 * mux, struct io * bkgio) {
    int i;

    memset(mux, 0, sizeof(*mux));
    mux->bkgio = bkgio;

    rwlock_init(&mux->txlock, "iomux4.txlock");
    condition_init(&mux->rxupdt, "iomux4.rxupdt");
    rbuf_init(&mux->rxbuf, /* buf */ NULL, /* capacity */ 64);

    for (i = 0; i < 4; i++) {
        ioinit(&mux->ch[i].io, /* iointf */ NULL, ioblksz(bkgio), 0);
        rbuf_init(&mux->ch[i].rxbuf, /* buf */ NULL, /* capacity */ 0);
    }
}

long iomux4_chan_read(struct iomux4 * mux, int chno, void * buf, long bufsz) {
    struct iomux4_chan * const ch = mux->ch+chno;
    int result;

    while (rbuf_empty(&ch->rxbuf) && mux->state != IOM4_ERR)
        iomux4_rxframe(mux);

    if (mux->state == IOM4_ERR)
        return mux->err;

    return rbuf_read(&ch->rxbuf, buf, bufsz);
}

void iomux4_rxframe(struct iomux4 * mux) {
    struct rbuf * const rxbuf = &mux->rxbuf;
    int chno, len;
    long rlen;

    while (mux->state != IOM4_ERR && mux->rxbusy)
        condition_wait(&mux->rxupdt);
    
    if (mux->state == IOM4_ERR)
        return;

    assert (!mux->rxbusy);
    mux->rxbusy = 1;

    if (rbuf_empty(rxbuf))
        iomux4_rxbytes(mux);

    if (mux->state == IOM4_ERR)
        return;

    len = rbuf_getc(rxbuf);
    chno = len >> 6;
    len &= 0x3f;

    if (len != 0) {
        while (rbuf_readable(rxbuf) < len && mux->state != IOM4_ERR)
            iomux4_rxbytes(mux);
        
        // We now have a full frame in mux->rxbuf. Copy it to channel rxbuf.

        rbuf_move(&mux->ch[chno].rxbuf, rxbuf);
}

void iomux4_rxbytes(struct iomux4 * mux) {
    struct rbuf * const rxbuf = &mux->rxbuf;
    void * wptr;
    unsigned int wmax;
    int readlen;

    if (rbuf_full(rxbuf))
        return;
    
    if (rbuf_empty(rxbuf))
        rbuf_reset(rxbuf);
    
    wptr = rbuf_wptr(&mux->rxbuf, &wmax);
    readlen = ioread(mux->bkgio, wptr, wmax);

    if (readlen <= 0) {
        mux->err = readlen ? readlen : 1;
        mux->state = IOM4_ERR;
    } else
        rbuf_produced(rxbuf, readlen);
}

void iomux4_waitsync(struct iomux4 * mux) {
    // ...
}

void iomux4_sendsyncreq(struct iomux4 * mux) {
    // ...
}

void iomux4_rxblock(struct iomux4 * mux) {
    struct rbuf * const rxbuf = &mux->rxbuf;
    unsigned int mvcnt;
    void * buf;
    int blklen;
    int chno;
    int result;

    iomux4_synchronize(mux);
    
    if (rbuf_empty(rxbuf)) {
        result = iomux4_rxbytes(mux);

        if (result < 0) {
            mux->err = result;
            mux->state = IOM4_ERR;
            return;
        }
    }

    result = rbuf_getc(rxbuf);
    blklen = result & 0x3F;
    chno = result >> 6;

    while (rbuf_readable(rxbuf) < blklen) {
        result = iomux4_rxbytes(mux);

        if (result < 0) {
            mux->err = result;
            mux->state = IOM4_ERR;
            return;
        }
    }

    mvcnt = rbuf_move(&mux->ch[chno].rxbuf, rxbuf, blklen);
    rbuf_consumed(rxbuf, blklen - mvcnt);
    condition_broadcast(&mux->rxupd);

    if (rbuf_empty(rxbuf))
        rbuf_reset(rxbuf);
}


long iomux4_chan_write(struct iomux4 * mux, int chno, const void * buf, long buflen) {
    char blkbuf[64];
    long bufpos = 0;
    int blklen;
    long wlen;

    if (blklen == 0)
        return 0;
        
    iomux4_synchronize(mux);

    rwlock_acquire(&mux->txlock, /* exclusive */ 1);
    
    if (mux->state == IOM4_ERR)
        return mux->err;

    blklen = MIN(buflen, 63);
    blkbuf[0] = (chno << 6) | blklen;
    memcpy(blkbuf+1, buf+bufpos, blklen);
    
    wlen = iowrite(mux->bkgio, blkbuf, blklen+1);

    if (wlen <= 0)
        return wlen;
    
    if (wlen != blklen)
        panic("short write");
    
    return buflen;
}

void iomux4_sendsyncreq(struct mux