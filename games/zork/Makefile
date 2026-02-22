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

ZORK_OBJS = \
	np.o \
	gdt.o \
	np1.o \
	np2.o \
	np3.o \
	dso1.o \
	dso2.o \
	dso3.o \
	dso4.o \
	dso5.o \
	dso6.o \
	dso7.o \
	dsub.o \
	supp.o \
	dgame.o \
	dinit.o \
	dmain.o \
	local.o \
	nobjs.o \
	rooms.o \
	sobjs.o \
	verbs.o \
	villns.o \
	actors.o \
	ballop.o \
	clockr.o \
	demons.o \
	dverb1.o \
	dverb2.o \
	lightp.o \
	nrooms.o \
	objcts.o \
	sverbs.o \

# $(KOBJDIR)/$(MP3CP1_PREFIX)_io.o 
# 	$(KOBJDIR)/$(MP3CP1_PREFIX)_device.o
# 	$(KOBJDIR)/$(MP3CP1_PREFIX)_filesys.o

ZORK_MP3_CP1_OBJS = \
	$(addprefix $(SOBJDIR)/$(MP3CP1_PREFIX)_, $(ZORK_OBJS)) \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_io.o \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_glue.o \
	$(KOBJDIR)/$(MP3CP1_PREFIX)_string.o \
	$(KOBJDIR)/$(MP3CP1_PREFIX)_setjmp.o \


ZORK_MP3_CP2_OBJS = \
	$(addprefix $(SOBJDIR)/$(MP3CP2_PREFIX)_, $(ZORK_OBJS)) \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_io.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_heap.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_start.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_string.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_syscall.o \

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. # -DDEBUG -DTRACE

ASFLAGS += -march=rv64g -mabi=lp64d

# Special flags for zork
# CFLAGS += -Wno-implicit-int -Wno-implicit-function-declaration
# CFLAGS += -Wno-unused-variable -Wno-return-type
CFLAGS += -Wno-unused-but-set-variable -Wno-parentheses -Wno-missing-braces

#################################################

all: zork-mp3-cp2

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
## CP1

zork-mp3-cp1: $(UOBJDIR)/zork_mp3_cp1.elf | $(BINDIR)
	mv $(UOBJDIR)/zork_mp3_cp1.elf $(BINDIR)/zork-mp3-cp1

# Main binaries
$(UOBJDIR)/zork_mp3_cp1.elf: $(ZORK_MP3_CP1_OBJS)
	$(LD) -T $(USRDIR)/no_umode.ld -o $@ $^

# Trek + glue binaries
$(UOBJDIR)/$(MP3CP1_PREFIX)_glue.o: glue.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

$(SOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(SRCDIR)/%.c | $(SOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

# User mode binaries
$(UOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(USRDIR)/%.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

$(KOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(KERNDIR)/%.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

$(KOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(KERNDIR)/%.s | $(KOBJDIR)
	$(AS) $(ASFLAGS) -o $@ $<
	
$(UOBJDIR)/$(MP3CP1_PREFIX)_%.s: $(USRDIR)/%.S | $(UOBJDIR)
	$(CPP) $(CPPFLAGS) -o $@ -c $<

$(UOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(USRDIR)/%.s | $(UOBJDIR)
	$(AS) $(ASFLAGS) -o $@ -c $<

#################################################
## CP2 + CP3

zork-mp3-cp2: $(UOBJDIR)/zork_mp3_cp2.elf | $(BINDIR)
	mv $(UOBJDIR)/zork_mp3_cp2.elf $(BINDIR)/zork-mp3-cp2

# Main binary
$(UOBJDIR)/zork_mp3_cp2.elf: $(ZORK_MP3_CP2_OBJS)
	$(LD) -T $(USRDIR)/umode.ld -o $@ $^

# Zork + glue binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o: glue.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(SOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(SRCDIR)/%.c | $(SOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<
	
# User mode binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.S | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -c $< -o $@

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.s | $(UOBJDIR)
	$(AS) $(ASFLAGS) -o $@ -c $<

#################################################
