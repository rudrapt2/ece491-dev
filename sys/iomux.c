// iomux.c - I/O Multiplexing
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "rbuf.h"
#include "ioimpl.h"
#include "thread.h"
#include "misc.h"
#include "string.h"

// Control bytes

#define CTLSYNC     0x00 // sync sequence byte
#define CTLSYNCREQ  0x40 // synchronization request
#define CTLSKIP1    0x80 // skip next byte
#define CTLFLOW     0xC0 // flow control, next byte is XON mask

#define SYNCLEN       63 // minimum sync sequence length
#define SYNCRETRY   4096 // how many received bytes w/o sync to wait

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
    struct io * cio;
    signed char state;
    unsigned char txena;
    char rxbusy, txbusy;
    int err; // negative or 1=EOF
    struct condition rxup;
    struct condition txup;
    struct rbuf rxbuf;
    struct iomux4_chan ch[4];
};

// INTERNAL FUNCTION DECLARATIONS
//

static void iomux4_synchronize(struct iomux4 * mux);
static void iomux4_recv_frame(struct iomux4 * mux);
static void iomux4_recv_bytes(struct iomux4 * mux);

static void iomux4_send_syncseq(struct iomux4 * mux, int ctlbyte);

// Send a synchronization sequence followed by a control byte. To send a
// synchronization sequence only, set /ctlbyte/ to CTLSYNC. To send a
// synchronization sequence followed by a synchronization request control byte,
// set /ctlbyte/ to CTLSYNCREQ.


static long iomux4_chan_read (
    struct iomux4 * mux, int chno, void * buf, long bufsz);

static long iomux4_chan_write (
    struct iomux4 * mux, int chno, const void * buf, long buflen);

static int iomux4_chan_ioctl (
    struct iomux4 * mux, int chno, int op, void * arg);

// IOINTF FUNCTION DECLARATIONS
//

static long iomux4_ch0_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch0_write(struct io * io, const void * buf, long buflen);
static long iomux4_ch0_ioctl(struct io * io, int op, void * arg);

// IOINTF STRUCTURE DEFINITIONS
//

static const struct iointf iomux4_iointf[4] = {
    [0] = {
        .implname = "iomux4_ch0_iointf",
        .read = &iomux4_ch0_read,
        .write = &iomux4_ch0_write,
        .ioctl = &iomux4_ch0_ioctl
    },

    // TODO finish
};

// EXPORTED FUNCTION DEFINITIONS
//

void iomux4_init(struct iomux4 * mux, struct io * cio) {
    int i;

    memset(mux, 0, sizeof(*mux));
    mux->cio = cio;

    condition_init(&mux->rxup, "iomux4.rxupdt");
    condition_init(&mux->txup, "iomux4.txupdt");
    rbuf_init(&mux->rxbuf, /* buf */ NULL, /* capacity */ 64);

    for (i = 0; i < 4; i++) {
        ioinit(&mux->ch[i].io, /* iointf */ NULL, ioblksz(cio), 0);
        rbuf_init(&mux->ch[i].rxbuf, /* buf */ NULL, /* capacity */ 0);
    }
}

// IOINTF FUNCTION DEFINITIONS
//

long iomux4_ch0_read(struct io * io, void * buf, long bufsz) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    return iomux4_chan_read(mux, 0, buf, bufsz);
}

long iomux4_ch0_write(struct io * io, const void * buf, long buflen) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    return iomux4_chan_write(mux, 0, buf, buflen);
}

long iomux4_ch0_ioctl(struct io * io, int op, void * arg) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    return iomux4_chan_ioctl(mux, 0, op, arg);
}

// INTERNAL FUNCTION DEFINITIONS
//

long iomux4_chan_read(struct iomux4 * mux, int chno, void * buf, long bufsz) {
    struct iomux4_chan * const ch = mux->ch+chno;
    int result;

    if (mux->state == IOM4_UNSYNC)
        iomux4_synchronize(mux);

    while (rbuf_empty(&ch->rxbuf) && mux->state != IOM4_ERR) {
        if (mux->rxbusy)
            condition_wait(&mux->rxup);
        else
            iomux4_recv_frame(mux);
    }

    if (mux->state == IOM4_ERR)
        return mux->err;

    return rbuf_read(&ch->rxbuf, buf, bufsz);
}

long iomux4_chan_write(struct iomux4 * mux, int chno, const void * buf, long buflen) {
    char wbuf[64];
    long bufpos = 0;
    long wlen;
    int blklen;

    if (buflen == 0)
        return 0;
    
    if (mux->state == IOM4_UNSYNC)
        iomux4_synchronize(mux);
    
    if (mux->state == IOM4_ERR)
        return mux->err;

    mux->txbusy = 1;

    while (bufpos < buflen) {
        blklen = MIN(buflen, 63);
        wbuf[0] = (chno << 6) | blklen;
        memcpy(wbuf+1,buf+bufpos, blklen);

        wlen = iowrite(mux->cio, wbuf, 1+blklen);

        if (wlen <= 1+blklen) {
            mux->state = IOM4_ERR;
            mux->err = (wlen < 0) ? wlen : 0;
            mux->txbusy = 0;
            return mux->err;
        }

        bufpos -= blklen;
    }
    
    mux->txbusy = 0;
    return buflen;
}

