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
#include "board/conf.h"
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

// CONSTANTS
//
#define CAT_SIZE 15
const char * art = 
" _\0"
" \\`*-.\0"
"  )  _`-.\0"
" .  : `. .\0"
" : _   '  \\ \0"
" ; *` _.   `*-._\0"
" `-.-'          `-.\0"
"   ;       `       `.\0"
"   :.       .        \\ \0"
"   . \\  .   :   .-'   .\0"
"   '  `+.;  ;  '      :\0"
"   :  '  |    ;       ;-.\0"
"   ; '   : :`-:     _.`* ;\0"
".*' /  .*' ; .*`- +'  `*'\0"
"`*-*   `*-*  `*-*'\0";


// INTERNAL FUNCTION DECLARATIONS
//

#ifndef STUDENT
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
#endif

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

#ifndef STUDENT
static void map_startup(struct mregion* regions, uint32_t size, int rwx_flags);
static void modify_startup(struct mregion* regions, uint32_t size, int rwx_flags);
static void subdivide(struct pte * entry, uintptr_t size);

static void combine(struct mregion* arr, uint32_t size);

static int cmp_mregion(void * a, void * b);
static void heapify(uintptr_t arr, uintptr_t elem_sz, uint32_t n_elem, uint32_t i, int(*cmp)(void * a, void * b));
static void heapSort(uintptr_t arr, uintptr_t elem_sz, uint32_t n_elem, int(*cmp)(void * a, void * b));
static void sort(uintptr_t arr, uintptr_t elem_sz, uint32_t n_elem, int(*cmp)(void * a, void * b));
#endif

static int print_memory_info(struct mregion * mmio_imm, unsigned long mmiocnt,
                             struct mregion * ram_imm, unsigned long ramcnt,
                             struct mregion * resv_imm, unsigned long resvcnt);


// INTERNAL GLOBAL VARIABLES
//

static mtag_t main_mtag;

static struct pte main_pt2[PTE_CNT] __attribute__((section(".bss.pagetable"), aligned(4096)));

#ifdef STUDENT
static struct pte main_pt1_0x80000[PTE_CNT]
__attribute__((section(".bss.pagetable"), aligned(4096)));

static struct pte main_pt0_0x80000[PTE_CNT]
__attribute__((section(".bss.pagetable"), aligned(4096)));

static struct pte main_pt0_0x80001[PTE_CNT]
__attribute__((section(".bss.pagetable"), aligned(4096)));
#endif

static struct page_chunk * free_chunk_list;

// EXPORTED FUNCTION DECLARATIONS
//

