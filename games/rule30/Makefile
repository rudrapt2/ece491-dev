# Makefile
# 

PREFIX=riscv64-unknown-elf-

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

RULE30_OBJS_MP0 = \
	$(KOBJDIR)/$(MP0_PREFIX)_rule30.o \
	$(KOBJDIR)/$(MP0_PREFIX)_glue.o \

RULE30_OBJS_MP2 = \
	$(KOBJDIR)/$(MP2_PREFIX)_rule30.o \
	$(KOBJDIR)/$(MP2_PREFIX)_glue.o \

RULE30_MP3_CP1_OBJS = \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_io.o \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_rule30.o \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_glue.o \
	$(KOBJDIR)/$(MP3CP1_PREFIX)_string.o \

RULE30_MP3_CP2_OBJS = \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_io.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_rule30.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_heap.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_start.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_string.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_syscall.o \

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I.

ASFLAGS += -march=rv64g -mabi=lp64d

#################################################

all: rule30-mp2

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

rule30-mp0: $(KOBJDIR)/rule30_mp0.o | $(BINDIR)
	mv $(KOBJDIR)/rule30_mp0.o $(BINDIR)/rule30.o

# Main binary
$(KOBJDIR)/rule30_mp0.o: $(RULE30_OBJS_MP0)
	$(LD) -r -o $@ $^

# Trek + glue binaries
$(KOBJDIR)/$(MP0_PREFIX)_rule30.o: rule30.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE0 -o $@ -c $<

$(KOBJDIR)/$(MP0_PREFIX)_glue.o: glue.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE0 -o $@ -c $<

#######################MP2#######################

rule30-mp2: $(KOBJDIR)/rule30_mp2.o | $(BINDIR)
	mv $(KOBJDIR)/rule30_mp2.o $(BINDIR)/rule30.o

# Main binary
$(KOBJDIR)/rule30_mp2.o: $(RULE30_OBJS_MP2)
	$(LD) -r -o $@ $^

# Trek + glue binaries
$(KOBJDIR)/$(MP2_PREFIX)_rule30.o: rule30.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE2 -o $@ -c $<

$(KOBJDIR)/$(MP2_PREFIX)_glue.o: glue.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE2 -o $@ -c $<

#######################MP3#######################
## CP1

rule30-mp3-cp1: $(UOBJDIR)/rule30_mp3_cp1.elf | $(BINDIR)
	mv $(UOBJDIR)/rule30_mp3_cp1.elf $(BINDIR)/rule30-mp3-cp1

# Main binaries
$(UOBJDIR)/rule30_mp3_cp1.elf: $(RULE30_MP3_CP1_OBJS)
	$(LD) -T $(USRDIR)/no_umode.ld -o $@ $^

# Trek + glue binaries
$(UOBJDIR)/$(MP3CP1_PREFIX)_rule30.o: rule30.c | $(UOBJDIR)
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

rule30-mp3-cp2: $(UOBJDIR)/rule30_mp3_cp2.elf | $(BINDIR)
	mv $(UOBJDIR)/rule30_mp3_cp2.elf $(BINDIR)/rule30-mp3-cp2

# Main binaries
$(UOBJDIR)/rule30_mp3_cp2.elf: $(RULE30_MP3_CP2_OBJS)
	$(LD) -T $(USRDIR)/umode.ld -o $@ $^

# Trek + glue binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_rule30.o: rule30.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o: glue.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

# User mode binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.S | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -c $< -o $@

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.s | $(UOBJDIR)
	$(AS) $(ASFLAGS) -o $@ -c $<

#################################################