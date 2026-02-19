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
#include "memory.h"
#include "error.h"
#include "device.h"

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
    const char * cdevname;
    struct io * cio;
    signed char state;
    unsigned char txstop;
    char rxbusy, txbusy;
    int err; // negative or 1=EOF
    struct condition rxup;
    struct condition txup;
    struct iomux4_chan ch[4];
    struct rbuf rxbuf;
    char rxbufmem[64];
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


static int iomux4_chan_open (
    struct io ** ioptr, struct iomux4 * mux, int chno);

static long iomux4_chan_read (
    struct iomux4 * mux, void * buf, long bufsz, int chno);

static long iomux4_chan_write (
    struct iomux4 * mux, const void * buf, long buflen, int chno);

static int iomux4_chan_ioctl (
    struct iomux4 * mux, int op, void * arg, int chno);

static int iomux4_chan_reclaim(struct iomux4 * mux, int chno);

// IOINTF FUNCTION DECLARATIONS
//

static int iomux4_ch0_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch0_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch0_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch0_ioctl(struct io * io, int op, void * arg);
static int iomux4_ch0_reclaim(struct io * io);

static int iomux4_ch1_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch1_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch1_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch1_ioctl(struct io * io, int op, void * arg);
static int iomux4_ch1_reclaim(struct io * io);

static int iomux4_ch2_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch2_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch2_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch2_ioctl(struct io * io, int op, void * arg);
static int iomux4_ch2_reclaim(struct io * io);

static int iomux4_ch3_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch3_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch3_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch3_ioctl(struct io * io, int op, void * arg);
static int iomux4_ch3_reclaim(struct io * io);


// IOINTF STRUCTURE DEFINITIONS
//

static const struct iointf iomux4_iointf[4] = {
    [0] = {
        .implname = "iomux4_ch0_iointf",
        .reclaim = &iomux4_ch0_reclaim,
        .read = &iomux4_ch0_read,
        .write = &iomux4_ch0_write,
        .ioctl = &iomux4_ch0_ioctl
    },

    [1] = {
        .implname = "iomux4_ch1_iointf",
        .reclaim = &iomux4_ch1_reclaim,
        .read = &iomux4_ch1_read,
        .write = &iomux4_ch1_write,
        .ioctl = &iomux4_ch1_ioctl
    },

    [2] = {
        .implname = "iomux4_ch2_iointf",
        .reclaim = &iomux4_ch2_reclaim,
        .read = &iomux4_ch2_read,
        .write = &iomux4_ch2_write,
        .ioctl = &iomux4_ch2_ioctl
    },

    [3] = {
        .implname = "iomux4_ch3_iointf",
        .reclaim = &iomux4_ch3_reclaim,
        .read = &iomux4_ch3_read,
        .write = &iomux4_ch3_write,
        .ioctl = &iomux4_ch3_ioctl
    }
};

static const device_openfn_t iomux4_openfn[4] = {
    (device_openfn_t)&iomux4_ch0_open,
    (device_openfn_t)&iomux4_ch1_open,
    (device_openfn_t)&iomux4_ch2_open,
    (device_openfn_t)&iomux4_ch3_open
};

// EXPORTED FUNCTION DEFINITIONS
//

int attach_iomux4(const char * cdevname) {
    struct iomux4 * mux;
    void * chrxbufmem;
    char * namebuf;
    size_t namelen;
    struct io * cio;
    struct io * chio[4];
    int result = 0;

    // Check if carrier device exists. We don't actually open it until one of
    // the channel devices is opened.

    if (device_exists(cdevname))
        return -ENOENT;
    
    // Allocate sapce for a temporary buffer for creating the channel device
    // names, which have the form DEVch0 ... DEVch3, where DEV is the carrier
    // device name.
    //
    // TODO We could use the space allocated for the receive buffer for the name
    // string manipulation. We only need the name buffer in this function, and
    // we don't need to use the receive buffer until after the device is opened.

    namelen = strlen(cdevname)+2+1;
    namebuf = kmalloc(namelen+1); // TODO temporarily use rxbufmem here?
    snprintf(namebuf, namelen, "%sch", cdevname);

    mux = kcalloc(1, sizeof(struct iomux4));

    // Check if the channel device names are available, so that when we cann
    // register_device() for each channel device, we know we will succeed.
    // (register_device() should only fail if the name is already taken.)

    for (int i = 0; i < 4; i++) {
        namebuf[namelen] = '0' + i;
        if (device_exists(namebuf)) {
            result = -EEXIST;
            goto fail1;
        }
    }

    // Initialize main imoux4 structure members
    
    condition_init(&mux->rxup, "iomux4.rxup");
    condition_init(&mux->txup, "iomux4.txup");
    rbuf_init(&mux->rxbuf, mux->rxbufmem, 64);
    mux->cdevname = cdevname;
    mux->state = IOM4_UNSYNC;
    mux->cio = cio;

    // Initialize I/O object for each channel and register the channel devices.
    // The rest (initializing channel receive buffers) happens when a channel is
    // opened.

    for (int i = 0; i < 4; i++) {
        struct io * const io = &mux->ch[i].io;
        ioinit(io, iomux4_iointf+i, /* blksz */ 1, /* refcnt */ 0);

        namebuf[namelen] = '0' + i;
        result = register_device(namebuf, -1, iomux4_openfn[i], mux);

        if (result != 0)
            panic(__func__);
    }

    goto done;

fail1:
    kfree(mux);
done:
    kfree(namebuf);
    return result;
}

