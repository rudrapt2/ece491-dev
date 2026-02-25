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

#define CTLCH0 0xC0 // Select channel 0
#define CTLCH1 0xC1 // Select channel 1
#define CTLCH2 0xC2 // Select channel 2
#define CTLCH3 0xC3 // Select channel 3
#define CTLCC  0xCC // reserved
#define CTLCD  0xCD // reserved
#define CTLESC 0xCE // Escape
#define CTLSYN 0xCF // Sync request

#define ISCTL(c) (((c) & 0xfc) == 0xc0 || ((c) & 0xfc) == 0xcc)
#define ISCTLCH(c) (((c) & 0xfc) == 0xc0)

struct iomux4_chan {
    struct io io;
    struct rbuf rxbuf;
    unsigned long dropcnt;
};

struct iomux4 {
    const char * cdevname;
    struct io * cdevio;
    signed char rxchno;
    char txbusy;
    char rxeof;
    int err;
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
static void iomux4_set_error(struct iomux4 * mux, int err);

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

static void iomux4_chan_reclaim(struct iomux4 * mux, int chno);

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
    void * chrxbufmem;
    char * namebuf;
    size_t namelen;
    size_t nbufsz;
    struct io * chio[4];
    int result = 0;

    // Check if carrier device exists. We don't actually open it until one of
    // the channel devices is opened.

    if (device_exists(cdevname))
        return -ENOENT;

    mux = kcalloc(1, sizeof(struct iomux4));

    // We use the receive buffer memory as a string buffer to create channel
    // device names, which have the form CDEVch0 ... CDEVch3, where CDEV is the
    // carrier.

    namelen = strlen(cdevname);
    namebuf = mux->rxbufmem;
    nbufsz = sizeof(mux->rxbufmem);

    if (nbufsz < namelen+3+1) {
        result = -ENAMETOOLONG;
        goto fail_free_mux;
    }

    // Check if the channel device names are available, so that when we call
    // register_device() for each channel device, we know we will succeed.
    // (register_device() should only fail if the name is already taken.)

    for (int i = 0; i < 4; i++) {
        snprintf(namebuf, nbufsz, "%sch%d", cdevname, i);
        if (device_exists(namebuf)) {
            result = -EEXIST;
            goto fail_free_mux;
        }
    }

    // Initialize iomux4 structure members
    
    condition_init(&mux->rxup, "iomux4.rxup");
    condition_init(&mux->txup, "iomux4.txup");
    rbuf_init(&mux->rxbuf, mux->rxbufmem, 64);
    mux->cdevname = cdevname;

    // Initialize I/O objects for each channel and register the channel devices.
    // The rest (initializing channel receive buffers) happens when a channel is
    // opened.

    for (int i = 0; i < 4; i++) {
        struct io * const io = &mux->ch[i].io;
        ioinit(io, iomux4_iointf+i, /* blksz */ 1, /* refcnt */ 0);

        snprintf(namebuf, nbufsz, "%sch%d", cdevname, i);
        result = register_device(namebuf, -1, iomux4_openfn[i], mux);

        if (result != 0)
            panic(__func__);
    }

    return 0;

fail_free_mux:
    kfree(mux);
    return result;
}

// INTERNAL FUNCTION DEFINITIONS
//

int iomux4_chan_open(struct io ** ioptr, struct iomux4 * mux, int chno) {
    struct io * const chio = &mux->ch[chno].io;
    int result;

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
            panic(__func__);
        
        // Allocate space for each channel's receive buffers. Each channel gets
        // 1/4 of a page. Initialize receive buffers for each channel.

        void * const chrxbufmem = alloc_phys_page();
        
        for (int i = 0; i < 4; i++) {
            struct ringbuf * const chrxbuf = &mux->ch[i].rxbuf;
            rbuf_init(chrxbuf, chrxbufmem + i*(PAGE_SIZE/4), PAGE_SIZE/4);
        }

        mux->rxbusy = 0;
        mux->txbusy = 0;
        mux->rxeof = 0;
        mux->err = 0;
        mux->rxchno = -1;
    }
    
    rbuf_reset(&mux->ch[chno].rxbuf);

    return mux->err; // 0 if no error
}

void iomux4_chan_reclaim(struct iomux4 * mux, int chno __attribute__ ((unused))) {
    // If any channels are still open, we're done. Otherwise, close the carrier device.

    for (int i = 0; i < 4; i++) {
        if (iorefcnt(&mux->ch->io) != 0)
            return;
    }

    ioclose(mux->cdevio);
    mux->cdevio = NULL;

    free_phys_page(rbuf_bufmem(&mux->ch[0].rxbuf));
}

long iomux4_chan_read(struct iomux4 * mux, void * buf, long bufsz, int chno) {
    struct iomux4_chan * const ch = mux->ch+chno;
    struct ringbuf * const rxbuf = &ch->rxbuf;
    long wlen;

    if (bufsz == 0)
        return 0;
    
    if (rbuf_empty(rxbuf)) {
        while (rbuf_empty(rxbuf) && mux->err == 0 && !mux->rxeof)
            iomux4_recv_bytes(mux);
                
        if (mux->err != 0)
            return mux->err;
    }

    return rbuf_getb(rxbuf, buf, bufsz);
}

