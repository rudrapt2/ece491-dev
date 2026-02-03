# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

ULIB_OBJS = \
	uio.o \
	heap.o \
	string.o \
	syscall.o \
	start.o

ULIB_LD = usr/umode.ld

OBJDIR=usr/build

ALL_TARGETS = \
	doomlauncher

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. -DUMODE

OBJS += $(addprefix $(OBJDIR)/, $(ULIB_OBJS))

.PHONY: all bin/% bin 
.SECONDARY: $(OBJS)

# Can onyl work with MP3 in U mode

#################################################

all: $(addprefix bin/, $(ALL_TARGETS))
	make -C source
	cp data/doom1.wad bin/doom1.wad

clean:
	make clean -C source
	rm -rf *.o *.elf *.asm usr/build/ bin/

#################################################

$(OBJDIR):
	mkdir -p $(OBJDIR)

bin:
	mkdir -p bin

$(OBJDIR)/%.o: usr/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: usr/%.S | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: usr/%.s | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

bin/%: $(OBJS) %.o | bin
	$(LD) -T $(ULIB_LD) -o $@ $^

#################################################