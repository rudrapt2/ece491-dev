/*! @file plic.c
    @brief RISC-V PLIC    
    @copyright Copyright (c) 2024-2025 University of Illinois
    @license SPDX-License-identifier: NCSA

*/

#ifdef PLIC_TRACE
#define TRACE
#endif

#ifdef PLIC_DEBUG
#define DEBUG
#endif

#include "conf.h"
#include "plic.h"
#include "misc.h"

#include <stdint.h>

// INTERNAL MACRO DEFINITIONS
//

// CTX(i,0) is hartid /i/ M-mode context
// CTX(i,1) is hartid /i/ S-mode context

#define CTX(i,s) (2*(i)+(s))

// INTERNAL TYPE DEFINITIONS
// 

/*! @struct plic_regs
    @brief PLIC registers
*/
struct plic_regs {
	union {
		uint32_t priority[PLIC_SRC_CNT]; /**< Interrupt Priorities registers */
		char _reserved_priority[0x1000];
	};

	union {
		uint32_t pending[PLIC_SRC_CNT/32]; /**< Interrupt Pending Bits registers */
		char _reserved_pending[0x1000];
	};

	union {
		uint32_t enable[PLIC_CTX_CNT][32]; /**< Interrupt Enables registers */
		char _reserved_enable[0x200000-0x2000];
	};

	struct {
		union {
			struct {
				uint32_t threshold;	/**< Priority Thresholds registers */
				uint32_t claim;	/**< Interrupt Claim/Completion registers */
			};
			
			char _reserved_ctxctl[0x1000];
		};
	} ctx[PLIC_CTX_CNT];
};

#define PLIC (*(volatile struct plic_regs*)PLIC_MMIO_BASE)

// INTERNAL FUNCTION DECLARATIONS
//
/*!
 * @brief Set the priority level of an interrupt source. 
 * @details This function changes the priority level of an interrupt source. 
 * Each array entry matches an interrupt source.
 * @param srcno Source number.
 * @param level Priority level.
 */
static void plic_set_source_priority (
	uint_fast32_t srcno, uint_fast32_t level);
/*!
 * @brief Check if an interrupt source is pending. 
 * @details This function returns 1 for a pending interrupt, 0 otherwise. This is decided
 * by checking the bit in the pending array that corresponds to scrno.
 * @param srcno Source number.
 * @return 1 for a pending interrupt, 0 otherwise.
 */
static int plic_source_pending(uint_fast32_t srcno);
/*!
 * @brief Enables an interrupt source for a context
 * @details This function sets the appropriate bit in the enable array. It calculates the index based on srcno and ctxno, 
 * and sets the corresponding bit.
 * @param ctxno Context number.
 * @param srcno Source number.
 */
static void plic_enable_source_for_context (
	uint_fast32_t ctxno, uint_fast32_t srcno);
/*!
 * @brief Disables an interrupt source for a context.
 * @details This function clears the appropriate bit in the enable array. Like plic_enable_source_for_context, it must clear
 * the correct bit for the given context and source.
 * @param ctxno Context number.
 * @param srcid Source number.
 */
static void plic_disable_source_for_context (
	uint_fast32_t ctxno, uint_fast32_t srcno);
/*!
 * @brief Set the interrupt priority threshold for a context.
 * @details This function sets a threshold. Afterwards, the context should ignore intererupts below this priority.
 * @param ctxno Context number.
 * @param level Priority level.
 */
static void plic_set_context_threshold (
	uint_fast32_t ctxno, uint_fast32_t level);
/*!
 * @brief Claim an interrupt for a given context.
 * @details function reads from the claim register and returns the interrupt ID of the highest-priority pending 
 * interrupt. It returns 0 if no interrupts are pending.
 * @param ctxno Context number.
 * @return interrupt ID of the highest-priority pending interrupt. 0 if no interrupts are pending.
 */
static uint_fast32_t plic_claim_context_interrupt (
	uint_fast32_t ctxno);
/*!
 * @brief Complete the handling of an interrupt for a given context.
 * @details This function writes the interrupt source number back to the claim register, notifying the PLIC that 
 * the interrupt has been services.
 * @param ctxno Context number.
 * @param srcno Source number.
 */
static void plic_complete_context_interrupt (
	uint_fast32_t ctxno, uint_fast32_t srcno);

