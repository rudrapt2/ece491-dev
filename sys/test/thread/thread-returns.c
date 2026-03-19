// test/thread/thread-returns.c
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "console.h"
#include "thread.h"
#include "page_table.h"
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
    // nothing
}

void main(void) {
    int pass = 0;
    int tid;

    test_name_init();
    console_init();
    timer_init(/* qemu virt timer frequency */ 24000000);
    memory_init();
    thrmgr_init();

    // The test itself
    tid = spawn_thread("dummy", thread_func);
    pass = (0 < tid);
    
    kprintf("%s %s\n", pass ? "PASS" : "FAIL", test_name);
}
