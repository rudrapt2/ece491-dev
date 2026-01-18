// test/thread/correct-tids.c
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

static volatile int tid_from_child;

static void thread_func(void) {
    tid_from_child = running_thread();

    for (;;)
        running_thread_yield();
}

void main(void) {
    test_name_init();
    console_init();
    timer_init(/* qemu virt timer frequency */ 24000000);
    memory_init();
    thrmgr_init();

    // The test itself

    if (running_thread() != 0) {
        kprintf("PASS %s\n", test_name);
        halt();
    }

    int ctid = spawn_thread("dummy", thread_func);

    for (int i = 0; i < 16 && tid_from_child == 0; i++)
        running_thread_yield();
    
    if (ctid != 0 && ctid == tid_from_child)
        kprintf("PASS %s\n", test_name);
    else
        kprintf("FAIL %s\n", test_name);
    
    halt();
}