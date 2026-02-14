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

#define CTLSYNC     0x00
#define CTLSYNCREQ  0x40
#define CTLFLOW     0xC0

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
    char rxbusy;
    char txbusy;
    signed char state;
    unsigned char remcnt;
    unsigned short unsync_bytecnt;
    short err; // negative or 1=EOF
    struct condition rxupdt;
    struct condition txupdt;
    struct rbuf rxbuf;
    struct iomux4_chan ch[4];
};

void iomux4_init(struct iomux4 * mux, struct io * bkgio) {
    int i;

    memset(mux, 0, sizeof(*mux));
    mux->bkgio = bkgio;

    condition_init(&mux->rxupdt, "iomux4.rxupdt");
    condition_init(&mux->txupdt, "iomux4.txupdt");
    rbuf_init(&mux->rxbuf, /* buf */ NULL, /* capacity */ 64);

    for (i = 0; i < 4; i++) {
        ioinit(&mux->ch[i].io, /* iointf */ NULL, ioblksz(bkgio), 0);
        rbuf_init(&mux->ch[i].rxbuf, /* buf */ NULL, /* capacity */ 0);
    }
}

long iomux4_chan_read(struct iomux4 * mux, int chno, void * buf, long bufsz) {
    struct iomux4_chan * const ch = mux->ch+chno;
    int result;

    if (mux->state == IOM4_UNSYNC)
        iomux4_synchronize(mux);

    while (rbuf_empty(&ch->rxbuf) && mux->state != IOM4_ERR) {
        if (mux->rxbusy)
            condition_wait(&mux->rxupdt);
        else
            iomux4_recv_frame(mux);
    }

    if (mux->state == IOM4_ERR)
        return mux->err;

    return rbuf_read(&ch->rxbuf, buf, bufsz);
}

void iomux4_synchronize(struct iomux4 * mux) {
    char buf[72];
    unsigned int bufpos;
    long result;

    while (mux->state == IOM4_UNSYNC && (mux->txbusy || mux->rxbusy))
        condition_wait(&mux->txupdt);
    
    if (mux->state != IOM4_UNSYNC)
        return;
    
    rbuf_reset(&mux->rxbuf);

    memset(buf, CTLSYNC, sizeof(buf)-1);
    buf[sizeof(buf)-1] = CTLSYNCREQ;
    bufpos = 0;

    mux->txbusy = 1;
    mux->rxbusy = 1;
    
    while (bufpos != sizeof(buf)) {
        result = iowrite(mux->bkgio, buf + bufpos, sizeof(buf) - bufpos);

        if (result < 0)
            goto iomux4_synchronize_error;
        
        bufpos -= result;
    }



iomux4_synchronize_error:
    mux->err = result;
    mux->state = IOM4_ERR;
iomux4_synchronize_done:
    mux->txbusy = 0;
    mux->rxbusy = 0;

}

void iomux4_recv_frame(struct iomux4 * mux) {
    struct rbuf * const rxbuf = &mux->rxbuf;
    int chno, blklen, cntl;

    if (mux->state == IOM4_ERR || mux->rxbusy)
        return;

    mux->rxbusy = 1;

    if (rbuf_empty(rxbuf)) {
        iomux4_recv_bytes(mux);

        if (mux->state == IOM4_ERR)
            goto iomux4_recv_frame_done;
    }

    cntl = rbuf_getc(rxbuf);
    chno = cntl >> 6;
    cntl &= 0x3f;

    if (blklen != 0) {
        struct rbuf * const chrxbuf = &mux->ch[chno].rxbuf;
        unsigned int chrxlen;

        while (rbuf_readable(rxbuf) < blklen && mux->state != IOM4_ERR)
            iomux4_recv_bytes(mux);
        
        chrxlen = rbuf_move(chrxbuf, rxbuf, blklen);
        rbuf_consumed(chrxbuf, blklen - chrxlen);

    } else {
        switch (cntl) {
        case 0x00:
            break;
        case 0x40:
            // sync request
        case 0x80:
            // reserved
        case 0xC0:
            // flow control
        }
    }

iomux4_recv_frame_done:
    mux->rxbusy = 0;
    return;
}

void iomux4_recv_bytes(struct iomux4 * mux) {
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