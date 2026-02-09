// memory.c - Physical and virtual memory manager
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef MEMORY_TRACE
#define TRACE
#endif

#ifdef MEMORY_DEBUG
#define DEBUG
#endif

#include "memory.h"

#include "conf.h"
#include "console.h"
#include "error.h"
#include "heap.h"
#include "misc.h"
#include "process.h"
#include "riscv.h"
#include "string.h"
#include "thread.h"

// COMPILE-TIME CONFIGURATION
//

// Minimum amount of memory in the initial heap block.

#ifndef HEAP_INIT_MIN
#define HEAP_INIT_MIN 256
#endif

// INTERNAL CONSTANT DEFINITIONS
//

#define MEGA_SIZE ((1UL << 9) * PAGE_SIZE)  // megapage size
#define GIGA_SIZE ((1UL << 9) * MEGA_SIZE)  // gigapage size

#define PTE_ORDER 3
#define PTE_CNT (1U << (PAGE_ORDER - PTE_ORDER))

#ifndef PAGING_MODE
#define PAGING_MODE RISCV_SATP_MODE_Sv39
#endif

#ifndef ROOT_LEVEL
#define ROOT_LEVEL 2
#endif

// IMPORTED GLOBAL SYMBOLS
//

// linker-provided (kernel.ld)
extern char _kimg_start[];
extern char _kimg_text_start[];
extern char _kimg_text_end[];
extern char _kimg_rodata_start[];
extern char _kimg_rodata_end[];
extern char _kimg_data_start[];
extern char _kimg_data_end[];
extern char _kimg_end[];

// EXPORTED GLOBAL VARIABLES
//

char memory_initialized = 0;

// INTERNAL TYPE DEFINITIONS
//

// We keep free physical pages in a linked list of _chunks_, where each chunk
// consists of several consecutive pages of memory. Initially, all free pages
// are in a single large chunk. To allocate a block of pages, we break up the
// smallest chunk on the list.

struct page_chunk {
    struct page_chunk * next;   // next page chunk in list
    unsigned long pagecnt;      // number of pages in chunk
};

struct pte {
    uint64_t flags : 8;
    uint64_t rsw : 2;
    uint64_t ppn : 44;
    uint64_t reserved : 7;
    uint64_t pbmt : 2;
    uint64_t n : 1;
};

// INTERNAL MACRO DEFINITIONS
//

#define VPN(vma) ((vma) / PAGE_SIZE)
#define VPN2(vma) ((VPN(vma) >> (2 * 9)) % PTE_CNT)
#define VPN1(vma) ((VPN(vma) >> (1 * 9)) % PTE_CNT)
#define VPN0(vma) ((VPN(vma) >> (0 * 9)) % PTE_CNT)

// The following macros test is a PTE is valid, global, or a leaf. The argument
// is a struct pte (*not* a pointer to a struct pte).

#define PTE_VALID(pte) (((pte).flags & PTE_V) != 0)
#define PTE_GLOBAL(pte) (((pte).flags & PTE_G) != 0)
#define PTE_LEAF(pte) (((pte).flags & (PTE_R | PTE_W | PTE_X)) != 0)

#define PT_INDEX(lvl, vpn) \
    (((vpn) & (0x1FF << (lvl * (PAGE_ORDER - PTE_ORDER)))) >> (lvl * (PAGE_ORDER - PTE_ORDER)))

// INTERNAL FUNCTION DECLARATIONS
//

static void ptab_reset(struct pte * ptab);

static struct pte * ptab_clone (struct pte * ptab);

static void ptab_discard(struct pte * ptab  // page table to discard
);

static void ptab_insert(struct pte * ptab,   // page table to modify
                        unsigned long vpn,  // virtual page number to insert
                        void * pp,           // pointer to physical page to insert
                        int rwxug_flags     // flags for inserted mapping
);

static void * ptab_remove(struct pte * ptab, unsigned long vpn);

static void ptab_adjust(struct pte * ptab, unsigned long vpn, int rwxug_flags);

