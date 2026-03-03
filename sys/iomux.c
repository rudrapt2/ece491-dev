// iomux.c - I/O Multiplexing
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef IOMUX_TRACE
#define TRACE
#endif

#ifdef IOMUX_DEBUG
#define DEBUG
#endif

#include "rbuf.h"
#include "ioimpl.h"
#include "thread.h"
#include "misc.h"
#include "string.h"
#include "memory.h"
#include "error.h"
#include "device.h"
#include "heap.h"

// Control bytes

#define CTLCH0 0xC0 // Select channel 0
#define CTLCH1 0xC1 // Select channel 1
#define CTLCH2 0xC2 // Select channel 2
#define CTLCH3 0xC3 // Select channel 3
#define CTLESC 0xCE // Escape

#define ISCTLCHn(c) (((c) & 0xfc) == 0xc0)

#define RXBUFSZ 64 // must be 64

struct iomux4_chan {
    struct io io;
    struct rbuf rxbuf;
    unsigned long dropcnt;
};

struct iomux4 {
    const char * cdevname;
    struct io * cdevio;
    signed char rxchno;
    char rxesc, rxeof;
    char rxbusy;
    int err;
    struct rwlock txlock;
    struct condition rxupd;
    struct iomux4_chan ch[4];
};

// INTERNAL FUNCTION DECLARATIONS
//

static int iomux4_chan_open (
    struct io ** ioptr, struct iomux4 * mux, int chno);

static long iomux4_chan_read (
    struct iomux4 * mux, void * buf, long bufsz, int chno);

static long iomux4_chan_write (
    struct iomux4 * mux, const void * buf, long buflen, int chno);

static int iomux4_chan_ioctl (
    struct iomux4 * mux, int op, void * arg, int chno);

static void iomux4_chan_reclaim(struct iomux4 * mux, int chno);

static void iomux4_receive(struct iomux4 * mux);

// DEVICE AND IOINTF FUNCTION DECLARATIONS
//

static int iomux4_ch0_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch0_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch0_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch0_ioctl(struct io * io, int op, void * arg);
static void iomux4_ch0_reclaim(struct io * io);

static int iomux4_ch1_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch1_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch1_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch1_ioctl(struct io * io, int op, void * arg);
static void iomux4_ch1_reclaim(struct io * io);

static int iomux4_ch2_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch2_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch2_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch2_ioctl(struct io * io, int op, void * arg);
static void iomux4_ch2_reclaim(struct io * io);

static int iomux4_ch3_open(struct io ** ioptr, struct iomux4 * mux);
static long iomux4_ch3_read(struct io * io, void * buf, long bufsz);
static long iomux4_ch3_write(struct io * io, const void * buf, long buflen);
static int iomux4_ch3_ioctl(struct io * io, int op, void * arg);
static void iomux4_ch3_reclaim(struct io * io);


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
    char namebuf[64];
    size_t namelen;
    int result = 0;

    trace("%s(\"%s\")", __func__, cdevname);

    // Check if carrier device exists. We don't actually open it until one of
    // the channel devices is opened.

    if (!device_exists(cdevname))
        return -ENOENT;

    // We use the receive buffer memory as a string buffer to create channel
    // device names, which have the form CDEVch0 ... CDEVch3, where CDEV is the
    // carrier.

    namelen = strlen(cdevname);
    if (namelen+3+1 > sizeof(namebuf))
        return -ENAMETOOLONG;

    // Check if the channel device names are available, so that when we call
    // register_device() for each channel device, we know we will succeed.
    // (register_device() should only fail if the name is already taken.)

    for (int i = 0; i < 4; i++) {
        snprintf(namebuf, sizeof(namebuf), "%sch%d", cdevname, i);
        if (device_exists(namebuf)) {
            return -EEXIST;
        }
    }

    mux = kcalloc(1, sizeof(struct iomux4));

    // Initialize iomux4 structure members
    
    rwlock_init(&mux->txlock, "iomux4.txlock");
    condition_init(&mux->rxupd, "iomux4.rxupd");
    mux->cdevname = cdevname;

    // Initialize I/O objects for each channel and register the channel devices.
    // The rest (initializing channel receive buffers) happens when a channel is
    // opened.

    for (int i = 0; i < 4; i++) {
        struct io * const io = &mux->ch[i].io;
        ioinit(io, iomux4_iointf+i, /* blksz */ 1, /* refcnt */ 0);

        snprintf(namebuf, sizeof(namebuf), "%sch%d", cdevname, i);
        result = register_device(namebuf, -1, iomux4_openfn[i], mux);

        if (result != 0)
            panic(__func__);
    }

    return 0;
}

