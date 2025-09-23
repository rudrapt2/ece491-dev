// rtc.c - Goldfish RTC driver
// 
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef RTC_TRACE
#define TRACE
#endif

#ifdef RTC_DEBUG
#define DEBUG
#endif

#include "rtc.h"
#include "conf.h"
#include "misc.h"
#include "devimpl.h"
#include "console.h"
#include "string.h"
#include "heap.h"

#include "error.h"

#include <stdint.h>

// INTERNAL TYPE DEFINITIONS
// 

struct rtc_regs {
    uint32_t time_low;  // read first, latches time_high
    uint32_t time_high; //
};

struct rtc_device {
    struct serial base; // must be first
    volatile struct rtc_regs * regs;
};

// INTERNAL FUNCTION DEFINITIONS
//

static int rtc_open(struct serial * ser);
static void rtc_close(struct serial * ser);
static int rtc_recv(struct serial * ser, void * buf, unsigned int bufsz);

static uint64_t read_real_time(volatile struct rtc_regs * regs);

// INTERNAL GLOBAL VARIABLES AND CONSTANTS
//

static const struct serial_intf rtc_serial_intf = {
    .blksz = 8,
    .open = &rtc_open,
    .close = &rtc_close,
    .recv = &rtc_recv
};

// EXPORTED FUNCTION DEFINITIONS
// 

void rtc_attach(void * mmio_base) {
    struct rtc_device * rtc;

    // Because there is usually only one RTC device, we could also allocate the
    // struct rtc statically, or not even bother with a struct at all.

    rtc = kcalloc(1, sizeof(*rtc));

    rtc->regs = mmio_base;

    serial_init(&rtc->base, &rtc_serial_intf);
    register_device("rtc", DEV_SERIAL, rtc);
}

int rtc_open(struct serial * ser) {
    trace("%s()", __func__);
    return 0;
}

void rtc_close(struct serial * ser) {
    trace("%s()", __func__);
}

int rtc_recv(struct serial * ser, void * buf, unsigned int bufsz) {
    struct rtc_device * const rtc = (struct rtc_device*)ser;
    uint64_t time_now;

    trace("%s(bufsz=%ld)", __func__, bufsz);
    
    if (bufsz == 0)
        return 0;
    
    if (bufsz < sizeof(uint64_t))
        return -EINVAL;
    
    time_now = read_real_time(rtc->regs);

    memcpy(buf, &time_now, sizeof(uint64_t));
    return sizeof(uint64_t);
}

uint64_t read_real_time(volatile struct rtc_regs * regs) {
    uint32_t lo, hi;

    // Note: must read low _then_ high

    lo = regs->time_low;
    hi = regs->time_high;
    return ((uint64_t)hi << 32) | lo;
}