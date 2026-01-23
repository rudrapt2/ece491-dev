// intr.h - Interrupt manager
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _INTR_H_
#define _INTR_H_

#include "plic.h"
#include "riscv.h"

// EXPORTED CONSTANT DEFINITIONS
//

#define INTR_PRIO_MIN PLIC_PRIO_MIN
#define INTR_PRIO_MAX PLIC_PRIO_MAX
#define INTR_SRC_CNT PLIC_SRC_CNT

// EXPORTED FUNCTION DECLARATIONS
//

extern char intrmgr_initialized;
extern void intrmgr_init(void);

extern void enable_intr_source (
    int srcno,
    int prio,
    void (*isr)(int srcno, void * aux),
    void * israux
);

extern void disable_intr_source(int srcno);

extern long enable_interrupts(void);

extern long disable_interrupts(void);

extern void restore_interrupts(int prev_status);

extern int interrupts_enabled(void);

extern int interrupts_disabled(void);

#endif // _INTR_H_