// INTERNAL FUNCTION DEFINITIONS
//

int iomux4_chan_open(struct io ** ioptr, struct iomux4 * mux, int chno) {
    struct io * const chio = &mux->ch[chno].io;
    int result;

    trace("%s(%p,%d)", __func__, mux, chno);

    if (iorefcnt(chio) != 0)
        return -EBUSY;

    // We wait to open the carrier device until one of the channels is opened. 

    if (mux->cdevio == NULL) {
        result = open_device(mux->cdevname, &mux->cdevio);
        
        // If open_device() fails, it could be because the device is already
        // open, but may become available later. So an error here is not sticky
        // (does not change state of IOM4_ERR).

        if (result < 0)
            return result;

        // Device block must be 1 byte

        if (ioblksz(mux->cdevio) != 1)
            return -ENOTSUP;
        
        // Allocate space for each channel's receive buffers. Each channel gets
        // 1/4 of a page. Initialize receive buffers for each channel.

        void * const chrxbufmem = alloc_phys_page();
        
        for (int i = 0; i < 4; i++) {
            struct rbuf * const chrxbuf = &mux->ch[i].rxbuf;
            rbuf_init(chrxbuf, chrxbufmem + i*(PAGE_SIZE/4), PAGE_SIZE/4);
        }

        mux->rxesc = 0;
        mux->rxeof = 0;
        mux->err = 0;
        mux->rxchno = -1;
    }
    
    rbuf_reset(&mux->ch[chno].rxbuf);
    mux->ch[chno].dropcnt = 0;

    *ioptr = ioaddref(&mux->ch[chno].io);
    return 0;
}

void iomux4_chan_reclaim(struct iomux4 * mux, int chno __attribute__ ((unused))) {
    // If any channels are still open, we're done. Otherwise, close the carrier device.

    for (int i = 0; i < 4; i++) {
        if (iorefcnt(&mux->ch->io) != 0)
            return;
    }

    iodropref(mux->cdevio);
    mux->cdevio = NULL;

    free_phys_page(rbuf_bufmem(&mux->ch[0].rxbuf));
}

long iomux4_chan_read(struct iomux4 * mux, void * buf, long bufsz, int chno) {
    struct iomux4_chan * const ch = mux->ch+chno;
    struct rbuf * const rxbuf = &ch->rxbuf;

    trace("%s(%p,%ld,%d)", __func__, mux, bufsz, chno);

    if (bufsz == 0)
        return 0;
    
    while (rbuf_empty(rxbuf)) {
        debug("%s(chno=%d): ch[%d].rxbuf is empty", __func__, chno, chno);
        if (ch->dropcnt != 0)
            return -EIO;
        
        if (mux->err != 0 || mux->rxeof)
            return mux->err;
            
        if (mux->rxbusy) {
            debug("%s(chno=%d): Another thread is receiving; waiting for rxupd", __func__, chno);
            condition_wait(&mux->rxupd);
            continue;
        }
        
        debug("%s(chno=%d): Taking control of receiver", __func__, chno);
        mux->rxbusy = 1;

        while (rbuf_empty(rxbuf) && mux->err == 0 && !mux->rxeof) {
            iomux4_receive(mux); // receive some data from carrier
            condition_broadcast(&mux->rxupd);
        }

        mux->rxbusy = 0;
    }
        
    return rbuf_getb(rxbuf, buf, bufsz);
}

