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
            - usr/progs/hello.c
                - successfully compiling
            - deprecated sys/start.s
