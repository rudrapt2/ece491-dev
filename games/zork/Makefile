# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

ZORK_OBJS = \
	source/actors.o \
	source/ballop.o \
	source/clockr.o \
	source/demons.o \
	source/dgame.o \
	source/dinit.o \
	source/dmain.o \
	source/dso1.o \
	source/dso2.o \
	source/dso3.o \
	source/dso4.o \
	source/dso5.o \
	source/dso6.o \
	source/dso7.o \
	source/dsub.o \
	source/dverb1.o \
	source/dverb2.o \
	source/gdt.o \
	source/lightp.o \
	source/local.o \
	source/nobjs.o \
	source/np1.o \
	source/np2.o \
	source/np3.o \
	source/np.o \
	source/nrooms.o \
	source/objcts.o \
	source/rooms.o \
	source/sobjs.o \
	source/supp.o \
	source/sverbs.o \
	source/verbs.o \
	source/villns.o \
	usr/string.o \
	usr/syscall.o \
	usr/heap.o \
	usr/start.o \
	glue.o

# ZORK_OBJS_MP2 = $(addprefix mp2_, $(ZORK_OBJS))
# ZORK_OBJS_MP3 = $(addprefix mp3_, $(ZORK_OBJS))

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. # -DDEBUG -DTRACE

# Special flags for zork

# CFLAGS += -Wno-implicit-int -Wno-implicit-function-declaration
# CFLAGS += -Wno-unused-variable -Wno-return-type
CFLAGS += -Wno-unused-but-set-variable -Wno-parentheses -Wno-missing-braces

ASFLAGS = -march=rv64imazicsr -mabi=lp64

all: mp3

mp3: $(ZORK_OBJS)
	$(LD) -T umode.ld -o zork.elf $^

clean:
	rm -rf *.o *.elf
