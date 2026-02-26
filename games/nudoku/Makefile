# Makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

BINDIR=bin
KERNDIR=kernel
USRDIR=usr
SRCDIR=source

KOBJDIR=$(KERNDIR)/build
UOBJDIR=$(USRDIR)/build
SOBJDIR=$(SRCDIR)/build

MP0_PREFIX=mp0
MP2_PREFIX=mp2
MP3CP1_PREFIX=mp3_cp1
MP3CP2_PREFIX=mp3_cp2

NUDOKU_OBJS = \
	main.o \
	curses.o \
	sudoku.o \

NUDOKU_MP3_CP2_OBJS = \
	$(addprefix $(SOBJDIR)/$(MP3CP2_PREFIX)_, $(NUDOKU_OBJS)) \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_io.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_heap.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_lumonOS.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_start.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_string.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_syscall.o \

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I$(SRCDIR) -I. # -DDEBUG -DTRACE

ASFLAGS += -march=rv64g -mabi=lp64d

# Special flags for nudoku
CFLAGS += -Wno-implicit-int -Wno-implicit-function-declaration
CFLAGS += -Wno-unused-variable -Wno-return-type -Wno-char-subscripts
CFLAGS += -Wno-unused-but-set-variable -Wno-parentheses -Wno-missing-braces
CFLAGS += -Wno-incompatible-pointer-types -Wno-int-conversion

#################################################

all: nudoku-mp3-cp2

clean:
	rm -rf *.o *.elf *.asm
	rm -rf $(KOBJDIR)
	rm -rf $(UOBJDIR)
	rm -rf $(SOBJDIR)
	rm -rf $(BINDIR)

$(KOBJDIR):
	mkdir -p $(KOBJDIR)

$(UOBJDIR):
	mkdir -p $(UOBJDIR)

$(SOBJDIR):
	mkdir -p $(SOBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

#################################################
## CP2

nudoku-mp3-cp2: $(UOBJDIR)/nudoku_mp3_cp2.elf | $(BINDIR)
	mv $(UOBJDIR)/nudoku_mp3_cp2.elf $(BINDIR)/nudoku-mp3-cp2

# Nudoku + main binaries
$(UOBJDIR)/nudoku_mp3_cp2.elf: $(NUDOKU_MP3_CP2_OBJS)
	$(LD) -T $(USRDIR)/umode.ld -o $@ $^

$(SOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(SRCDIR)/%.c | $(SOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -Dmain=nudoku_main -DVERSION='"0.1"' -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o: glue.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_lumonOS.o: lumonOS.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

# User mode binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.S | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -c $< -o $@

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.s | $(UOBJDIR)
	$(AS) $(ASFLAGS) -o $@ -c $<

#################################################
