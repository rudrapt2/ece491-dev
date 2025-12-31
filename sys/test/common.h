// common.h - Common function for tests
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "console.h"
#include "see.h"

unsigned int failcnt = 0;

#define TEST_ASSERT(c) do { \
    if (!(c)) { \
        kprintf("FAIL %s %s:%d (%s)\n", \
            __func__, __FILE__, __LINE__, #c); \
        failcnt++; \
    } \
} while (0)

#define HARD_ASSERT(c) do { \
    if (!(c)) { \
        kprintf("FAIL %s %s:%d (%s)\n", \
            __func__, __FILE__, __LINE__, #c); \
        halt_failure(); \
    } \
} while (0)

#define FINISH() do { \
    if (failcnt == 0) { \
        kputs("PASS"); \
        halt_success(); \
    } else \
        halt_failure(); \
} while (0)