// DEVICE AND IOINTF FUNCTION DEFINITIONS
//

int iomux4_ch0_open(struct io ** ioptr, struct iomux4 * mux) {
    return iomux4_chan_open(ioptr, mux, 0);
}

int iomux4_ch0_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    return iomux4_chan_reclaim(mux, 0);
}

long iomux4_ch0_read(struct io * io, void * buf, long bufsz) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    return iomux4_chan_read(mux, buf, bufsz, 0);
}

long iomux4_ch0_write(struct io * io, const void * buf, long buflen) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    return iomux4_chan_write(mux, buf, buflen, 0);
}

int iomux4_ch0_ioctl(struct io * io, int op, void * arg) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    return iomux4_chan_ioctl(mux, op, arg, 0);
}

int iomux4_ch1_open(struct io ** ioptr, struct iomux4 * mux) {
    return iomux4_chan_open(ioptr, mux, 1);
}

int iomux4_ch1_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[1].io);
    return iomux4_chan_reclaim(mux, 1);
}

long iomux4_ch1_read(struct io * io, void * buf, long bufsz) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[1].io);
    return iomux4_chan_read(mux, buf, bufsz, 1);
}

long iomux4_ch1_write(struct io * io, const void * buf, long buflen) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[1].io);
    return iomux4_chan_write(mux, buf, buflen, 1);
}

int iomux4_ch1_ioctl(struct io * io, int op, void * arg) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[1].io);
    return iomux4_chan_ioctl(mux, op, arg, 1);
}

int iomux4_ch2_open(struct io ** ioptr, struct iomux4 * mux) {
    return iomux4_chan_open(ioptr, mux, 2);
}

int iomux4_ch2_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[2].io);
    return iomux4_chan_reclaim(mux, 2);
}

long iomux4_ch2_read(struct io * io, void * buf, long bufsz) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[2].io);
    return iomux4_chan_read(mux, buf, bufsz, 2);
}

long iomux4_ch2_write(struct io * io, const void * buf, long buflen) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[2].io);
    return iomux4_chan_write(mux, buf, buflen, 2);
}

int iomux4_ch2_ioctl(struct io * io, int op, void * arg) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[2].io);
    return iomux4_chan_ioctl(mux, op, arg, 2);
}

int iomux4_ch3_open(struct io ** ioptr, struct iomux4 * mux) {
    return iomux4_chan_open(ioptr, mux, 3);
}

int iomux4_ch3_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[3].io);
    return iomux4_chan_reclaim(mux, 3);
}

long iomux4_ch3_read(struct io * io, void * buf, long bufsz) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[3].io);
    return iomux4_chan_read(mux, buf, bufsz, 3);
}

long iomux4_ch3_write(struct io * io, const void * buf, long buflen) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[3].io);
    return iomux4_chan_write(mux, buf, buflen, 3);
}

int iomux4_ch3_ioctl(struct io * io, int op, void * arg) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[3].io);
    return iomux4_chan_ioctl(mux, op, arg, 3);
}

// INTERNAL FUNCTION DEFINITIONS
//

int iomux4_chan_open(struct io ** ioptr, struct iomux4 * mux, int chno) {
    void * chrxbufmem;
    int result;

    // Check if the carrier device is open, since we wait to open the carrier
    // device until one of the channel devices is opened. 

    if (mux->cio == NULL) {
        result = open_device(mux->cdevname, &mux->cio);
        if (result < 0) return result;

        // Device block must be 1 byte
        if (ioblksz(mux->cio) != 1)
            panic(__func__);
        
        // Allocate space for each channel's receive buffers. Each channel gets
        // 1/4 of a page. Initialize receive buffers for each channel.

        chrxbufmem = alloc_phys_page();
        
        for (int i = 0; i < 4; i++) {
            struct ringbuf * const chrxbuf = &mux->ch[i].rxbuf;
            rbuf_init(chrxbuf, chrxbufmem + i*(PAGE_SIZE/4), PAGE_SIZE/4);
        }

        mux->err = 0;
        mux->rxbusy = 0;
        mux->txbusy = 0;
        mux->state = IOM4_UNSYNC;
        mux->txstop = 0;
    }

    // Send sync request sequence here. Then wait on iomux4_synchronize()
}

int iomux4_chan_reclaim(struct iomux4 * mux, int chno) {
    // ...
}

long iomux4_chan_read(struct iomux4 * mux, void * buf, long bufsz, int chno) {
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

long iomux4_chan_write(struct iomux4 * mux, const void * buf, long buflen, int chno) {
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

int iomux4_chan_ioctl(struct iomux4 * mux, int op, void * arg, int chno) {
    // ...
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
