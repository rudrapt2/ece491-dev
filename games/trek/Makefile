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

KOBJDIR=$(KERNDIR)/build
UOBJDIR=$(USRDIR)/build

MP0_PREFIX=mp0
MP2_PREFIX=mp2
MP3CP1_PREFIX=mp3_cp1
MP3CP2_PREFIX=mp3_cp2

TREK_OBJS_MP0 = \
	$(KOBJDIR)/$(MP0_PREFIX)_trek.o \
	$(KOBJDIR)/$(MP0_PREFIX)_glue.o \

TREK_OBJS_MP2 = \
	$(KOBJDIR)/$(MP2_PREFIX)_trek.o \
	$(KOBJDIR)/$(MP2_PREFIX)_glue.o \

TREK_MP3_CP1_OBJS = \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_uio.o \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_trek.o \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_glue.o \
	$(KOBJDIR)/$(MP3CP1_PREFIX)_string.o \

TREK_MP3_CP2_OBJS = \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_uio.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_trek.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_heap.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_start.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_string.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_syscall.o \

CFLAGS = -Wall -Werror=implicit-function-declaration
CFLAGS += -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.

ASFLAGS += -march=rv64imazicsr -mabi=lp64

#################################################

all: trek-mp2

clean:
	rm -rf *.o *.elf *.asm
	rm -rf $(KOBJDIR)
	rm -rf $(UOBJDIR)
	rm -rf $(BINDIR)

$(KOBJDIR):
	mkdir -p $(KOBJDIR)

$(UOBJDIR):
	mkdir -p $(UOBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

#######################MP0#######################

trek-mp0: $(KOBJDIR)/trek_mp0.o | $(BINDIR)
	mv $(KOBJDIR)/trek_mp0.o $(BINDIR)/trek.o

# Main binary
$(KOBJDIR)/trek_mp0.o: $(TREK_OBJS_MP0)
	$(LD) -r -o $@ $^

# Trek + glue binaries
$(KOBJDIR)/$(MP0_PREFIX)_trek.o: trek.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE0 -o $@ -c $<

$(KOBJDIR)/$(MP0_PREFIX)_glue.o: glue.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE0 -o $@ -c $<

#######################MP2#######################

trek-mp2: $(KOBJDIR)/trek_mp2.o | $(BINDIR)
	mv $(KOBJDIR)/trek_mp2.o $(BINDIR)/trek.o

# Main binary
$(KOBJDIR)/trek_mp2.o: $(TREK_OBJS_MP2)
	$(LD) -r -o $@ $^

# Trek + glue binaries
$(KOBJDIR)/$(MP2_PREFIX)_trek.o: trek.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE2 -o $@ -c $<

$(KOBJDIR)/$(MP2_PREFIX)_glue.o: glue.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE2 -o $@ -c $<

#######################MP3#######################
## CP1

trek-mp3-cp1: $(UOBJDIR)/trek_mp3_cp1.elf | $(BINDIR)
	mv $(UOBJDIR)/trek_mp3_cp1.elf $(BINDIR)/trek

# Main binaries
$(UOBJDIR)/trek_mp3_cp1.elf: $(TREK_MP3_CP1_OBJS)
	$(LD) -T $(USRDIR)/no_umode.ld -o $@ $^

# Trek + glue binaries
$(UOBJDIR)/$(MP3CP1_PREFIX)_trek.o: trek.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

$(UOBJDIR)/$(MP3CP1_PREFIX)_glue.o: glue.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

# User mode binaries
$(UOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(USRDIR)/%.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

$(KOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(KERNDIR)/%.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<
	
$(UOBJDIR)/$(MP3CP1_PREFIX)_%.s: $(USRDIR)/%.S | $(UOBJDIR)
	$(CPP) $(CPPFLAGS) -o $@ -c $<

$(UOBJDIR)/$(MP3CP1_PREFIX)_%.o: $(USRDIR)/%.s | $(UOBJDIR)
	$(AS) $(ASFLAGS) -o $@ -c $<

#################################################
## CP2 + CP3

trek-mp3-cp2: $(UOBJDIR)/trek_mp3_cp2.elf | $(BINDIR)
	mv $(UOBJDIR)/trek_mp3_cp2.elf $(BINDIR)/trek

# Main binaries
$(UOBJDIR)/trek_mp3_cp2.elf: $(TREK_MP3_CP2_OBJS)
	$(LD) -T $(USRDIR)/umode.ld -o $@ $^

# Trek + glue binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_trek.o: trek.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o: glue.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

# User mode binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.s: $(USRDIR)/%.S | $(UOBJDIR)
	$(CPP) $(CPPFLAGS) -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.s | $(UOBJDIR)
	$(AS) $(ASFLAGS) -o $@ -c $<

#################################################