#ifdef STUDENT
void memory_init (
    const struct mregion * ram_og, unsigned long ramcnt,
    const struct mregion * mmio_og, unsigned long mmiocnt,
    const struct mregion * resv_og, unsigned long resvcnt)
{

    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;

    // All parameters are used for running on real hardware and printing memory info; 
    // you can ignore them in this simplified memory_init() implementation


    struct mregion mmio[mmiocnt];
    struct mregion ram[ramcnt];
    struct mregion resv[resvcnt+1];

    memcpy(mmio, mmio_og, mmiocnt * sizeof(struct mregion));
    memcpy(ram, ram_og, ramcnt * sizeof(struct mregion));
    memcpy(resv, resv_og, resvcnt * sizeof(struct mregion));

    //The memory print function expects the kernel to be mapped as a reserved
    //region and the array to be sorted. For the qvirt target, the second 
    //reserved region is in high memory, so we move it right one and insert our
    //kernel as the second to last entry.
    resvcnt++;
    resv[resvcnt-1].pma = resv[resvcnt-2].pma;
    resv[resvcnt-1].size = resv[resvcnt-2].size;
    resv[resvcnt-2].pma = 
        ROUND_DOWN((uintptr_t)(void *)_kimg_start, PAGE_SIZE);
    resv[resvcnt-2].size = 
        ROUND_UP((uintptr_t)(void *)_kimg_end, PAGE_SIZE) - 
        ROUND_DOWN((uintptr_t)(void *)_kimg_start, PAGE_SIZE);

    kprintfluffy(40, &art, "");   
    kprintfluffy(40, &art, "System Memory Map:");
    kprintfluffy(40, &art, "");

    int fluffy = print_memory_info(mmio, mmiocnt, ram, ramcnt, resv, resvcnt);

    for(int i = 0; i < CAT_SIZE - 5 - fluffy; i++){ 
        kprintfluffy(40, &art, "");
    }

    void * heap_start;
    void * heap_end;

    uintptr_t pma;
    const void * pp;


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

    // First two physical megaranges of RAM are mapped as individual pages with
    // permissions based on kernel image region.
    //
    // This also means that the kernel must be smaller than 4 MB

    main_pt1_0x80000[VPN1(RAM_START_PMA)] = ptab_pte(main_pt0_0x80000, PTE_G);
    main_pt1_0x80000[VPN1(RAM_START_PMA + MEGA_SIZE)] = ptab_pte(main_pt0_0x80001, PTE_G);

    //Map the reserved region before the kernel
    for (pp = RAM_START; pp < text_start; pp+=PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] = leaf_pte(pp, PTE_R);
    }

    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        assert(PTE_VALID(main_pt1_0x80000[VPN1((uintptr_t)pp)])); //Kernel too big.
        struct pte * pt1 = pageptr(main_pt1_0x80000[VPN1((uintptr_t)pp)].ppn);
        pt1[VPN0((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        assert(PTE_VALID(main_pt1_0x80000[VPN1((uintptr_t)pp)]));
        struct pte * pt1 = pageptr(main_pt1_0x80000[VPN1((uintptr_t)pp)].ppn);
        pt1[VPN0((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_G);
    }

    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        assert(PTE_VALID(main_pt1_0x80000[VPN1((uintptr_t)pp)]));
        struct pte * pt1 = pageptr(main_pt1_0x80000[VPN1((uintptr_t)pp)].ppn);
        pt1[VPN0((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages - dtb area for QEMU

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END - MEGA_SIZE; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    //Map DTB as read only
    main_pt1_0x80000[VPN1((uintptr_t)pp)] = leaf_pte(pp, PTE_R | PTE_G);

    // Enable paging; this part always makes me nervous.

    main_mtag = ptab_to_mtag(main_pt2, 0);
    csrw_satp(main_mtag);
    kprintf("Enabled Address Translation.\n");
    sfence_vma();

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

    heap_init(heap_start, heap_end - heap_start);

    debug("Heap allocator: [%p,%p): %zu KB free", heap_start, heap_end,
            (heap_end - heap_start) / 1024);

    debug("Heap allocator: [%p,%p): %zu KB free",
            heap_start, heap_end, (heap_end - heap_start) / 1024);

#ifndef STUDENT
    // YOUR CODE HERE
    // Initialize free chunk list
#else
    free_chunk_list = heap_end; // heap_end is page aligned
    free_chunk_list->pagecnt = (RAM_SIZE - (heap_end - RAM_START) - MEGA_SIZE) / PAGE_SIZE;
    free_chunk_list->next = NULL;
#endif
    
    kprintfluffy(40, &art, "Free Chunk List Initialized!");
    kprintfluffy(40, &art, "%d Free Pages.", free_phys_page_count());
    kprintfluffy(40, &art, "");

    debug("Page allocator: [%p,%p): %u pages free",
            heap_end, RAM_END, free_chunk_list->pagecnt);

    // Allow supervisor to access user memory. We could be more precise by only
    // enabling supervisor access to user memory when we are explicitly trying
    // to access user memory, and disable it at other times. This would catch
    // bugs that cause inadvertent access to user memory (due to bugs).

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}
#else
void memory_init (
    const struct mregion * ram_og, unsigned long ramcnt,
    const struct mregion * mmio_og, unsigned long mmiocnt,
    const struct mregion * resv_og, unsigned long resvcnt)
{
    
    struct mregion kernel_text = {
        .pma = (uintptr_t)_kimg_text_start,
        .size = (uintptr_t)_kimg_text_end - (uintptr_t)_kimg_text_start
    };
    struct mregion kernel_rodata = {
        .pma = (uintptr_t)_kimg_rodata_start,
        .size = (uintptr_t)_kimg_rodata_end - (uintptr_t)_kimg_rodata_start
    };
    struct mregion kernel_data = {
        .pma = (uintptr_t)_kimg_data_start,
        .size = (uintptr_t)_kimg_end - (uintptr_t)_kimg_data_start
    };

    //Copy mregions to not destroy them
   
    struct mregion mmio[mmiocnt];
    struct mregion ram[ramcnt];
    struct mregion resv[resvcnt+1];

    memcpy(mmio, mmio_og, mmiocnt * sizeof(struct mregion));
    memcpy(ram, ram_og, ramcnt * sizeof(struct mregion));
    memcpy(resv, resv_og, resvcnt * sizeof(struct mregion));

    trace("%s()", __func__);

    //If there is no RAM, we fail. (Who's hopes and dreams did we load the kernel into and start the stack on???)
    assert (ramcnt > 0);

    //Add the kernel as a reserved region - we'll set up permissions later, this is to initialize the chunk list
    resvcnt += 1;

    //Probably don't need to check this because elf files are guaranteed to be page aligned but I do it to be safe anyway.
    resv[resvcnt-1].pma = 
        ROUND_DOWN((uintptr_t)(void *)_kimg_start, PAGE_SIZE);
    resv[resvcnt-1].size = 
        ROUND_UP((uintptr_t)(void *)_kimg_end, PAGE_SIZE) - 
        ROUND_DOWN((uintptr_t)(void *)_kimg_start, PAGE_SIZE);

    //Sort to avoid O(n^2) while initializing the free chunk list.
    sort((uintptr_t)(void *)resv, sizeof(struct mregion), resvcnt, cmp_mregion);
    sort((uintptr_t)(void *)ram, sizeof(struct mregion), ramcnt, cmp_mregion);
    sort((uintptr_t)(void *)mmio, sizeof(struct mregion), mmiocnt, cmp_mregion);

    //Merge overlapping regions and null out entries that have been subsumed
    combine(resv, resvcnt);
    combine(ram, ramcnt);
    combine(mmio, mmiocnt);

    kprintfluffy(40, &art, "");   
    kprintfluffy(40, &art, "System Memory Map:");
    kprintfluffy(40, &art, "");

    int fluffy = print_memory_info(mmio, mmiocnt, ram, ramcnt, resv, resvcnt);
    for(int i = 0; i < CAT_SIZE - 5 - fluffy; i++){ 
        kprintfluffy(40, &art, "");
    }

    //Initialize free_chunk_list to start of RAM.
    free_chunk_list = NULL;
    struct page_chunk** curr = &free_chunk_list;
    uintptr_t chunk_start = ram->pma; //first entry guaranteed to be non-NULL

    //Build free_chunk_list
    //ASSUME: Resv is a strict subset of RAM
    for (int ram_idx = 0, resv_idx = 0; ram_idx < ramcnt;) {
        unsigned long sz = 0;
        uintptr_t start = chunk_start;

        while (resv[resv_idx].size == 0x0 && resv_idx < resvcnt)
            resv_idx++; //Ignore NULLs

        if (ram[ram_idx].pma + ram[ram_idx].size < resv[resv_idx].pma || resv_idx >= resvcnt) {
            sz = ram[ram_idx].pma + ram[ram_idx].size - start;
            ram_idx++;
            while (ram[ram_idx].size == 0x0 && ram_idx <= ramcnt)
                ram_idx++; //Ignore NULLs
            chunk_start = ram[ram_idx].pma;
        }
        else {
            sz = resv[resv_idx].pma - start;
            chunk_start = resv[resv_idx].pma + resv[resv_idx].size;
            resv_idx++;
        }

        if (sz > 0) { //Don't want to add to linkedlist unless we actually have a chunk
            (*curr) = (void *)start;
            (*curr)->next = NULL;
            (*curr)->pagecnt = sz/PAGE_SIZE;
            curr = &((*curr)->next); //curr = *curr should be ok too but that breaks typing
        }
    }

    //If everything is reserved, fail. (Why does this look like a load access fault?)
    assert(free_chunk_list != NULL);

    kprintfluffy(40, &art, "Free Chunk List Initialized!");
    kprintfluffy(40, &art, "%d Free Pages.", free_phys_page_count());
    kprintfluffy(40, &art, "");

    //Redundant, but clear the main table.
    memset(main_pt2, 0, 4096);

    //Map MMIO
    map_startup(mmio, mmiocnt, PTE_R | PTE_W);

    //Map RAM (R/W)
    map_startup(ram, ramcnt, PTE_R | PTE_W);

    //Map Reserved regions (R)
    modify_startup(resv, resvcnt, PTE_R);

    //Map Kernel (Text: RX, Rodata: R, Data: RW)
    modify_startup(&kernel_text, 1, PTE_R | PTE_X);
    modify_startup(&kernel_rodata, 1, PTE_R);
    modify_startup(&kernel_data, 1, PTE_R | PTE_W);

    // Enable paging; this part always makes me nervous.

    main_mtag = ptab_to_mtag(main_pt2, 0);
    csrw_satp(main_mtag);
    kprintf("Enabled Address Translation.\n");
    sfence_vma();

    //Since our RAM can be non-contiguous I just initialize with no grant and let the heap allocate
    //for itself later
    heap_init(NULL, 0);

    // Allow supervisor to access user memory. We could be more precise by only
    // enabling supervisor access to user memory when we are explicitly trying
    // to access user memory, and disable it at other times. This would catch
    // bugs that cause inadvertent access to user memory (due to bugs).

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}

static void map_startup(struct mregion* regions, uint32_t size, int rwx_flags) {
    for (int i = 0; i < size; i++) {
        uintptr_t pp = regions[i].pma;
        uintptr_t end = regions[i].pma + regions[i].size;
        while (pp < end) {
            if (ROUND_UP(pp, GIGA_SIZE) == pp && pp + GIGA_SIZE <= end) {
                main_pt2[VPN2(pp)] = leaf_pte((void *)pp, rwx_flags | PTE_G);
                pp+=GIGA_SIZE;
                continue;
            }
            if (!PTE_VALID(main_pt2[VPN2(pp)])) {
                //Allocate gigapage if we need to place megapage inside
                void * ptr = alloc_phys_page();
                memset(ptr, 0, PAGE_SIZE);
                main_pt2[VPN2(pp)] = ptab_pte(ptr, 0); //Not a global page
            }
            struct pte * pt1 = pageptr(main_pt2[VPN2(pp)].ppn);
            if (ROUND_UP(pp, MEGA_SIZE) == pp && pp + MEGA_SIZE <= end) {
                pt1[VPN1(pp)] = leaf_pte((void *)pp, rwx_flags | PTE_G);
                pp+=MEGA_SIZE;
                continue;
            }
            if (!PTE_VALID(pt1[VPN1(pp)])) {
                //Allocate megapage if we need to place page inside
                void * ptr = alloc_phys_page();
                memset(ptr, 0, PAGE_SIZE);
                pt1[VPN1(pp)] = ptab_pte(ptr, 0); //Not a global page
            }
            struct pte * pt0 = pageptr(main_pt2[VPN1(pp)].ppn);
            pt0[VPN0(pp)] = leaf_pte((void *)pp, rwx_flags | PTE_G);
            pp+=PAGE_SIZE;
        }
    }
}

static void modify_startup(struct mregion* regions, uint32_t size, int rwx_flags) {
    for (int i = 0; i < size; i++) {
        uintptr_t pp = regions[i].pma;
        uintptr_t end = regions[i].pma + regions[i].size;
        while (pp < end) {
            if (ROUND_UP(pp, GIGA_SIZE) == pp && pp + GIGA_SIZE <= end && PTE_LEAF(main_pt2[VPN2(pp)])) {
                main_pt2[VPN2(pp)] = leaf_pte((void *)pp, rwx_flags | PTE_G);
                pp+=GIGA_SIZE;
                continue;
            }
            //If it's a leaf, we need to subdivide so we can generate more granular modifications
            if (PTE_LEAF(main_pt2[VPN2(pp)]))subdivide(main_pt2 + VPN2(pp), MEGA_SIZE);
            struct pte * pt1 = pageptr(main_pt2[VPN2(pp)].ppn);
            if (ROUND_UP(pp, MEGA_SIZE) == pp && pp + MEGA_SIZE <= end && PTE_LEAF(pt1[VPN1(pp)])) {
                pt1[VPN1(pp)] = leaf_pte((void *)pp, rwx_flags | PTE_G);
                pp+=MEGA_SIZE;
                continue;
            }
            if (PTE_LEAF(pt1[VPN1(pp)]))subdivide(pt1 + VPN1(pp), PAGE_SIZE);
            struct pte * pt0 = pageptr(pt1[VPN1(pp)].ppn);
            pt0[VPN0(pp)] = leaf_pte((void *)pp, rwx_flags | PTE_G);
            pp+=PAGE_SIZE;
        }
    }
}

static void subdivide(struct pte * entry, uintptr_t size) {
    assert(PTE_LEAF(*entry));
    struct pte * internal_page = alloc_phys_page();
    memset((void *)internal_page, 0, PAGE_SIZE);
    uintptr_t pp = (uintptr_t)pageptr(entry->ppn);
    int flags = entry->flags;
    *entry = ptab_pte(internal_page, flags & PTE_G);
    for (int i = 0; i < PAGE_SIZE/sizeof(struct pte); i++) {
        internal_page[i] = leaf_pte((void *)pp, flags & (PTE_R | PTE_W | PTE_X | PTE_U | PTE_G));
        pp+=size;
    }
}

static void combine(struct mregion* arr, uint32_t size) {
    for (uint32_t curr = 0, nxt = 1; nxt < size; nxt++) {
        if (arr[curr].pma + arr[curr].size >= arr[nxt].pma) {
            if (arr[nxt].pma + arr[nxt].size > arr[curr].pma + arr[curr].size)
                arr[curr].size+=(arr[nxt].pma + arr[nxt].size - (arr[curr].pma + arr[curr].size));
            //Indicate to ignore this.
            arr[nxt].pma = 0x0;
            arr[nxt].size = 0x0;
        }else curr = nxt;
    }
}

static int cmp_mregion(void * a, void * b) {
    if (((struct mregion*)a)->pma > ((struct mregion*)b)->pma)
        return 1;
    if (((struct mregion*)a)->pma < ((struct mregion*)b)->pma)
        return -1;
    if (((struct mregion*)a)->size > ((struct mregion*)b)->size)
        return 1;
    if (((struct mregion*)a)->size < ((struct mregion*)b)->size)
        return -1;
    return 0;
}

//TODO Everything below this related to init should really go in its own file.
static void heapify(uintptr_t arr, uintptr_t elem_sz, uint32_t n_elem, uint32_t i, int(*cmp)(void * a, void * b)) {
    uint32_t largest = i;
    uint32_t l = 2 * i + 1; //Left index
    uint32_t r = 2 * i + 2; //Right index

    // Left child > root
    if (l < n_elem && cmp((void *)(arr + l * elem_sz), (void *)(arr + largest * elem_sz)) > 0)
        largest = l;

    // Right child > largest
    if (r < n_elem && cmp((void *)(arr + r * elem_sz), (void *)(arr + largest * elem_sz)) > 0)
        largest = r;

    // If largest is not root
    if (largest != i) {
        //This is a swap operation, but we don't know the element size
        //so we have to allocate our buffer dynamically in bytes
        //and memcpy everything over instead of just being able to
        //use assignment operators
        char temp[elem_sz];
        memcpy((void *)temp, (void *)(arr + i*elem_sz), elem_sz);
        memcpy((void *)(arr + i*elem_sz), (void *)(arr + largest*elem_sz), elem_sz);
        memcpy((void *)(arr + largest*elem_sz), (void *)temp, elem_sz);

        // Recursively heapify the affected sub-tree
        heapify(arr, elem_sz, n_elem, largest, cmp);
    }
}

static void heapSort(uintptr_t arr, uintptr_t elem_sz, uint32_t n_elem, int(*cmp)(void * a, void * b)) {
    // Build heap (rearrange vector)
    for (int i = n_elem / 2 - 1; i >= 0; i--)
        heapify(arr, elem_sz, n_elem, i, cmp);

    // One by one extract an element from heap
    for (int i = n_elem - 1; i > 0; i--) {
        // Move current root to end
        char temp[elem_sz];
        memcpy((void *)temp, (void *)arr, elem_sz);
        memcpy((void *)arr, (void *)(arr + i*elem_sz), elem_sz);
        memcpy((void *)(arr + i*elem_sz), (void *)temp, elem_sz);

        // Call max heapify on the reduced heap
        heapify(arr, elem_sz, i, 0, cmp);
    }
}

//Cmp should return positive if a > b, negative if a < b, 0 if equal
//this is heapsort. Maybe switch to blocksort?
static void sort(uintptr_t arr, uintptr_t elem_sz, uint32_t n_elem, int(*cmp)(void * a, void * b)) {
    heapSort(arr, elem_sz, n_elem, cmp);
}
#endif

mtag_t active_mspace(void) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    return active_space_mtag();
#endif
}

mtag_t switch_mspace(mtag_t mtag) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    mtag_t prev_mtag;

    prev_mtag = csrrw_satp(mtag);
    sfence_vma();

    return prev_mtag;
#endif
}

mtag_t clone_active_mspace(void) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    return ptab_to_mtag(ptab_clone(active_space_ptab()), 0);
#endif
}

void reset_active_mspace(void) {
#ifdef STUDENT
    // YOUR CODE HERE
    return;
#else
    ptab_reset(active_space_ptab());
    sfence_vma();
#endif
}

mtag_t discard_active_mspace(void) {
#ifdef STUDENT
    //FIXME Your code here
    return 0;
#else
    struct pte * ptab;

    ptab = active_space_ptab();
    switch_mspace(main_mtag);
    ptab_discard(ptab);
    return main_mtag;
#endif
}

// The map_page() function maps a single page into the active address space at
// the specified address. The map_range() function maps a range of contiguous
// pages into the active address space. Note that map_page() is a special case
// of map_range(), so it can be implemented by calling map_range(). Or
// map_range() can be implemented by calling map_page() for each page in the
// range. The current implementation does the latter.

// We currently map 4K pages only. At some point it may be desirable to support
// mapping megapages and gigapages.

void * map_page(uintptr_t vma, void * pp, int rwxug_flags) {
#ifdef STUDENT
    // YOUR CODE HERE
    return NULL;
#else
    ptab_insert(active_space_ptab(), VPN(vma), pp, rwxug_flags);
    sfence_vma();
    return (void *)vma;
#endif
}

void * map_range(uintptr_t vma, size_t size, void * pp, int rwxug_flags) {
#ifdef STUDENT
    // YOUR CODE HERE
    return NULL;
#else
    uintptr_t const vma_start = vma;

    assert (vma % PAGE_SIZE == 0);
    assert (size % PAGE_SIZE == 0);

    while (size != 0) {
        map_page(vma, pp, rwxug_flags);
        vma += PAGE_SIZE;
        pp += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    sfence_vma();
    return (void *)vma_start;
#endif
}

void * alloc_and_map_range(uintptr_t vma, size_t size, int rwxug_flags) {
#ifdef STUDENT
    // YOUR CODE HERE
    return NULL;
#else
    struct pte * ptab;
    unsigned long vpn;

    assert ((vma % PAGE_SIZE) == 0);
    size = ROUND_UP(size, PAGE_SIZE);
    ptab = active_space_ptab();

    for (vpn = VPN(vma); vpn < VPN(vma+size); vpn++)
        ptab_insert(ptab, vpn, alloc_phys_page(), rwxug_flags);
    sfence_vma();
    return (void *)vma;
#endif
}

void set_range_flags(const void * vp, size_t size, int rwxug_flags) {
#ifdef STUDENT
    // YOUR CODE HERE
    return;
#else
    uintptr_t const vma = (uintptr_t)vp;
    struct pte * root;
    unsigned long vpn;

    assert ((vma % PAGE_SIZE) == 0);
    size = ROUND_UP(size, PAGE_SIZE);
    root = active_space_ptab();

    for (vpn = VPN(vma); vpn < VPN(vma+size); vpn++)
        ptab_adjust(root, vpn, rwxug_flags);
    sfence_vma();
#endif
}

void unmap_and_free_range(void * vp, size_t size) {
#ifdef STUDENT
    // YOUR CODE HERE
    return;
#else
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
    sfence_vma();
#endif
}

int enforce_vptr(const void * vp, size_t size, int rwxug_flags) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    uintptr_t const vma = (uintptr_t)vp;
    struct pte * ptab;
    unsigned long vpn;
    struct pte * pte;
    void * pp;

    if (size == 0)
        return 0;

    // Check if pointer is well-formed and region does not wrap around zero

    if (vp == NULL || !wellformed(vma) || vma + size < vma || vma < UMEM_START_VMA || vma + size > UMEM_END_VMA)
        return -EINVAL;

    ptab = active_space_ptab();

    for (vpn = VPN(vma); vpn <= VPN(vma+size-1); vpn++) {
        pte = ptab_fetch(ptab, vpn);

        if ((pte == NULL || !PTE_VALID((*pte))) && !(rwxug_flags & PTE_X)) { //If the page is invalid and we don't care about execute

            if (free_phys_page_count() < 3)
                return 0;

            pp = alloc_phys_page();
            memset(pp, 0, PAGE_SIZE);
            //We only automap user mappings.
            map_page((uintptr_t)pageptr(vpn), pp, PTE_R | PTE_W | PTE_U);
            pte = ptab_fetch(ptab, vpn);
        }
    
        if ((pte->flags & rwxug_flags) != rwxug_flags)
            return -EACCESS;
    }
    return 0;
#endif
}

int validate_vstr(const char * vs, int rwxug_flags) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
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
        if ((pte->flags & rwxug_flags) != rwxug_flags)
            return -EACCESS;

        while (VPN((uintptr_t)vs) == vpn) {
            if (*vs == '\0')
                return 0;
            vs += 1;
        }
    }
#endif
}

void * alloc_phys_page(void) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    return alloc_phys_pages(1);
#endif
}

