PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

BINDIR=bin
USRDIR=usr
KERNDIR=kernel
SRCDIR=source

UOBJDIR=$(USRDIR)/build
SOBJDIR=$(SRCDIR)/build

MP3CP2_PREFIX=mp3_cp2

TETRIS_OBJS = \
	input.o \
	screen.o \
	shapes.o \
	tetris.o \
	scores_stub.o \

TETRIS_MP3_CP2_OBJS = \
	$(addprefix $(SOBJDIR)/$(MP3CP2_PREFIX)_, $(TETRIS_OBJS)) \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_heap.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_start.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_string.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_syscall.o \
	$(UOBJDIR)/$(MP3CP2_PREFIX)_setjmp.o \

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. -I$(SRCDIR)
CFLAGS += -D_PATH_SCOREFILE='"state.save"'

ASFLAGS += -march=rv64g -mabi=lp64d

all: tetris-mp3-cp2

clean:
	rm -rf *.o *.elf *.asm
	rm -rf build
	rm -rf $(UOBJDIR)
	rm -rf $(SOBJDIR)
	rm -rf $(BINDIR)

$(UOBJDIR):
	mkdir -p $(UOBJDIR)

$(SOBJDIR):
	mkdir -p $(SOBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# CP2

tetris-mp3-cp2: $(UOBJDIR)/tetris_mp3_cp2.elf | $(BINDIR)
	mv $(UOBJDIR)/tetris_mp3_cp2.elf $(BINDIR)/tetris-mp3-cp2

$(UOBJDIR)/tetris_mp3_cp2.elf: $(TETRIS_MP3_CP2_OBJS)
	$(LD) -T $(USRDIR)/umode.ld -o $@ $^

$(SOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(SRCDIR)/%.c | $(SOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -Dmain=tetris_main -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_glue.o: glue.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.c | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.S | $(UOBJDIR)
	$(CC) $(CFLAGS) -DAEE32 -c $< -o $@

$(UOBJDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.s | $(UOBJDIR)
	$(AS) $(ASFLAGS) -o $@ -c $<
