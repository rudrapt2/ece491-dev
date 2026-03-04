// rtc.c - Goldfish RTC driver
// 
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef RTC_TRACE
#define TRACE
#endif

#ifdef RTC_DEBUG
#define DEBUG
#endif

#include "misc.h"
#include "device.h"
#include "console.h"
#include "string.h"
#include "heap.h"
#include "ioimpl.h"

#include "error.h"

#include <stdint.h>

// INTERNAL TYPE DEFINITIONS
// 

struct rtc_regs {
    uint32_t time_low;  // read first, latches time_high
    uint32_t time_high; //
};

struct rtc_device {
    volatile struct rtc_regs * regs;
    struct io io;
};

// INTERNAL FUNCTION DEFINITIONS
//

static int rtc_open(struct io ** ioptr, void * aux);
static void rtc_reclaim(struct io * io);
static long rtc_read(struct io * io, void * buf, long bufsz);

static uint64_t read_real_time(volatile struct rtc_regs * regs);

// INTERNAL GLOBAL VARIABLES AND CONSTANTS
//

static const struct iointf rtc_intf = {
    .implname = "rtc",
    .read = &rtc_read
};

// EXPORTED FUNCTION DEFINITIONS
// 

void attach_rtc(void * mmio_base) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct rtc_device * rtc;

    // Because there is usually only one RTC device, we could also allocate the
    // struct rtc statically, or not even bother with a struct at all.

    rtc = kcalloc(1, sizeof(*rtc));

    rtc->regs = mmio_base;

    register_device("rtc", -1, &rtc_open, rtc);
    ioinit(&rtc->io, &rtc_intf, 8, 0);
#endif
}

int rtc_open(struct io ** ioptr, void * aux) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct rtc_device * const rtc = aux;
    trace("%s()", __func__);
    *ioptr = ioaddref(&rtc->io);
    return 0;
#endif
}

long rtc_read(struct io * io, void * buf, long bufsz) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    struct rtc_device * const rtc =
        (void*)io - offsetof(struct rtc_device, io);
    uint64_t time_now;

    trace("%s(bufsz=%ld)", __func__, bufsz);
    
    if (bufsz == 0)
        return 0;
    
    time_now = read_real_time(rtc->regs);

    memcpy(buf, &time_now, sizeof(uint64_t));
    return sizeof(uint64_t);
#endif
}

uint64_t read_real_time(volatile struct rtc_regs * regs) {
#ifdef STUDENT
    // YOUR CODE HERE
#else
    uint32_t lo, hi;

    // Note: must read low _then_ high

    lo = regs->time_low;
    hi = regs->time_high;
    return ((uint64_t)hi << 32) | lo;
#endif
}