struct pte * ptab_fetch(struct pte * ptab, unsigned long vpn);

static inline mtag_t active_space_mtag(void);
static inline mtag_t ptab_to_mtag(struct pte * root, unsigned int asid);
static inline struct pte * mtag_to_ptab(mtag_t mtag);
static inline struct pte * active_space_ptab(void);

static inline void * pageptr(uintptr_t n);
static inline uintptr_t pagenum(const void * p);
static inline int wellformed(uintptr_t vma);

static inline struct pte leaf_pte(const void * pp, uint_fast8_t rwxug_flags);
static inline struct pte ptab_pte(const struct pte * pt, uint_fast8_t g_flag);
static inline struct pte null_pte(void);

// INTERNAL GLOBAL VARIABLES
//

static mtag_t main_mtag;

static struct pte main_pt2[PTE_CNT] __attribute__((section(".bss.pagetable"), aligned(4096)));

static struct pte main_pt1_0x80000[PTE_CNT]
    __attribute__((section(".bss.pagetable"), aligned(4096)));

static struct pte main_pt0_0x80000[PTE_CNT]
    __attribute__((section(".bss.pagetable"), aligned(4096)));

static struct page_chunk * free_chunk_list;

// EXPORTED FUNCTION DECLARATIONS
//

void memory_init(void) {
    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;

    void * heap_start;
    void * heap_end;

    uintptr_t pma;
    const void * pp;

    trace("%s()", __func__);

    debug("           RAM: [%p,%p): %zu MB", RAM_START, RAM_END, RAM_SIZE / 1024 / 1024);
    debug("  Kernel image: [%p,%p)", _kimg_start, _kimg_end);

    // Kernel must fit inside 2MB megapage (one level 1 PTE)

    if (MEGA_SIZE < _kimg_end - _kimg_start) panic(NULL);

    // Initialize main page table with the following direct mapping:
    //
    //         0 to RAM_START:           RW gigapages (MMIO region)
    // _kimg_start to _kimg_end:         RX/R/RW pages based on kernel image
    // _kimg_end to RAM_START+MEGA_SIZE: RW pages (heap and free page pool)
    // RAM_START+MEGA_SIZE to RAM_END:   RW megapages (free page pool)
    //
    // RAM_START = 0x80000000
    // MEGA_SIZE = 2 MB
    // GIGA_SIZE = 1 GB

    // Identity mapping of MMIO region as two gigapage mappings
    for (pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        main_pt2[VPN2(pma)] = leaf_pte((void * )pma, PTE_R | PTE_W | PTE_G);

    // Third gigarange has a second-level subtable
    main_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // First physical megarange of RAM is mapped as individual pages with
    // permissions based on kernel image region.

    main_pt1_0x80000[VPN1(RAM_START_PMA)] = ptab_pte(main_pt0_0x80000, PTE_G);

    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_G);
    }

    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Enable paging; this part always makes me nervous.

    main_mtag = ptab_to_mtag(main_pt2, 0);
    csrw_satp(main_mtag);

    // Give the memory between the end of the kernel image and the next page
    // boundary to the heap allocator, but make sure it is at least
    // HEAP_INIT_MIN bytes.

    heap_start = _kimg_end;
    heap_end = (void * )ROUND_UP((uintptr_t)heap_start, PAGE_SIZE);

    if (heap_end - heap_start < HEAP_INIT_MIN) {
        heap_end += ROUND_UP(HEAP_INIT_MIN - (heap_end - heap_start), PAGE_SIZE);
    }

    if (RAM_END < heap_end)
        panic("out of memory");

    // Initialize heap memory manager

    heap_init(heap_start, heap_end - heap_end);

    debug("Heap allocator: [%p,%p): %zu KB free", heap_start, heap_end,
          (heap_end - heap_start) / 1024);

    debug("Heap allocator: [%p,%p): %zu KB free",
        heap_start, heap_end, (heap_end - heap_start) / 1024);

    free_chunk_list = heap_end; // heap_end is page aligned
    free_chunk_list->pagecnt = (RAM_END - heap_end) / PAGE_SIZE;
    free_chunk_list->next = NULL;

    debug("Page allocator: [%p,%p): %u pages free",
        heap_end, RAM_END, free_chunk_list->pagecnt);

    // Allow supervisor to access user memory. We could be more precise by only
    // enabling supervisor access to user memory when we are explicitly trying
    // to access user memory, and disable it at other times. This would catch
    // bugs that cause inadvertent access to user memory (due to bugs).

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}

