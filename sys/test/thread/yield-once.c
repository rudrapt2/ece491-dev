// test/thread/create-yield-once.c
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "console.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "heap.h"
#include "misc.h"
#include "timer.h"

static char test_name[] = __FILE__;

static void test_name_init(void) {
    char * p;

    p = strchr(test_name, '.');
    if (p != NULL)
        *p = '\0';
}

static void thread_func(void) {
    kprintf("PASS %s\n", test_name);
    halt();
}

void main(void) {
    test_name_init();
    console_init();
    timer_init(/* qemu virt timer frequency*/ 24000000);
    memory_init();
    thrmgr_init();
    
    // The test itself
    spawn_thread("dummy", thread_func);
    running_thread_yield();

    kprintf("FAIL %s\n", test_name);
    halt();
}