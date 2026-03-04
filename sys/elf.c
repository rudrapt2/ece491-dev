// elf.c - ELF executable loader
//
// Copyright (c) 2024-2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef ELF_TRACE
#define TRACE
#endif

#ifdef ELF_DEBUG
#define DEBUG
#endif

#include "elf.h"
#include "conf.h"
#include "io.h"
#include "string.h"
#include "memory.h"
#include "error.h"
#include "misc.h"

#include <stdint.h>

// Offsets into e_ident

#define EI_CLASS        4   
#define EI_DATA         5
#define EI_VERSION      6
#define EI_OSABI        7
#define EI_ABIVERSION   8   
#define EI_PAD          9  


// ELF header e_ident[EI_CLASS] values

#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

// ELF header e_ident[EI_DATA] values

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

// ELF header e_ident[EI_VERSION] values

#define EV_NONE     0
#define EV_CURRENT  1

// ELF header e_type values

enum elf_et {
    ET_NONE = 0,
    ET_REL,
    ET_EXEC,
    ET_DYN,
    ET_CORE
};

// The /elf64_ehdr/ structure.
// ELF header struct
struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff; 
    uint32_t e_flags; 
    uint16_t e_ehsize; 
    uint16_t e_phentsize; 
    uint16_t e_phnum; 
    uint16_t e_shentsize; 
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};


// The /elf_pt/ enumeration.
// Program header p_type values
enum elf_pt {
	PT_NULL = 0, 
	PT_LOAD,
	PT_DYNAMIC,
	PT_INTERP,
	PT_NOTE,
	PT_SHLIB,
	PT_PHDR,
	PT_TLS
};

// Program header p_flags bits

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

// The /elf64_phdr/ structure.
// Program header struct
struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

// ELF header e_machine values (short list)

#define  EM_RISCV   243

