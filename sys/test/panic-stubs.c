// panic-stubs.c - Empty functions that panic when called
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include <stdint.h>
#include "string.h"
#include "misc.h"

static void __attribute__ ((noreturn))
    stub_called_panic(const char * func_name);

#define DEFINE_STUB(func) \
__attribute__ ((weak)) void func(void) { \
    stub_called_panic(__func__); \
}

// intr.h
DEFINE_STUB(handle_smode_interrupt);
DEFINE_STUB(handle_umode_interrupt);
DEFINE_STUB(enable_intr_source);
DEFINE_STUB(disable_intr_source);

// process.h
DEFINE_STUB(process_exit);
DEFINE_STUB(process_exec);

// syscall.h
DEFINE_STUB(handle_syscall);

// device.h
DEFINE_STUB(register_device);

// memory.h
DEFINE_STUB(active_mspace);
DEFINE_STUB(switch_mspace);
DEFINE_STUB(handle_umode_page_fault);

void stub_called_panic(const char * func_name) {
    static char msgbuf[256];
    snprintf(msgbuf, sizeof(msgbuf), "%s called", func_name);
    panic(msgbuf);
}