void iomux4_receive(struct iomux4 * mux) {
    unsigned char cbuf[64];
    long rlen;

    trace("%s()", __func__);

    assert (mux->rxbusy); // should be holding lock

    rlen = ioread(mux->cdevio, cbuf, sizeof(cbuf));

    if (rlen <= 0) {
        if (rlen < 0)
            mux->err = rlen;
        else
            mux->rxeof = 1;
        return;
    }

    for (int i = 0; i < rlen; i++) {
        int const rxesc = mux->rxesc;
        int const c = cbuf[i];
        mux->rxesc = 0;

        if (!rxesc && (ISCTLCHn(c) || c == CTLESC)) {
            if (ISCTLCHn(c))
                mux->rxchno = cbuf[i] & 3;
            else
                mux->rxesc = 1;
        } else {
            if (0 <= mux->rxchno) {
                if (!rbuf_full(&mux->ch[mux->rxchno].rxbuf))
                    rbuf_putc(&mux->ch[mux->rxchno].rxbuf, cbuf[i]);
                else
                    mux->ch[mux->rxchno].dropcnt += 1;
            }
        }
    }
}


long iomux4_chan_write(struct iomux4 * mux, const void * buf, long buflen, int chno) {
    long bufpos = 0;
    long n, wlen;
    unsigned char c;
    long retval;

    trace("%s(%p,%ld,%d)", __func__, mux, buflen, chno);

    if (buflen == 0)
        return 0;

    rwlock_acquire(&mux->txlock, /* exclusive */ 1);

    if (mux->err != 0) {
        retval = mux->err;
        goto done_release_txlock;
    }

    c = CTLCH0+chno;
    wlen = iowrite(mux->cdevio, &c, 1);

    if (wlen != 1) {        
        if (wlen < 0)
            mux->err = wlen;
        else
            assert (wlen == 0);

        retval = wlen;
        goto done_release_txlock;
    }
    
    n = -1; // number of unescaped bytes we can send

    for (;;) {
        const unsigned char * const cbuf = buf+bufpos;
    
        // Count how many bytes from /bufpos/ do not need to be escaped and send
        // them as a block.

        while (++n < buflen-bufpos) {
            if (ISCTLCHn(cbuf[n]) || cbuf[n] == CTLESC)
                break;
        }

        if (n > 0) {
            wlen = iowrite(mux->cdevio, cbuf, n);
            
            if (wlen != n) {
                if (wlen < 0) {
                    mux->err = wlen;
                    retval = (bufpos > 0) ? bufpos : wlen;
                } else {
                    assert (wlen < n);
                    retval = bufpos + wlen;
                }

                goto done_release_txlock;
            }

            bufpos += n;
            n = 0;
        }

        // If we're not at the end of the buffer, that means the next byte needs
        // to be escaped. Send escape byte CTLESC. The byte that needs to be
        // escaped will be sent in next iteration.

        if (bufpos < buflen) {
            c = CTLESC;
            wlen = iowrite(mux->cdevio, &c, 1);
            
            if (wlen != 1) {                
                if (wlen < 0)
                    mux->err = wlen;
                else
                    assert (wlen == 0);

                retval = (bufpos > 0) ? bufpos : wlen;                
                goto done_release_txlock;
            }
        } else
            break;
    }

    retval = bufpos;

done_release_txlock:
    rwlock_release(&mux->txlock);
    return retval;
}

int iomux4_chan_ioctl(struct iomux4 * mux, int op, void * arg, int chno) {
    struct iomux4_chan * const ch = mux->ch+chno;
    struct rbuf * const rxbuf = &ch->rxbuf;

    trace("%s(%p,%d,%d)", __func__, mux, op, chno);

    switch (op) {
    case IOC_RESET:
        rbuf_reset(rxbuf);
        ch->dropcnt = 0;
        return 0;
    default:
        return -ENOTSUP;
    }
}

// DEVICE AND IOINTF FUNCTION DEFINITIONS
//

int iomux4_ch0_open(struct io ** ioptr, struct iomux4 * mux) {
    return iomux4_chan_open(ioptr, mux, 0);
}

void iomux4_ch0_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[0].io);
    iomux4_chan_reclaim(mux, 0);
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

void iomux4_ch1_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[1].io);
    iomux4_chan_reclaim(mux, 1);
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

void iomux4_ch2_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[2].io);
    iomux4_chan_reclaim(mux, 2);
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

void iomux4_ch3_reclaim(struct io * io) {
    struct iomux4 * const mux = (void*)io - offsetof(struct iomux4, ch[3].io);
    iomux4_chan_reclaim(mux, 3);
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