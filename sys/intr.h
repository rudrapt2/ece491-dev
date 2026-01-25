// intr.h - Interrupt manager
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _INTR_H_
#define _INTR_H_

#include "plic.h"
#include "riscv.h"

// INTERRUPT MANAGER
//
// The interrupt manager provides:
// - A registration API for external interrupt sources (via the PLIC).
// - Helpers for manipulating the global interrupt enable state (sstatus.SIE).
// - A dispatcher used by the trap handler to service supervisor-mode interrupts.
//
// The interrupt manager is responsible for enabling supervisor external
// interrupts (sie.SEIE). The timer subsystem is responsible for enabling and
// disabling supervisor timer interrupts (sie.STIE). 
//
// External interrupt sources are identified by a platform-specific source
// number /srcno/ in the range [1..INTR_SRC_CNT]. Source 0 is reserved as "no
// interrupt" by the PLIC claim interface.
//
// Interrupt sources are handled by an ISR function:
//
//     void isr(int srcno, void * aux);
//
// The /aux/ pointer is stored verbatim and passed back to the ISR when the
// interrupt is dispatched. The lifetime of /aux/ must extend for as long as the
// total lifetime of the system.

// EXPORTED CONSTANT DEFINITIONS
//

#define INTR_PRIO_MIN PLIC_PRIO_MIN
#define INTR_PRIO_MAX PLIC_PRIO_MAX
#define INTR_SRC_CNT  PLIC_SRC_CNT

// EXPORTED FUNCTION DECLARATIONS
//

extern char intrmgr_initialized;

extern void intrmgr_init(void);

// Initializes the interrupt manager. This function enables supervisor external
// interrupts (sie.SEIE) and initializes internal ISR dispatch state.
//
// On entry intrmgr_init() assumes:
// - The PLIC is initialized, and /plic_initialized/ is 1.
// - intrmgr_init() is called once at system initialization time.
//
// On return intrmgr_init() guarantees:
// - /intrmgr_initialized/ is set to 1.
// - Supervisor external interrupts are enabled (sie.SEIE is set).
//
// * This function must be called once at system initialization time.
// * This function must _not_ be called from an ISR.
// * This function does not context-switch.

extern void enable_intr_source (
    int srcno,
    int prio,
    void (*isr)(int srcno, void * aux),
    void * israux
);

// Registers an ISR for an external interrupt source and enables that source in
// the PLIC with the given priority.
//
// The /isr/ pointer must remain valid for at least as long as the interrupt 
// source remains enabled. The /israux/ pointer is passed to the ISR and must 
// remain valid for the same duration.
//
// On entry enable_intr_source() assumes:
// - /srcno/ is in the range [1..INTR_SRC_CNT].
// - /prio/ is greater than 0.
// - /isr/ is a pointer to executable code.
// - intrmgr_init() has been called and /intrmgr_initialized/ is 1.
// - plic_init() has been called and /plic_initialized/ is 1.
//
// On return enable_intr_source() guarantees:
// - The ISR dispatch table contains (/isr/, /israux/) for /srcno/.
// - The PLIC is configured so that source /srcno/ is logically enabled 
//   for hart 0 in s-mode with priority /prio/.
//
// * This function may be called from an ISR.
// * This function does not context-switch.
//
// See also: disable_intr_source().

extern void disable_intr_source(int srcno);

// Disables an external interrupt source and removes its ISR registration.
//
// On entry disable_intr_source() assumes:
// - intrmgr_init() has been called and /intrmgr_initialized/ is 1.
// - plic_init() has been called and /plic_initialized/ is 1.
//
// On return disable_intr_source() guarantees:
// - The PLIC is configured so that source /srcno/ is disabled.
// - The ISR dispatch table entry for /srcno/ is cleared.
//
// * This function may be called from an ISR.
// * This function does not context-switch.

extern long enable_interrupts(void);

// Sets sstatus.SIE to 1 (globally enables interrupts) and returns the previous
// value of the sstatus CSR.
//
// On return enable_interrupts() guarantees:
// - sstatus.SIE is set (interrupts are globally enabled).
//
// * This function may be called from an ISR.
// * This function does not context-switch.
//
// See also: disable_interrupts(), restore_interrupts(), interrupts_enabled().


extern long disable_interrupts(void);

// Clears sstatus.SIE (globally disables interrupts) and returns the previous
// value of the sstatus CSR.
//
// The return value is typically passed to restore_interrupts() to restore
// the prior interrupt enable state.
//
// On return disable_interrupts() guarantees:
// - sstatus.SIE is clear (interrupts are globally disabled).
//
// * This function may be called from an ISR.
// * This function does not context-switch.
//
// See also: enable_interrupts(), restore_interrupts(), interrupts_disabled().


extern void restore_interrupts(int prev_status);

// Restores sstatus.SIE to its previous value. The /prev_status/ argument is
// typically the value returned by enable_interrupts() or disable_interrupts().
// Only the RISCV_SSTATUS_SIE bit of /prev_status/ is used.
//
// On entry restore_interrupts() assumes:
// - /prev_status/ is a value previously returned by enable_interrupts() or
//   disable_interrupts(), or any value whose RISCV_SSTATUS_SIE bit matches the
//   desired interrupt enable state.
//
// On return restore_interrupts() guarantees:
// - sstatus.SIE matches (prev_status & RISCV_SSTATUS_SIE).
//
// * This function may be called from an ISR.
// * This function does not context-switch.
//
// See also: enable_interrupts(), disable_interrupts().


extern int interrupts_enabled(void);

// Returns non-zero if interrupts are globally enabled (sstatus.SIE is set).
//
// * This function may be called from an ISR.
//
// See also: interrupts_disabled().


extern int interrupts_disabled(void);

// Returns non-zero if interrupts are globally disabled (sstatus.SIE is clear).
//
// * This function may be called from an ISR.
//
// See also: interrupts_enabled().

#endif // _INTR_H_
