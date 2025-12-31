// test/thread.c - Test of thread system
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

static char test_name[] = __FILE__;

static void test_name_init(void) {
    char * p;

    p = strchr(test_name, '.');
    if (p != NULL)
        *p = '\0';
}

static void thread_func(void) {
    kprintf("PASS %s\n", test_name);
    halt_success();
}

void main(void) {
    static char miniheap[1][4096];
    int pass = 0;
    int tid;

    test_name_init();
    console_init();
    thrmgr_init();
    heap_init(miniheap[0], miniheap[1]);

    // The test itself
    tid = spawn_thread("dummy", thread_func);
    running_thread_yield();

    kprintf("FAIL %s\n", test_name);
    halt_failure();
}

// DUMMY FUNCTION DEFINITIONS
//

#define PAGE_CNT 8

void * alloc_phys_page(void) {
    static char pages[PAGE_CNT][PAGE_SIZE] __attribute__ ((aligned(PAGE_SIZE)));
    static int pgno;

    if (PAGE_CNT <= pgno)
        panic("Too many physical pages requested");
    
    return pages[pgno++];
}

void free_phys_page(void * pp) {
    // nothing
}