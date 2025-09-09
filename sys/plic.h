/*! @file plic.h
    @brief RISC-V PLIC    
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA

*/

#ifndef _PLIC_H_
#define _PLIC_H_

// These functions are called from intr.c to handle external interrupts. Do not
// call these (including plic_init) directly from other parts of the kernel.

/*!
 * @brief Minimum priority level for PLIC 
 */
#define PLIC_PRIO_MIN 1 
/*!
 * @brief Maximum priority level for PLIC 
 */
#define PLIC_PRIO_MAX 7

/*!
 * @brief Initilize PLIC
 */
extern void plic_init(void);

/*!
 * @brief Enable the interrupt source with srcno by setting its priority level to prio.
 * @param srcno Source number.
 * @param prio Priority level.
 */
extern void plic_enable_source(int srcno, int prio);

/*!
 * @brief Disable the interrupt source with srcno.
 * @param srcno Source number.
 */
extern void plic_disable_source(int srcno);

/*!
 * @brief Claim an interrupt. Since we only have one context, S-mode hart 0, hardwire the claim interrupt to that context.
 * @return interrupt ID of the highest-priority pending interrupt. 0 if no interrupts are pending.
 */
extern int plic_claim_interrupt(void);

/*!
 * @brief Complete the handling of an interrupt given its source number. Since we only have one context, S-mode hart 0, hardwire the claim interrupt to that context.
 * @param srcno Source number.
 */
extern void plic_finish_interrupt(int srcno);

#endif