/*!
 * @brief Enable all interrupt sources for a given context.
 * @details This function sets all bits in the corresponding entry of enable array for the specified context.
 * @param ctxno Context number.
 */
static void plic_enable_all_sources_for_context(uint_fast32_t ctxno);
/*!
 * @brief Disable all interrupt sources for a given context.
 * @details his function clears all bits in the corresponding entry of enable array for the specified context.
 * @param ctxno Context number.
 */
static void plic_disable_all_sources_for_context(uint_fast32_t ctxno);

// We currently only support single-hart operation, sending interrupts to S mode
// on hart 0 (context 0). The low-level PLIC functions already understand
// contexts, so we only need to modify the high-level functions (plit_init,
// plic_claim_request, plic_finish_request)to add support for multiple harts.

// EXPORTED FUNCTION DEFINITIONS
// 

void plic_init(void) {
	int i;

	// Disable all sources by setting priority to 0

	for (i = 0; i < PLIC_SRC_CNT; i++)
		plic_set_source_priority(i, 0);
	
	// Route all sources to S mode on hart 0 only

	for (int i = 0; i < PLIC_CTX_CNT; i++)
		plic_disable_all_sources_for_context(i);
	
	plic_enable_all_sources_for_context(CTX(0,1));
}

extern void plic_enable_source(int srcno, int prio) {
	trace("%s(srcno=%d,prio=%d)", __func__, srcno, prio);
	assert (0 < srcno && srcno <= PLIC_SRC_CNT);
	assert (prio > 0);

	plic_set_source_priority(srcno, prio);
}

extern void plic_disable_source(int irqno) {
	if (0 < irqno)
		plic_set_source_priority(irqno, 0);
	else
		debug("plic_disable_irq called with irqno = %d", irqno);
}

extern int plic_claim_interrupt(void) {
	// FIXME: Hardwired S-mode hart 0
	trace("%s()", __func__);
	return plic_claim_context_interrupt(CTX(0,1));
}

extern void plic_finish_interrupt(int irqno) {
	// FIXME: Hardwired S-mode hart 0
	trace("%s(irqno=%d)", __func__, irqno);
	plic_complete_context_interrupt(CTX(0,1), irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//

static inline void plic_set_source_priority(uint_fast32_t srcno, uint_fast32_t level) {
	PLIC.priority[srcno] = level;
}

static inline int plic_source_pending(uint_fast32_t srcno) {
	const int i = srcno / 32; // in uint32_t units
	const int j = srcno % 32; // bit index inside uint32_t

	return ((PLIC.pending[i] >> j) & 1);
}

static inline void plic_enable_source_for_context(uint_fast32_t ctxno, uint_fast32_t srcno) {
	const int i = srcno / 32; // in uint32_t units
	const int j = srcno % 32; // bit index inside uint32_t
	volatile uint32_t * const ptr = PLIC.enable[ctxno]+i;
	__atomic_or_fetch(ptr, UINT32_C(1) << j, __ATOMIC_RELAXED);
}

static inline void plic_disable_source_for_context(uint_fast32_t ctxno, uint_fast32_t srcid) {
	const int i = srcid / 32; // in uint32_t units
	const int j = srcid % 32; // bit index in uint32_t
	volatile uint32_t * const ptr = PLIC.enable[ctxno]+i;

	__atomic_or_fetch(ptr, ~(UINT32_C(1) << j), __ATOMIC_RELAXED);
}

static inline void plic_set_context_threshold(uint_fast32_t ctxno, uint_fast32_t level) {
	PLIC.ctx[ctxno].threshold = level;
}

static inline uint_fast32_t plic_claim_context_interrupt(uint_fast32_t ctxno) {
	return PLIC.ctx[ctxno].claim;
}

static inline void plic_complete_context_interrupt(uint_fast32_t ctxno, uint_fast32_t srcno) {
	PLIC.ctx[ctxno].claim = srcno;
}

static void plic_enable_all_sources_for_context(uint_fast32_t ctxno) {
	int i;

	for (i = 0; i < PLIC_SRC_CNT/32; i++)
		PLIC.enable[ctxno][i] = ~UINT32_C(0);
}

static void plic_disable_all_sources_for_context(uint_fast32_t ctxno) {
	int i;

	for (i = 0; i < PLIC_SRC_CNT/32; i++)
		PLIC.enable[ctxno][i] = UINT32_C(0);
}
