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

ROGUE_OBJS = \
	hit.o \
	use.o \
	zap.o \
	save.o \
	init.o \
	move.o \
	pack.o \
	play.o \
	ring.o \
	room.o \
	trap.o \
	level.o \
	score.o \
	throw.o \
	curses.o \
	object.o \
	random.o \
	message.o \
	monster.o \
	spec_hit.o \
	inventory.o \

ROGUE_MP3_CP1_OBJS = \
	$(addprefix $(SOBJDIR)/$(MP3CP1_PREFIX)_, $(ROGUE_OBJS)) \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_io.o \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_main.o \
	$(UOBJDIR)/$(MP3CP1_PREFIX)_glue.o \
	$(KOBJDIR)/$(MP3CP1_PREFIX)_string.o \
	$(KOBJDIR)/$(MP3CP1_PREFIX)_setjmp.o \

ROGUE_MP3_CP2_OBJS = \
	$(addprefix $(SOBJDIR)/$(MP3CP2_PREFIX)_, $(ROGUE_OBJS)) \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_io.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_heap.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_main.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_start.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_string.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_syscall.o \

CFLAGS = -Wall -fno-omit-frame-pointer -Oz -s -ggdb -gdwarf-2
CFLAGS += -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -Wno-return-mismatch -Wno-int-conversion
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -Wno-implicit-int -Wno-implicit-function-declaration
CFLAGS += -Wno-unused-variable -Wno-return-type
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -mcmodel=medany
CFLAGS += -I. # -DDEBUG -DTRACE
CFLAGS += -DCURSES 
CFLAGS += $(EXTRA_CFLAGS)

ASFLAGS += -march=rv64g -mabi=lp64d

#################################################

all: rogue-mp3-cp2

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

rogue-mp3-cp1: $(UOBJDIR)/rogue_mp3_cp1.elf | $(BINDIR)
	mv $(UOBJDIR)/rogue_mp3_cp1.elf $(BINDIR)/rogue-mp3-cp1

# Main binaries
$(UOBJDIR)/rogue_mp3_cp1.elf: $(ROGUE_MP3_CP1_OBJS)
	$(LD) -T $(USRDIR)/no_umode.ld -o $@ $^

# Rogue + glue binaries
$(UOBJDIR)/$(MP3CP1_PREFIX)_main.o: main.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

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

rogue-mp3-cp2: $(UOBJDIR)/rogue_mp3_cp2.elf | $(BINDIR)
	mv $(UOBJDIR)/rogue_mp3_cp2.elf $(BINDIR)/rogue-mp3-cp2

# Main binary
$(UOBJDIR)/rogue_mp3_cp2.elf: $(ROGUE_MP3_CP2_OBJS)
	$(LD) -T $(USRDIR)/umode.ld -o $@ $^

# Rogue + glue binaries
$(UOBJDIR)/$(MP3CP2_PREFIX)_main.o: main.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

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