long iomux4_chan_write(struct iomux4 * mux, const void * buf, long buflen, int chno) {
    long bufpos = 0;
    long wlen;

    if (buflen == 0)
        return 0;
    
    while (mux->txbusy)
        condition_wait(&mux->txup);

    if (mux->err != 0)
        return mux->err;

    mux->txbusy = 1;

    unsigned char ctlbyte = CTLCH0 + chno;
    wlen = iowrite(mux->cdevio, &ctlbyte, 1);

    if (wlen != 1) {
        if (wlen < 0)
            iomux4_set_error(mux, wlen);
        else
            assert (wlen == 0);
        goto done_return_wlen;
    }

    while (bufpos < buflen) {
        const unsigned char * const cbuf = buf+bufpos;
        long n = 0;

        while (n < buflen-bufpos && !ISCTL(cbuf[n]))
            n += 1;
        
        if (n > 0) {
            wlen = iowrite(mux->cdevio, cbuf, n);
            if (wlen != n) {
                

    if (wlen < 0)
        iomux4_set_error(mux, wlen);

done_return_wlen:
    mux->txbusy = 0;
    return wlen;
}

int iomux4_chan_ioctl(struct iomux4 * mux, int op, void * arg, int chno) {
    switch (op) {
    case IOC_RESET:
        rbuf_reset(&mux->ch[chno].rxbuf);
        return 0;
    default:
        return -ENOTSUP;
    }
}

void iomux4_synchronize(struct iomux4 * mux) {
    int nsync, nbytes;

    while (mux->state == IOM4_UNSYNC && mux->rxbusy)
        condition_wait(&mux->rxup);
    
    mux->rxbusy = 1;

    // Reset al internal buffers and the carrier to clear stale data.

    rbuf_reset(&mux->rxbuf);
    for (int i = 0; i < 4; i++)
        rbuf_reset(&mux->ch[i].rxbuf);
    ioctl(mux->cdevio, IOC_RESET, NULL);

    iomux4_send_syncseq(mux, CTLSYNCREQ);

    if (mux->state != IOM4_UNSYNC)
        goto iomux4_synchronize_done;

    // Wait to receive SYNCLEN-byte sync sequence. If we receive SYNCRETRY
    // bytes without seeing a sync sequence send request again.

    nsync = 0;
    nbytes = 0;

    while (nbytes < SYNCRETRY) {
        int const c = rbuf_getc(&mux->rxbuf);

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

iomux4_synchronize_done:
    mux->rxbusy = 0;
}

void iomux4_recv_frame(struct iomux4 * mux) {
    struct rbuf * const rxbuf = &mux->rxbuf;
    int chno, blklen;
    int c;

    if (mux->rxeof || mux->err != 0)
        return;

    mux->rxbusy = 1;

    c = rbuf_getc(rxbuf);

    while (ctlbyte < 0) {
        iomux4_recv_bytes(mux);
        if (mux->state == IOM4_ERR || mux->rxeof)
            goto iomux4_recv_frame_done;
        ctlbyte = rbuf_getc(rxbuf);
    }

    chno = ctlbyte >> 6;
    blklen = ctlbyte & 0x3f;

    if (blklen == 0) {
        switch (ctlbyte) {
        case CTLSYNCREQ:
            iomux4_send_syncseq(mux, CTLSYNC);
            if (mux->state == IOM4_ERR)
                goto iomux4_recv_frame_done;
            break;
        }
    }

    do {
        struct rbuf * const chrxbuf = &mux->ch[chno].rxbuf;

        while (rbuf_readable(rxbuf) < blklen) {
            iomux4_recv_bytes(mux);
            if (mux->state == IOM4_ERR)
                goto iomux4_recv_frame_done;
        }
        
        blklen -= rbuf_move(chrxbuf, rxbuf, blklen);
    } while (blklen != 0 && !mux->rxeof);

iomux4_recv_frame_done:
    mux->rxbusy = 0;
}

void iomux4_recv_bytes(struct iomux4 * mux) {
    struct rbuf * const rxbuf = &mux->rxbuf;
    void * wptr;
    unsigned int wmax;
    int readlen;

    // MUST be called with rxbusy == 1 (locked)

    if (rbuf_full(rxbuf))
        return;
    
    if (rbuf_empty(rxbuf))
        rbuf_reset(rxbuf);
    
    wptr = rbuf_wptr(&mux->rxbuf, &wmax);
    readlen = ioread(mux->cdevio, wptr, wmax);

    if (readlen <= 0) {
        if (readlen < 0)
            iomux4_set_error(mux, readlen);
        else
            mux->rxeof = 1;
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
        result = iowrite(mux->cdevio, seqbuf + sbpos, sizeof(seqbuf) - sbpos);

        if (result < 0) {
            iomux4_set_error(result);
            break;
        }
        
        sbpos -= result;
    }

    mux->txbusy = 0;
}

static void iomux4_send_chstat(struct iomux4 * mux) {
    unsigned char minibuf[2];
    int result;

    while (mux->state != IOM4_ERR && mux->txbusy)
        condition_wait(&mux->txup);
    
    if (mux->state != IOM4_UNSYNC)
        return;
    
    minibuf[0] = CTLCHSTAT;
    minibuf[1] = mux->loc_chstat;
    result = iowrite(mux->cdevio, minibuf, sizeof(minibuf));

    if (result != sizeof(minibuf))
        iomux4_set_error(mux, (result < 0) ? result : EIO);
    else
        return 0;
}

static void iomux4_set_error(struct iomux4 * mux, int err) {
    mux->err = err;
    mux->state = IOM4_ERR;
    condition_broadcast(&mux->csup);
    condition_broadcast(&mux->rxup);
    condition_broadcast(&mux->txup);
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