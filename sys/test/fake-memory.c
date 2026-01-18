// fake-memory.c - A memory manager that says all the right things
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "memory.h"
#include "heap.h"
#include "misc.h"
#include "string.h"

#define PAGE_CNT 16

static char miniheap[64*1024];
static char physpages[PAGE_CNT][PAGE_SIZE] __attribute__ ((aligned(PAGE_SIZE)));
static char pageinuse[PAGE_CNT];

char memory_initialized = 0;

void memory_init(void) {
    heap_init(miniheap, sizeof(miniheap));
    memory_initialized = 1;
}

void * alloc_phys_page(void) {
    int i;

    for (i = 0; i < PAGE_CNT; i++) {
        if (!pageinuse[i]) {
            memset(physpages[i], 0x11, PAGE_SIZE);
            pageinuse[i] = 1;
            return physpages[i];
        }
    }

    panic("Too many physical pages requested");
}

void free_phys_page(void * pp) {
    int i;

    for (i = 0; i < PAGE_CNT; i++) {
        if (pp == physpages[i]) {
            if (!pageinuse[i])
                panic("free_phys_page() called with page already freed");
            pageinuse[i] = 0;
            return;
        }
    }

    panic("free_phys_page() called with address not allocated");
}