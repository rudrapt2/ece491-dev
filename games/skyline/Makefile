# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

ULIB_OBJS = \
	usr/io.o \
	usr/error.o \
	usr/string.o \
	usr/syscall.o \
	usr/heap.o

ULIB_OBJS += usr/start.o
ULIB_LD = usr/umode.ld

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I.

all: skyline

bin:
	mkdir -p bin

skyline: $(ULIB_OBJS) demo.o gpu.o rand.o mp1.o demo_scene.o | bin
	$(LD) $(LDFLAGS) -T $(ULIB_LD) -o $@ $^
	mv skyline bin/skyline

clean:
	rm -rf *.o usr/*.o skyline bin