int elf_load(struct io * io, void (**eptr)(void)) {
#ifdef STUDENT
    // YOUR CODE HERE
    return 0;
#else
    static const uint32_t MAGIC_LSB = 0x464c457f;
    struct elf64_ehdr ehdr;
    struct elf64_phdr phdr;
    uint_fast16_t phidx;
    unsigned long long size;
    unsigned long long pos;
    long result;
    int entry_ok = 0;
#ifndef MP3CP1
    int pte_flags;
    size_t memsz;
#endif

    trace("%s(elfio=%p,eptr=%p)", __func__, io, eptr);

    // Get ELF file length

    result = ioctl(io, IOC_GETEND, &size);

    if (result != 0)
        return result;
    
    if (size < sizeof(struct elf64_ehdr))
        return -EBADFMT;
    
    // Read in ELF header

    result = iofetch(io, 0, &ehdr, sizeof(ehdr));

    if (result != sizeof(ehdr)) {
        if (result < 0)
            return result;
        return -EIO;
    }

    // Check ident bytes for expected values

    if (*(uint32_t*)ehdr.e_ident != MAGIC_LSB)
        return -EBADFMT;

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
        return -EBADFMT;

    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
        return -EBADFMT;

    if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
        return -EBADFMT;
    
    // Check rest of header

    if (ehdr.e_type != ET_EXEC || ehdr.e_machine != EM_RISCV) {
        debug("ELF file is not a RISC-V executable");
        return -EBADFMT;
    }

    if (ehdr.e_phentsize < sizeof(struct elf64_phdr)) {
        debug("e_phentsize is too small (%lu expected, %d found)",
            sizeof(struct elf64_phdr), ehdr.e_phentsize);
        return -EBADFMT;
    }

    if (size < ehdr.e_phoff) {
        debug("e_phoff past eof");
        return -EBADFMT;
    }

    if (size - ehdr.e_phoff < (uint64_t)ehdr.e_phnum * ehdr.e_phentsize) {
        debug("program header past eof");
        return -EBADFMT;
    }

    if (ehdr.e_phnum == 0) {
        debug("ELF file contains no program header entries");
        return -EBADFMT;
    }

    // Go through program header entries and load those that are of type PT_LOAD
    // into memory. We call alloc_and_map_range() to allocate and map the
    // requested virtual memory region. Ideally, if something goes wrong, we
    // should unmap and free any regions we mapped. However, since we are called
    // from exec after the process memory space has been reset, any errors here
    // result in process termination, so the fact that we did not clean up does
    // not matter.

    for (phidx = 0; phidx < ehdr.e_phnum; phidx++) {
        pos = ehdr.e_phoff + (uint64_t)phidx * ehdr.e_phentsize;
        
        result = iofetch(io, pos, &phdr, sizeof(phdr));

        if (result != sizeof(phdr)) {
            if (result < 0)
                return result;
            else
                return -EIO;
        }
        
        if (phdr.p_type != PT_LOAD) {
            debug("PH[%d] Skipping program header with p_type = 0x%x",
                phidx, phdr.p_type);
            continue;
        }

        if (phdr.p_memsz == 0) {
            debug("PH[%d] Skipping zero-size program header");
            continue;
        }

        if (phdr.p_memsz < phdr.p_filesz) {
            debug("p_filesz larger than p_memsz");
            return -EBADFMT;
        }

        // Alignment must be a multiple of PAGE_SIZE

        if (phdr.p_vaddr % phdr.p_align) {
            debug("p_vaddr not %lu-byte aligned (p_align)", phdr.p_align);
            return -EBADFMT;
        }

        // Check flags. All regions should be readable.

        if ((phdr.p_flags & PF_R) == 0 && (phdr.p_flags & PF_X) == 0) {
            debug("Program region with neither PF_R nor PF_X flags set");
            return -EBADFMT;
        }
        
        // Check if entry is in current region and the region is executable.

        if (phdr.p_vaddr <= ehdr.e_entry &&
            ehdr.e_entry < phdr.p_vaddr + phdr.p_memsz)
        {
            if ((phdr.p_flags & PF_X) == 0) {
                debug("e_entry not in executable region");
                return -EBADFMT;
            }

            entry_ok = 1;
        }

#ifndef MP3CP1 
        // vmem checks
        if (phdr.p_vaddr < UMEM_START_VMA || UMEM_END_VMA <= phdr.p_vaddr) {
            debug("p_vaddr out of range");
            return -EBADFMT;
        }

        if (UMEM_END_VMA - phdr.p_vaddr < phdr.p_memsz) {
            debug("p_memsz out of range");
            return -EBADFMT;
        }

        if (phdr.p_align == 0 || phdr.p_align % PAGE_SIZE) {
            debug("p_align not a multiple of page size (%lu)", PAGE_SIZE);
            return -EBADFMT;
        }
        // Round up p_memsz to page boundary boundary

        memsz = (phdr.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

        pte_flags = PTE_U;
        if (phdr.p_flags & PF_R)
            pte_flags |= PTE_R;
        if (phdr.p_flags & PF_W)
            pte_flags |= PTE_W;
        if (phdr.p_flags & PF_X)
            pte_flags |= PTE_X;

        alloc_and_map_range(phdr.p_vaddr, memsz, PTE_R | PTE_W | PTE_U);
#endif

        if (phdr.p_filesz != 0) {
            debug("PH[%d] Loading %d bytes at address %p from file offset 0x%lx",
                phidx, phdr.p_filesz, (void*)phdr.p_vaddr, phdr.p_offset);
            
            pos = phdr.p_offset;

            result = iofetch(io, pos, (void *)phdr.p_vaddr, phdr.p_filesz);
            if (result < phdr.p_filesz) {
                if (result < 0)
                    return result;
                else
                    return -EIO;
            }
        }

        // Zero any space between p_vaddr + p_filesz and p_vaddr + p_memsz

        if (phdr.p_filesz < phdr.p_memsz) {
            debug("PH[%d] Zeroing %d bytes at address %p",
                phidx, phdr.p_memsz - phdr.p_filesz,
                (void*)phdr.p_vaddr + phdr.p_filesz);

            memset (
                (void*)phdr.p_vaddr + phdr.p_filesz, 0,
                phdr.p_memsz - phdr.p_filesz);
        }

#ifndef MP3CP1 
        if (pte_flags != (PTE_R | PTE_W | PTE_U))
            set_range_flags((void*)phdr.p_vaddr, memsz, pte_flags);
#endif
    }

    if (!entry_ok) {
        debug("e_entry not in a program region");
        return -EBADFMT;
    }

    *eptr = (void (*)(void)) ehdr.e_entry;
    return 0;
#endif
}