mtag_t active_mspace(void) { return active_space_mtag(); }

mtag_t switch_mspace(mtag_t mtag) {
    mtag_t prev_mtag;

    prev_mtag = csrrw_satp(mtag);
    sfence_vma();
    
    return prev_mtag;
}

mtag_t clone_active_mspace(void) {
    return ptab_to_mtag(ptab_clone(active_space_ptab()), 0);
}

void reset_active_mspace(void) {
    ptab_reset(active_space_ptab());
    sfence_vma();
}

mtag_t discard_active_mspace(void) {
    struct pte * ptab;

    ptab = active_space_ptab();
    switch_mspace(main_mtag);
    ptab_discard(ptab);
    return main_mtag;
}

// The map_page() function maps a single page into the active address space at
// the specified address. The map_range() function maps a range of contiguous
// pages into the active address space. Note that map_page() is a special case
// of map_range(), so it can be implemented by calling map_range(). Or
// map_range() can be implemented by calling map_page() for each page in the
// range. The current implementation does the latter.

// We currently map 4K pages only. At some point it may be disirable to support
// mapping megapages and gigapages.

void * map_page(uintptr_t vma, void * pp, int rwxug_flags) {
    // assert (vma % PAGE_SIZE == 0);

    ptab_insert(active_space_ptab(), VPN(vma), pp, rwxug_flags);
    return (void*)vma;
}

