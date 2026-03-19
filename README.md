mp1:
    - multiple threads per process
    - system calls match unix system calls
    - nonblocking io

mp2:
    - multiple harts
    - signals

mp3
    - memory mapped files, updated vmem


03/15/2026:
    - 1900 - 2200
        - research higher half kernel
            - https://medium.com/@connorstack/how-does-a-higher-half-kernel-work-107194e46a64        
        - research early boot process
        - where to download toolchain for musl
            - https://toolchains.bootlin.com/
            - riscv64-lp64d, musl

03/16/2026:
    - 0100 - 0200
        - implement code changes
            - sys/boot.s
                - sets up an early boot page table and does coarse-grained Gigarange mappings for 1:1 vma->pma mappings and also real kernel vma->pma mappings; each has RWX perms
                - note the need to store memory addresses of functions/variables in memory then load from that address to prevent pc-relative shenanigans
            - usr/progs/hello.c
                - successfully compiling
            - deprecated sys/start.s

03/19/2026:
    - 0100 - 0500
        - implement changes to page_table_init()
            - sets up proper page tables
        - implement changes to various other places
            - note that anywhere alloc_phys_page(s) was used must now use proper virtual addresses
            - dma devices like vioblk need to be given physical addresses