void free_phys_page(void * pp) {
#ifdef STUDENT
    // YOUR CODE HERE
    return;
#else
    free_phys_pages(pp, 1);
#endif
}

void * alloc_phys_pages(unsigned int cnt) {
#ifdef STUDENT
    // YOUR CODE HERE
    return NULL;
#else
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
                return (void *)chunk;
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
    return (void *)best + best->pagecnt * PAGE_SIZE;
#endif
}

void free_phys_pages(void * pp, unsigned int cnt) {
#ifdef STUDENT
    // YOUR CODE HERE
    return;
#else
    struct page_chunk * const chunk = pp;

    if (pp == NULL)
        return;

    // The page can be anywhere, so the checks are obsolete
    assert (((uintptr_t)pp % PAGE_SIZE) == 0);

    chunk->next = free_chunk_list;
    chunk->pagecnt = cnt;
    free_chunk_list = chunk;
#endif
}

unsigned long free_phys_page_count(void) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    const struct page_chunk * chunk;
    unsigned long cnt = 0;

    for (chunk = free_chunk_list; chunk != NULL; chunk = chunk->next)
        cnt += chunk->pagecnt;

    return cnt;
#endif
}

int handle_umode_page_fault(struct trap_frame * tfr, uintptr_t vma) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    struct pte * pte;
    void * pp;

    if (UMEM_START_VMA <= vma && vma < UMEM_END_VMA) {
        pte = ptab_fetch(active_space_ptab(), VPN(vma));
        if (pte == NULL || !PTE_VALID(*pte)) {

            //If there is a possible OOM, just don't handle the page fault.
            if (free_phys_page_count() < 3)
                return 0;

            pp = alloc_phys_page();
            memset(pp, 0, PAGE_SIZE);
            map_page(vma, pp, PTE_R | PTE_W | PTE_U);
            return 1; // handled, restart instruction
        }
    }

    return 0; // not handled
