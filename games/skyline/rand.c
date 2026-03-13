/*! @file viorng.c
    @brief VirtIO rng device
    @copyright Copyright (c) 2024-2026 University of Illinois
    @license SPDX-License-identifier: NCSA
*/

#include "usr/heap.h"
#include "usr/error.h"
#include "usr/string.h"
#include "usr/syscall.h"
#include <stdint.h>

// INTERNAL FUNCTION DECLARATIONS
//

// INTERNAL GLOBAL VARIABLES
//

static int rngfd;

// EXPORTED FUNCTION DEFINITIONS
//

void rand_init();

uint64_t rand(); 
int_fast32_t rand1(int_fast32_t rmin, int_fast32_t rmax);
int_fast32_t rand2(int_fast32_t rmin, int_fast32_t rmax);
int_fast32_t rand3(int_fast32_t rmin, int_fast32_t rmax);

// Attaches a VirtIO rng device. Declared and called directly from virtio.c.

void rand_init() {
    rngfd = _open(-1, "/dev/viorng0");
    if (rngfd < 0) {
        printf("Failed to open rng device (%s)\n", error_desc(rngfd));
        _exit();
    }
}

uint64_t rand() {
    uint64_t buf;
    int res = _read(rngfd, &buf, sizeof(buf));
    if (res < 0) {
        printf("Failed to read from rng device (%s)\n", error_desc(res));
        _exit();
    }
    return buf;
}

// Returns a value chosen uniformly at random from the range [rmin,rmax].

int_fast32_t rand1(int_fast32_t rmin, int_fast32_t rmax) {
    return rmin + rand() % (rmax - rmin + 1);
}

// Returns a random value in the range [rmin,rmax], chosen with a bias toward
// the middle. Specifically, we return the sum of two random draws from
// appropriately chosen uniform distributions.

int_fast32_t rand2(int_fast32_t rmin, int_fast32_t rmax) {
    uint_fast32_t r0, r1;
    
    r0 = rand() % ((rmax - rmin)/2 + 1);
    r1 = rand() % ((rmax - rmin + 3)/2);
    return (rmin + r0 + r1);
}

int_fast32_t rand3(int_fast32_t rmin, int_fast32_t rmax) {
    uint_fast32_t r0, r1, r2;

    r0 = rand() % ((rmax - rmin)/3 + 1);
    r1 = rand() % ((rmax - rmin + 4)/3);
    r2 = rand() % ((rmax - rmin + 5)/3);
 
    return (rmin + r0 + r1 + r2);
}