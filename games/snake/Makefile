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
	usr/heap.o \
	usr/start.o \
	usr/error.o \
	usr/string.o \
	usr/syscall.o \

ULIB_LD = usr/umode.ld

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -DUMODE
CFLAGS += -I.

all: bin/snake

bin/snake: $(ULIB_OBJS) snake.o | bin
	$(LD) -T $(ULIB_LD) -o $@ $^

bin: 
	mkdir $@

clean:
	rm -rf *.o *.elf *.asm bin/
