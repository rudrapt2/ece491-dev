// text/iomux4/iomux4-loopback.c - Manual iomux4 test
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "console.h"
#include "intr.h"
#include "device.h"
#include "thread.h"
#include "string.h"
#include "error.h"
#include "misc.h" // for halt()
#include "io.h"
#include "iomux.h"
#include "timer.h"

extern void board_init(unsigned int hartid, void * dtb); // from board/xxx.c
extern void attach_board_devices(void); // from board/xxx.c
extern void attach_loop(void); // from dev/loop.c

static void run_tests(void);

void main(unsigned int hartid, void * dtb) {
    board_init(hartid, dtb);
    intrmgr_init();
    thrmgr_init();
    devmgr_init();

    attach_board_devices();
    attach_loop();

    int result = attach_iomux4("loop");
    if (result != 0) panic("attach_iomux4() failed");

    enable_interrupts();

    run_tests();
}

static volatile int done;
static volatile unsigned long long rngst;

static int rand(void) {
	rngst *= 25214903917UL;
	rngst += 11;
	return (rngst >> 12) & (-1U >> 1);
}

static void writer_thrfn(int chno, struct io * io) {
    unsigned char msgbuf[64];
    unsigned int msglen;
    unsigned int acc = 0;
    unsigned int val;

    while (!done) {
        msglen = 0;
        while (msglen < 64) {
            if ((rand() % 63 != 0) && msglen < 63) {
                val = rand() & 0x7f;
                msgbuf[msglen++] = val;
                acc += val;
            } else {
                msgbuf[msglen++] = 0x80 | (acc % (127-chno));
                acc = 0;
                break;
            }
        }

        iowrite(io, msgbuf, msglen);

        if (rand() % 3)
            sleep_ms(100 + rand() % 100);
    }
}

static void reader_thrfn(int chno, struct io * io) {
    unsigned char msgbuf[64];
    unsigned int acc = 0;
    long rlen;

    while (!done) {
        memset(msgbuf, 0, sizeof(msgbuf));
        rlen = ioread(io, msgbuf, sizeof(msgbuf));

        if (rlen <= 0) {
            kprintf("FAIL ioread(ch%d) returned %d",
                chno, rlen, (rlen < 0) ? error_desc(rlen) : "success");
            halt();
        }

        for (int i = 0; i < rlen; i++) {
            if (msgbuf[i] & 0x80) {
                if (acc % (127-chno) != (msgbuf[i] & 0x7f)) {
                    kprintf("FAIL Bad message 0x%02x on channel %d!\n", msgbuf[i], chno);
                    halt();
                }

                acc = 0;
            } else
                acc += msgbuf[i];
        }
    }
}

void run_tests(void) {
    static char devnamebuf[32];
    static char wthrnamebuf[4][4];
    static char rthrnamebuf[4][4];
    int wtid[4], rtid[4];
    struct io * chio[4];
    int result;

    rngst = rdtime();

    for (int i = 0; i < 4; i++) {
        snprintf(devnamebuf, sizeof(devnamebuf), "loopch%d", i);
        result = open_device(devnamebuf, chio+i);
        assert (result == 0);

        snprintf(wthrnamebuf[i], sizeof(wthrnamebuf[i]), "Wr%d", i);
        wtid[i] = spawn_thread(wthrnamebuf[i], (void*)writer_thrfn, i, chio[i]);
        assert (0 < wtid[i]);

        snprintf(rthrnamebuf[i], sizeof(rthrnamebuf[i]), "Rd%d", i);
        rtid[i] = spawn_thread(rthrnamebuf[i], (void*)reader_thrfn, i, chio[i]);
        assert (0 < rtid[i]);
    }

    kputs("Main thread going to sleep for 5 seconds");
    sleep_sec(5);

    done = 1;
    for (int i = 0; i < 4; i++)
        join_thread(wtid[i]);

    kputs("PASS");
}