void iomux4_synchronize(struct iomux4 * mux) {
    int nsync, nbytes;
    int c, i, result;

    while (mux->state == IOM4_UNSYNC && mux->rxbusy)
        condition_wait(&mux->rxup);
    
    mux->rxbusy = 1;

    // Reset al internal buffers and the carrier to clear stale data.

    rbuf_reset(&mux->rxbuf);
    for (i = 0; i < 4; i++)
        rbuf_reset(&mux->ch[i].rxbuf);
    ioctl(mux->cio, IOC_RESET, NULL);

    for (;;) {
        iomux4_send_syncseq(mux, CTLSYNCREQ);

        if (mux->state != IOM4_UNSYNC)
            goto iomux4_synchronize_done;

        // Wait to receive SYNCLEN-byte sync sequence. If we receive SYNCRETRY
        // bytes without seeing a sync sequence send request again.

        nsync = 0;
        nbytes = 0;

        while (nbytes < SYNCRETRY) {
            c = rbuf_getc(&mux->rxbuf);

            if (c < 0) {
                rbuf_recv_bytes(mux);
                if (mux->state != IOM4_UNSYNC)
                    goto iomux4_synchronize_done;
            } else {
                nbytes += 1;
                if (c == CTLSYNC && ++nsync == SYNCLEN)
                    goto iomux4_synchronize_done;
            }
        }
    }

iomux4_synchronize_done:
    mux->rxbusy = 0;
}

void iomux4_recv_frame(struct iomux4 * mux) {
    struct rbuf * const rxbuf = &mux->rxbuf;
    int chno, blklen;
    int cc = CTLSYNC; // prev control byte
    int c; // current control byte

    if (mux->state == IOM4_ERR || mux->rxbusy)
        return;

    mux->rxbusy = 1;

    // Read control byte. Handle special control bytes (CTLSYNC, CTLSYNCREQ,
    // CTLSKIP1, CLTFLOW) and repeat loop. Otherwise, break and parse control
    // byte as channel and length.

    for (;;) {
        c = rbuf_getc(rxbuf);

        if (c < 0) {
            iomux4_recv_bytes(mux);
            if (mux->state == IOM4_ERR)
                goto iomux4_recv_frame_done;
            continue;
        }

        if (cc == CTLFLOW) {
            iomux4_update_txena(mux, c);
        } else if (cc == CTLSKIP1) {
            // nothing
        } else if (c == CTLSYNCREQ)
            iomux4_send_syncseq(mux, CTLSYNC);
        else if (c != CTLSYNC && c != CTLSKIP1 && c != CTLFLOW)
            break;

        cc = c;
    }

    chno = c >> 6;
    blklen = c & 0x3f;
    assert (blklen != 0);

    while (blklen != 0) {
        struct rbuf * const chrxbuf = &mux->ch[chno].rxbuf;

        while (rbuf_readable(rxbuf) < blklen) {
            iomux4_recv_bytes(mux);
            if (mux->state == IOM4_ERR)
                goto iomux4_recv_frame_done;
        }
        
        blklen -= rbuf_move(chrxbuf, rxbuf, blklen);
    }

iomux4_recv_frame_done:
    mux->rxbusy = 0;
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
    readlen = ioread(mux->cio, wptr, wmax);

    if (readlen <= 0) {
        mux->err = readlen ? readlen : 1;
        mux->state = IOM4_ERR;
    } else
        rbuf_produced(rxbuf, readlen);
}

void iomux4_send_syncseq(struct iomux4 * mux, int ctlbyte) {
    char seqbuf[72];
    int sbpos = 0;
    int result;

    while (mux->state == IOM4_UNSYNC && mux->txbusy)
        condition_wait(&mux->txup);
    
    if (mux->state != IOM4_UNSYNC)
        return;
    
    mux->txbusy = 1;

    memset(seqbuf, CTLSYNC, sizeof(seqbuf)-1);
    seqbuf[sizeof(seqbuf)-1] = ctlbyte;
        
    while (sbpos != sizeof(seqbuf)) {
        result = iowrite(mux->cio, seqbuf + sbpos, sizeof(seqbuf) - sbpos);

        if (result < 0) {
            mux->err = result;
            mux->state = IOM4_ERR;
            break;
        }
        
        sbpos -= result;
    }

    mux->txbusy = 0;
}
