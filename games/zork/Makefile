# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

ZORK_OBJS = \
	actors.o \
	ballop.o \
	clockr.o \
	demons.o \
	dgame.o \
	dinit.o \
	dmain.o \
	dso1.o \
	dso2.o \
	dso3.o \
	dso4.o \
	dso5.o \
	dso6.o \
	dso7.o \
	dsub.o \
	dverb1.o \
	dverb2.o \
	gdt.o \
	lightp.o \
	local.o \
	nobjs.o \
	np1.o \
	np2.o \
	np3.o \
	np.o \
	nrooms.o \
	objcts.o \
	rooms.o \
	sobjs.o \
	supp.o \
	sverbs.o \
	verbs.o \
	villns.o \
	glue.o \
	string.o \
	syscall.o \
	heap.o \
	start.o \

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

# dtextc.o: dtextc.dat
# 	echo .end | $(AS) $(ASFLAGS) -o $@
# 	$(OBJCOPY) \
# 	  --set-section-flags .dtextc=alloc,load,readonly,data \
# 	  --add-section .dtextc=dtextc.dat $@

clean:
	rm -rf *.o *.elf
