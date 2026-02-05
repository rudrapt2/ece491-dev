// main.c - main function of the kernel (called from start.s)
//
// Copyright (c) 2024-2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifdef NGFS_TRACE
#define TRACE
#endif

#ifdef NGFS_DEBUG
#define DEBUG
#endif

#include "../../conf.h"
#include "../../console.h"
#include "../../intr.h"
#include "../../device.h"
#include "../../thread.h"
#include "../../filesys.h"
#include "../../process.h"
#include "../../error.h"
#include "../../misc.h"
#include "../../io.h"
#include "../../fs/ngfs.h"
#include "../../fs/ktfs.h"
#include "../../elf.h"
#include "../../heap.h"
#include "../../string.h"

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

extern void board_init(unsigned int hartid, void * dtb); // from board/xxx.c
extern void attach_devices(void); // from board/xxx.c

extern char _kimg_blob_start[], _kimg_blob_end[];
unsigned long long rand_state;

static unsigned long long rand() {
    rand_state *= 25214903917UL;
    rand_state += 11;
    return (rand_state >> 12);
}

int fake_elf_load(struct io * io, void (**eptr)(void)) {
    static const uint32_t MAGIC_LSB = 0x464c457f;
    struct elf64_ehdr ehdr;
    struct elf64_phdr phdr;
    uint_fast16_t phidx;
    unsigned long long size;
    unsigned long long pos;
    long result;
    int pte_flags;
    int entry_ok = 0;
    size_t memsz;
    char c;

    // Get ELF file length

    result = ioctl(io, IOC_GETEND, &size);

    if (result != 0)
        return result;
    
    result = iofetch(io, 0, &ehdr, sizeof(ehdr));

    if (result != sizeof(ehdr)) {
        if (result < 0)
            return result;
        return -EIO;
    }

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

        if (phdr.p_vaddr < UMEM_START_VMA || UMEM_END_VMA <= phdr.p_vaddr) {
            debug("p_vaddr out of range");
            return -EBADFMT;
        }

        if (UMEM_END_VMA - phdr.p_vaddr < phdr.p_memsz) {
            debug("p_memsz out of range");
            return -EBADFMT;
        }

        if (phdr.p_memsz < phdr.p_filesz) {
            debug("p_filesz larger than p_memsz");
            return -EBADFMT;
        }

        // Alignment must be a multiple of PAGE_SIZE

        if (phdr.p_align == 0 || phdr.p_align % PAGE_SIZE) {
            debug("p_align not a multiple of page size (%lu)", PAGE_SIZE);
            return -EBADFMT;
        }

        if (phdr.p_vaddr % phdr.p_align) {
            debug("p_vaddr not %lu-byte aligned (p_align)", phdr.p_align);
            return -EBADFMT;
        }

        // Round up p_memsz to page boundary boundary

        memsz = (phdr.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

        if (phdr.p_vaddr <= ehdr.e_entry &&
            ehdr.e_entry < phdr.p_vaddr + phdr.p_memsz)
        {

            entry_ok = 1;
        }

        if (phdr.p_filesz != 0) {
            debug("PH[%d] Loading %d bytes at address %p from file offset 0x%lx",
                phidx, phdr.p_filesz, (void*)phdr.p_vaddr, phdr.p_offset);
            
            pos = phdr.p_offset;

            for (int i=0; i < phdr.p_filesz; i++) {
                result = iofetch(io, pos+i, &c, 1);
                assert(result == 1);
                debug("i=%d, r=%d, t=%d", i, *((char*)phdr.p_vaddr+i), c);
                assert(c == *((char*)phdr.p_vaddr+i));
            }
        }

        // Zero any space between p_vaddr + p_filesz and p_vaddr + p_memsz

        if (phdr.p_filesz < phdr.p_memsz) {
            debug("PH[%d] Zeroing %d bytes at address %p",
                phidx, phdr.p_memsz - phdr.p_filesz,
                (void*)phdr.p_vaddr + phdr.p_filesz);

            for (int i=0; i < phdr.p_memsz - phdr.p_filesz; i++) {
                result = iofetch(io, pos+i, &c, 1);
                assert(result == 1);
                assert(c == *((char*)phdr.p_vaddr+i));
            }
        }
    }

    if (!entry_ok) {
        debug("e_entry not in a program region");
        return -EBADFMT;
    }

    *eptr = (void (*)(void)) ehdr.e_entry;
    return 0;
}

void elf_check() {
    char FILE[] = "shell"; // contents should be same as blob.raw
    struct io * fio, * memio;
    int result;
    void (*entry1)(void);
    void (*entry2)(void);

    memio = create_memio(_kimg_blob_start, _kimg_blob_end-_kimg_blob_start, NULL);
    result = open_file("c", FILE, &fio);
    if (result < 0) {
        kprintf("Failed to open file %s:\n", FILE, error_name(result));
        return;
    }

    assert(elf_load(memio, &entry1) == 0);
    assert(fake_elf_load(fio, &entry2) == 0);
    assert(entry1 == entry2);
    kprintf("elf load passed\n");
}

void test_fs() {
    rand_state = rdtime();
    char FILE[] = "shell"; // contents should be same as blob.raw
    struct io * fio, * memio;
    int result;
    unsigned long long start = 0;
    unsigned long long e1,e2;
    char c, p;
    int bytes = 0;

    memio = create_memio(_kimg_blob_start, _kimg_blob_end-_kimg_blob_start, NULL);
    result = open_file("c", FILE, &fio);
    if (result < 0) {
        kprintf("Failed to open file %s:\n", FILE, error_name(result));
        return;
    }
    ioctl(fio, IOC_SETPOS, &start);
    ioctl(memio, IOC_SETPOS, &start);
    ioctl(fio, IOC_GETEND, &e1);
    ioctl(memio, IOC_GETEND, &e2);
    assert(e1==e2);

    for (int i=0;i<1000000;i++) {
        result = ioread(fio, &c, 1);
        if (result < 0) {
            kprintf("Failed to read from %s:\n", FILE, error_name(result));
            return;
        }
        result = ioread(memio, &p, 1);
        if (result == 0) break;
        if (c != p) {
            kprintf("Failed data check: expected %c but got %c (read %d bytes)\n", p, c, bytes);
            return;
        }
        bytes++;
        if (rand() % 1000 == 0) {
            start = rand() % 10000;
            ioctl(fio, IOC_SETPOS, &start);
            ioctl(memio, IOC_SETPOS, &start);
        }
    }
    kprintf("test_fs passed with %d byte file\n", bytes);
}

void mount_drive(char * mntname, char * devname, 
    int (*mount)(const char * mpname, struct io * bkgio)) {
    
    struct io * hd;
    int result;

    result = open_device(devname, &hd);

    if (result < 0) {
        kprintf("Failed to open storage device %s: %s\n", 
            devname, error_desc(result));
        halt();
    }

    result = mount(mntname, hd);

    if (result != 0) {
        kprintf("mount(%s, bkgio(%s)) failed: %s\n",
            mntname, devname, error_desc(result));
        halt();
    }
}

void main(unsigned int hartid, void * dtb) {
    board_init(hartid, dtb);
    intrmgr_init();
    devmgr_init();
    thrmgr_init();

    // MP3 stuff
    memory_init();
    procmgr_init();
    fsmgr_init();

    attach_devices();
    enable_interrupts();

    mount_devfs("dev");
    mount_drive("c", "vioblk1", mount_ngfs);

    // test_fs();
    elf_check();
}
