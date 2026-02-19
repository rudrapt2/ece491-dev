#ifndef MEM_DESC_H
#define MEM_DESC_H

struct mregion {
    unsigned long pma;
    unsigned long size;
};

struct matlas {
    struct mregion * ram;
    struct mregion * mmio;
    struct mregion * resv;
};

#endif

// 0x0000_0000_0000_0000 to 0x0000_003F_FFFF_FFFF kernel
// 0xFFFF_FFC0_0000_0000 to 0xFFFF_FFFF_FFFF_FFFF user
//
