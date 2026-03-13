/*! @file demo.c
    @brief Ahan Goel's implemetation of a VIOGPU Driver
    @copyright Copyright (c) 2024-2026 University of Illinois
    @license SPDX-License-identifier: NCSA
*/

#include "usr/io.h"
#include "usr/heap.h"
#include "usr/error.h"
#include "usr/string.h"
#include "usr/syscall.h"
#include <stdint.h>

#include <stdarg.h>

static int gpufd;

// Internal function declarations
void gpu_init();
long gpu_draw();
void gpu_attach_buffer(void ** fbuf_ptr);

void gpu_init() {
    gpufd = _open(-1, "/dev/viogpu0");
    if (gpufd < 0) {
        printf("Failed to open rng device (%s)\n", error_desc(gpufd));
        _exit();
    }
}

long gpu_draw() {
    return _write(gpufd, NULL, 0);
}

void gpu_attach_buffer(void ** fbuf_ptr) {
    
    // Map the buffer to the GPU
    int res = _ioctl(gpufd, IOC_MAPBUF, fbuf_ptr);
    if (res < 0) {
        printf("Failed to map fbuf (%s)", error_desc(res));
        _exit();
    }
}
