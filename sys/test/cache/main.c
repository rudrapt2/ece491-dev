// main.c - main function of the kernel (called from start.s)
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "../../conf.h"
#include "../../console.h"
#include "../../intr.h"
#include "../../device.h"
#include "../../thread.h"
#include "../../filesys.h"
#include "../../process.h"
#include "../../cache.h"
#include "../../error.h"
#include "../../misc.h"
#include "../../io.h"

extern void board_init(unsigned int hartid, void * dtb); // from board/xxx.c
extern void attach_devices(void); // from board/xxx.c

struct cache * init_cache() {
    struct io * bkgio = create_memio(alloc_phys_pages(2), 4096*2, NULL);

    return create_cache(bkgio, 4096);
}

void test_cache(struct cache * cache) {
    char * data;
    for (int i = 0; i < 4096; i++) {
        cache_fetch(cache, 4096, (void**)&data);
        data[i] = (char)i;
        cache_release(cache, data, 1);

        yield_running_thread();

        cache_fetch(cache, 4096, (void**)&data);
        for (int j=0; j<=i; j++)
            assert(data[j] == (char)j);
        cache_release(cache, data, 0);
    }
    kprintf("test cache passed!\n");
}

void main(unsigned int hartid, void * dtb) {
    board_init(hartid, dtb);
    intrmgr_init();
    devmgr_init();
    thrmgr_init();

    // MP3 stuff
    memory_init();
    procmgr_init();
    fsmgr_init();

    attach_devices();
    enable_interrupts();

    struct cache * cache = init_cache();
    test_cache(cache);
}
