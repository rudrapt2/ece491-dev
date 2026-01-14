// test/thread/create-yield-hotpotato.c
//
// Copyright (c) 2026 University of Illinois
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

static volatile int potato;
static volatile int yieldcnt1;
static volatile int yieldcnt2;

static void thread_func1(void) {
    for (;;) {
        while (potato != 1) {
            running_thread_yield();
            yieldcnt1 += 1;
        }

        potato = 2;
    }
}

static void thread_func2(void) {
    for (;;) {
        while (potato != 2) {
            running_thread_yield();
            yieldcnt2 += 1;
        }

        potato = 0;
    }
}

void main(void) {
    test_name_init();
    console_init();
    timer_init(/* qemu virt timer frequency*/ 24000000);
    memory_init();
    thrmgr_init();
    
    // The test itself

    int yieldcnt0 = 0;

    spawn_thread("thr1", thread_func1);
    spawn_thread("thr2", thread_func2);

    for (int i = 0; i < 100; i++) {
        while (potato != 0) {
            running_thread_yield();
            yieldcnt0 += 1;
        }

        potato = 1;
    }

    kprintf("PASS %s\n", test_name);
    halt();
}