void * map_range(uintptr_t vma, size_t size, void * pp, int rwxug_flags) {
    uintptr_t const vma_start = vma;

    assert (vma % PAGE_SIZE == 0);
    assert (size % PAGE_SIZE == 0);

    while (size != 0) {
        map_page(vma, pp, rwxug_flags);
        vma += PAGE_SIZE;
        pp += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    return (void*)vma_start;
}

void * alloc_and_map_range(uintptr_t vma, size_t size, int rwxug_flags) {
    struct pte * ptab;
    unsigned long vpn;

    assert ((vma % PAGE_SIZE) == 0);
    size = ROUND_UP(size, PAGE_SIZE);
    ptab = active_space_ptab();

    for (vpn = VPN(vma); vpn < VPN(vma+size); vpn++)
        ptab_insert(ptab, vpn, alloc_phys_page(), rwxug_flags);

    return (void*)vma;
}

void set_range_flags(const void * vp, size_t size, int rwxug_flags) {
    uintptr_t const vma = (uintptr_t)vp;
    struct pte * root;
    unsigned long vpn;
    
    assert ((vma % PAGE_SIZE) == 0);
    size = ROUND_UP(size, PAGE_SIZE);
    root = active_space_ptab();

    for (vpn = VPN(vma); vpn < VPN(vma+size); vpn++)
        ptab_adjust(root, vpn, rwxug_flags);
}

void unmap_and_free_range(void * vp, size_t size) {
    uintptr_t const vma = (uintptr_t)vp;
    struct pte * ptab;
    unsigned long vpn;
    void * pp;
    
    assert ((vma % PAGE_SIZE) == 0);
    size = ROUND_UP(size, PAGE_SIZE);
    ptab = active_space_ptab();

    for (vpn = VPN(vma); vpn < VPN(vma+size); vpn++) {
        pp = ptab_remove(ptab, vpn);
        free_phys_page(pp); // ok if null
    }
}

int validate_vptr(const void * vp, size_t len, int rwxu_flags) {
    uintptr_t const vma = (uintptr_t)vp;
    struct pte * ptab;
    unsigned long vpn;
    struct pte * pte;

    if (len == 0)
        return 0;

    // Check if pointer is well-formed and region does not wrap around zero

    if (vp == NULL || !wellformed(vma) || vma + len < vma)
        return -EINVAL;
    
    ptab = active_space_ptab();

    for (vpn = VPN(vma); vpn <= VPN(vma+len-1); vpn++) {
        pte = ptab_fetch(ptab, vpn);
        if (pte == NULL || !PTE_VALID(*pte))
            return -EACCESS;
        if ((pte->flags & rwxu_flags) != rwxu_flags)
            return -EACCESS;
    }

    return 0;
}

int validate_vstr(const char * vs, int rug_flags) {
    struct pte * ptab;
    unsigned long vpn;
    struct pte * pte;

    // Check if pointer is well-formed

    if (vs == NULL || !wellformed((uintptr_t)vs))
        return -EINVAL;
    
    ptab = active_space_ptab();

    for (;;) {
        vpn = pagenum(vs);
        pte = ptab_fetch(ptab, vpn);
        if (pte == NULL || !PTE_VALID(*pte))
            return -EACCESS;
        if ((pte->flags & rug_flags) != rug_flags)
            return -EACCESS;
        
        while (VPN((uintptr_t)vs) == vpn) {
            if (*vs == '\0')
                return 0;
            vs += 1;
        }
    }
}

void * alloc_phys_page(void) {
    return alloc_phys_pages(1);
}

void free_phys_page(void * pp) {
    free_phys_pages(pp, 1);
}

void * alloc_phys_pages(unsigned int cnt) {
    struct page_chunk * best = NULL;
    struct page_chunk * * chunkptr;
    struct page_chunk * chunk;

    // Find smallest chunk that fits request. We maintain a pointer to the
    // canonical pointer to the current chunk, and a pointer to the canonical
    // pointer to the best chunk so far. Keeping a pointer to canonincal pointer
    // allows us to remove from the list easily.
   
    chunkptr = &free_chunk_list;

    while ((chunk = * chunkptr) != NULL) {
        if (cnt <= chunk->pagecnt) {
            if (cnt == chunk->pagecnt) {
                // We found a chunk of the exact size we need. Remove it from
                // the list and return it.
                * chunkptr = chunk->next;
                return (void*)chunk;
            }

            // Check if current chunk is smaller than best so far.
            if (best == NULL || chunk->pagecnt < best->pagecnt)
                best = chunk;
        }

        chunkptr = &chunk->next;
    }

    if (best == NULL)
        panic("out of pages");
    
    // Break end off the best chunk we found and return it.

    best->pagecnt -= cnt;
    return (void*)best + best->pagecnt * PAGE_SIZE;
}

void free_phys_pages(void * pp, unsigned int cnt) {
    struct page_chunk * const chunk = pp;

    if (pp == NULL)
        return;

    // Do some checks to make sure this is a page we plausibly allocated
    assert ((void*)_kimg_end < pp && pp < RAM_END);
    assert (((uintptr_t)pp % PAGE_SIZE) == 0);

    chunk->next = free_chunk_list;
    chunk->pagecnt = cnt;
    free_chunk_list = chunk;
}

unsigned long free_phys_page_count(void) {
    const struct page_chunk * chunk;
    unsigned long cnt = 0;

    for (chunk = free_chunk_list; chunk != NULL; chunk = chunk->next)
        cnt += chunk->pagecnt;
    
    return cnt;
}

//This function isn't required and can be removed. We can rename to
//handle_page_fault in general maybe since we only want to handle
//page faults in user range either way.
int handle_smode_page_fault(struct trap_frame * tfr, uintptr_t vma) {
    // ...
    return 0;
}

int handle_umode_page_fault(struct trap_frame * tfr, uintptr_t vma) {
    struct pte * pte;
    void * pp;

    if (UMEM_START_VMA <= vma && vma < UMEM_END_VMA) {
        pte = ptab_fetch(active_space_ptab(), VPN(vma));
        if (pte == NULL || !PTE_VALID(*pte)) {
            pp = alloc_phys_page();
            memset(pp, 0, PAGE_SIZE);
            map_page(vma, pp, PTE_R | PTE_W | PTE_U);
	    restore_interrupts(pie);
            return 1; // handled, restart instruction
        }
    }
    
    return 0; // not handled
}

// INTERNAL FUNCTION DEFINITIONS
//

int _ptab_reset(unsigned int lvl, struct pte * pt, int keep_global) {
    int empty = 1; // subtable contains a mapping
    unsigned int i;
    void * pp;

    for (i = 0; i < PTE_CNT; i++) {
        if (PTE_VALID(pt[i])) {
            if (!PTE_GLOBAL(pt[i])) {
                pp = pageptr(pt[i].ppn);

                if (lvl == 0) {
                    assert ((pt[i].flags & (PTE_W | PTE_X)) != 0);
                    // The if the page is in RAM, return it to the allocator
                    if ((void*)_kimg_end <= pp && pp < RAM_END)
                        free_phys_page(pp);
                } else {
                    assert (!PTE_LEAF(pt[i]));
                    empty &= _ptab_reset(lvl - 1, pp, keep_global);
                }

                pt[i] = null_pte();
            
            } else
                empty &= ~keep_global; //Mark non-empty only if keeping globals
        }
    }

    if (empty)
        free_phys_page(pt);
    
    return empty;
}

void ptab_reset(struct pte * ptab) {
    _ptab_reset(ROOT_LEVEL, ptab, 1);
}

void _ptab_clone(unsigned int lvl, struct pte * dst, struct pte * src) {
    unsigned int i;
    void * pp;

    for (i = 0; i < PTE_CNT; i++) {
        if (PTE_VALID(src[i]) && !PTE_GLOBAL(src[i])) {
            pp = alloc_phys_page();
            memcpy(pp, pageptr(src[i].ppn), PAGE_SIZE);

            if (lvl == 0) {
                assert ((src[i].flags & (PTE_W | PTE_X)) != 0);
                dst[i] = leaf_pte(pp, src[i].flags);
            } else {
                assert (!PTE_LEAF(src[i]));
                dst[i] = ptab_pte(pp, src[i].flags);
                _ptab_clone(lvl - 1, pp, pageptr(src[i].ppn));
            }   
        }
    }
}

struct pte * ptab_clone(struct pte * ptab) {
    struct pte * clone;

    clone = alloc_phys_page();
    memcpy(clone, ptab, PAGE_SIZE);
    _ptab_clone(ROOT_LEVEL, clone, ptab);
    return clone;
}

void ptab_discard(struct pte * ptab) {
    _ptab_reset(ROOT_LEVEL, ptab, 0);
}

void _ptab_insert (
    unsigned int lvl,
    struct pte * pt,
    unsigned long vpn,
    void * pp,
    int rwxug_flags)
{
    unsigned int const i = PT_INDEX(lvl, vpn); // vpn >> (lvl*(PAGE_ORDER - PTE_ORDER));
    struct pte * cpt;

    if (lvl == 0) {
        if (!PTE_VALID(pt[i]))
            pt[i] = leaf_pte(pp, rwxug_flags);
        else
            panic(NULL);
    } else {
        if (!PTE_VALID(pt[i])) {
            cpt = alloc_phys_page();
            memset(cpt, 0, PAGE_SIZE);
            pt[i] = ptab_pte(cpt, 0); // intermediate page tables should NEVER be global
        } else {
            assert (!PTE_LEAF(pt[i]));
	    // You can never clear the global bit later
	    assert ((rwxug_flags & PTE_G) || !PTE_GLOBAL(pt[i]));
            cpt = pageptr(pt[i].ppn);
        }

        _ptab_insert(lvl - 1, cpt, vpn, pp, rwxug_flags);
    }
}

void ptab_insert (
    struct pte * ptab,
    unsigned long vpn,
    void * pp,
    int rwxug_flags)
{
    _ptab_insert(ROOT_LEVEL, ptab, vpn, pp, rwxug_flags);
}

int _ptab_adjust (
    unsigned int lvl,
    struct pte * pt,
    unsigned long vpn,
    int rwxug_flags)
{
    unsigned int const i = PT_INDEX(lvl, vpn); // vpn >> (lvl*(PAGE_ORDER - PTE_ORDER));
    int const M = PTE_R | PTE_W | PTE_X | PTE_U | PTE_G;
    struct pte * cpt;
    int g_flag;

    if (!PTE_VALID(pt[i]))
        return 0;

    if (lvl == 0) {
        assert (PTE_LEAF(pt[i]));
        pt[i].flags = (pt[i].flags & ~M) | (rwxug_flags & M);
    } else {
        assert (!PTE_LEAF(pt[i]));
        cpt = pageptr(pt[i].ppn);
        g_flag = _ptab_adjust(lvl - 1, cpt, vpn, rwxug_flags);
        // Clear G bit if updated flags have G bit clear
        pt[i].flags &= (pt[i].flags & g_flag) | ~PTE_G;
    }

    return pt[i].flags & PTE_G;
}

void ptab_adjust(struct pte * ptab, unsigned long vpn, int rwxug_flags) {
    _ptab_adjust(ROOT_LEVEL, ptab, vpn, rwxug_flags);
}

void * ptab_remove(struct pte * ptab, unsigned long vpn) {
    struct pte * pte;
    void * pp;

    pte = ptab_fetch(ptab, vpn);

    if (pte != NULL && PTE_VALID(*pte)) {
        pp = pageptr(pte->ppn);
        * pte = null_pte();
        return pp;
    } else
        return NULL;
}

struct pte * _ptab_fetch(int lvl, struct pte * pt, unsigned long vpn) {
    unsigned int const i = PT_INDEX(lvl, vpn); // vpn >> (lvl*(PAGE_ORDER - PTE_ORDER));

    if (lvl == 0)
        return pt+i;
    else if (PTE_VALID(pt[i]) && !PTE_LEAF(pt[i]))
        return _ptab_fetch(lvl - 1, pageptr(pt[i].ppn), vpn);
    else
        return NULL;
}

struct pte * ptab_fetch(struct pte * ptab, unsigned long vpn) {
    return _ptab_fetch(2, ptab, vpn);
}

mtag_t active_space_mtag(void) {
    return csrr_satp();
}

static inline mtag_t ptab_to_mtag(struct pte * ptab, unsigned int asid) {
    return (
        ((unsigned long)PAGING_MODE << RISCV_SATP_MODE_shift) |
        ((unsigned long)asid << RISCV_SATP_ASID_shift) |
        pagenum(ptab) << RISCV_SATP_PPN_shift);
}

static inline struct pte * mtag_to_ptab(mtag_t mtag) {
    return (struct pte * )((mtag << 20) >> 8);
}

static inline struct pte * active_space_ptab(void) {
    return mtag_to_ptab(active_space_mtag());
}

static inline void * pageptr(uintptr_t n) {
    return (void*)(n << PAGE_ORDER);
}

static inline unsigned long pagenum(const void * p) {
    return (unsigned long)p >> PAGE_ORDER;
}

static inline int wellformed(uintptr_t vma) {
    // Address bits 63:38 must be all 0 or all 1
    uintptr_t const bits = (intptr_t)vma >> 38;
    return (!bits || !(bits+1));
}

static inline struct pte leaf_pte(const void * pp, uint_fast8_t rwxug_flags) {
    return (struct pte) {
        .flags = rwxug_flags | PTE_A | PTE_D | PTE_V,
        .ppn = pagenum(pp)
    };
}

static inline struct pte ptab_pte(const struct pte * pt, uint_fast8_t g_flag) {
    return (struct pte) {
        .flags = g_flag | PTE_V,
        .ppn = pagenum(pt)
    };
}

static inline struct pte null_pte(void) {
    return (struct pte) { };
}