#endif
}

// INTERNAL FUNCTION DEFINITIONS
//

#ifndef STUDENT
int _ptab_reset(unsigned int lvl, struct pte * pt, int keep_global) {
    int empty = 1; // subtable contains a mapping
    unsigned int i;
    void * pp;

    for (i = 0; i < PTE_CNT; i++) {
        if (PTE_VALID(pt[i])) {
            if (!PTE_GLOBAL(pt[i])) {
                pp = pageptr(pt[i].ppn);

                if (lvl == 0) {
                    assert ((pt[i].flags & (PTE_R | PTE_W | PTE_X)) != 0);
                    free_phys_page(pp);
                    pt[i] = null_pte();
                } else {
                    assert (!PTE_LEAF(pt[i]));
                    int entry_empty = _ptab_reset(lvl - 1, pp, keep_global);
                    if (entry_empty)pt[i] = null_pte(); //Remove the mapping if
                                        //it's empty
                    empty &= entry_empty;
                }
            } else
                empty = !(keep_global); //Mark non-empty only if keeping globals
        }
    }
    if (empty) {
        free_phys_page(pt);
    }
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
#endif

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
    return (void *)(n << PAGE_ORDER);
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

//This function assumes:
//- resv strictly overlaps with RAM
//- MMIO and RAM do not overlap
//- Arrays are sorted
//- 0 size entries are invalid
//- That I'm very sorry to whoever has to read this.
//
// * THIS FUNCTION IS DESTRUCTIVE TO MREGION ARRAYS.
//
//...returns how many times kprintfluffy was called :innocent:
static int print_memory_info(struct mregion * mmio_imm, unsigned long mmiocnt,
                             struct mregion * ram_imm, unsigned long ramcnt,
                             struct mregion * resv_imm, unsigned long resvcnt) {
    unsigned long mmio_idx = 0;
    unsigned long ram_idx = 0;
    unsigned long resv_idx = 0;

    int fluffy = 0;

    //The first thing we do is copy out the arguments to prevent destruction of
    //the arguments.

    struct mregion mmio[mmiocnt];
    struct mregion ram[ramcnt];
    struct mregion resv[resvcnt];

    memcpy(mmio, mmio_imm, mmiocnt * sizeof(struct mregion));
    memcpy(ram, ram_imm, ramcnt * sizeof(struct mregion));
    memcpy(resv, resv_imm, resvcnt * sizeof(struct mregion));

    while (mmio_idx < mmiocnt || ram_idx < ramcnt) {
        uintptr_t mmio_loc = mmio[mmio_idx].pma;
        uintptr_t ram_loc = ram[ram_idx].pma;
        uintptr_t resv_loc = resv[resv_idx].pma;
        
        if (mmio_loc < ram_loc && mmio_idx < mmiocnt) {
            kprintfluffy(40, &art, "%08p - %08p - MMIO", 
                         mmio_loc, 
                         mmio_loc + mmio[mmio_idx].size - 1);
            fluffy++;
            mmio_idx++;
        } else {

            if (ram_loc <= resv_loc && 
               ram_loc + ram[ram_idx].size > resv_loc && resv_idx < resvcnt) {

                if (ram_loc < resv_loc) {
                    kprintfluffy(40, 
                                 &art, 
                                 "%p - %p - Free RAM", 
                                 ram_loc, 
                                 resv_loc - 1);
                    ram[ram_idx].size -= resv_loc - ram_loc;
                    ram[ram_idx].pma = resv_loc;
                    fluffy++;
                } else {

                    if (resv_loc <= (uintptr_t)_kimg_start && 
                        resv_loc + resv[resv_idx].size > (uintptr_t)_kimg_start) {

                        if (resv_loc < (uintptr_t)_kimg_start) {
                            kprintfluffy(40, 
                                         &art, 
                                         "%p - %p - SBI Reserved", 
                                         resv_loc, 
                                         (uintptr_t)_kimg_start - 1);

                            resv[resv_idx].size -= 
                                (uintptr_t)_kimg_start - resv_loc;
                            resv[resv_idx].pma = (uintptr_t)_kimg_start;
                            ram[ram_idx].pma = (uintptr_t)_kimg_start;
                            ram[ram_idx].size -= 
                                (uintptr_t)_kimg_start - resv_loc;
                            fluffy++;
                        } else {
                            kprintfluffy(40, 
                                         &art, 
                                         "%p - %p - Kernel TEXT", 
                                         _kimg_text_start, 
                                         ROUND_UP((uintptr_t)_kimg_text_end, 
                                                  PAGE_SIZE) - 1);

                            kprintfluffy(40, 
                                         &art, 
                                         "%p - %p - Kernel RODATA", 
                                         _kimg_rodata_start, 
                                         ROUND_UP((uintptr_t)_kimg_rodata_end, 
                                                  PAGE_SIZE) - 1);

                            kprintfluffy(40, 
                                         &art, 
                                         "%p - %p - Kernel DATA", 
                                         _kimg_data_start, 
                                         ROUND_UP((uintptr_t)_kimg_data_end, 
                                                  PAGE_SIZE) - 1);

                            resv[resv_idx].pma = 
                                ROUND_UP((uintptr_t)_kimg_end, PAGE_SIZE);
                            resv[resv_idx].size -= 
                                ROUND_UP((uintptr_t)_kimg_end, PAGE_SIZE) - resv_loc;
                            ram[ram_idx].pma = 
                                ROUND_UP((uintptr_t)_kimg_end, PAGE_SIZE);
                            ram[ram_idx].size -= 
                                ROUND_UP((uintptr_t)_kimg_end, PAGE_SIZE) - resv_loc;
                            fluffy+=3;
                        }
                    } else {
                        kprintfluffy(40, 
                                     &art, 
                                     "%p - %p - SBI Reserved", 
                                     resv_loc, 
                                     resv_loc + resv[resv_idx].size - 1);
                        ram[ram_idx].pma = resv_loc + resv[resv_idx].size;
                        ram[ram_idx].size -= resv[resv_idx].size;
                        resv_idx++;
                        fluffy++;
                    }
                }
            } else {
                kprintfluffy(40, 
                             &art, 
                             "%p - %p - Free RAM", 
                             ram_loc, 
                             ram_loc + ram[ram_idx].size - 1);
                fluffy++;
                ram_idx++;
            }
        }

        //Ignore NULL entries (don't need to do this first because first entries 
        //are guaranteed to be populated)
        while(mmio[mmio_idx].size == 0 && mmio_idx < mmiocnt)
                mmio_idx++;
        while(ram[ram_idx].size == 0 && ram_idx < ramcnt)
                ram_idx++;
        while(resv[resv_idx].size == 0 && resv_idx < resvcnt)
                resv_idx++;

    }
    return fluffy;
}
