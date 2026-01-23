// plic.h - Interface to RISC-V PLIC
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _PLIC_H_
#define _PLIC_H_

#define PLIC_PRIO_MIN 1 // QEMU-specific
#define PLIC_PRIO_MAX 7 // QEMU-specific
#define PLIC_SRC_CNT 96 // QEMU-specific

extern char plic_initialized;
extern void plic_init(void * mmio_base);

extern void plic_enable_source(int srcno, int prio);

extern void plic_disable_source(int srcno);

extern int plic_claim_interrupt(void);

extern void plic_finish_interrupt(int srcno);

#endif