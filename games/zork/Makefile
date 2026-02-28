# Makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

KERNDIR=kernel
USRDIR=usr
SRCDIR=source

MP3CP1_PREFIX=mp3cp1
MP3CP2_PREFIX=mp3cp2

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
	villns.o 

ZORK_MP3_CP1_OBJS = \
	$(addprefix $(SRCDIR)/$(MP3CP1_PREFIX)_, $(ZORK_OBJS)) \
	$(USRDIR)/$(MP3CP1_PREFIX)_io.o \
	$(KERNDIR)/$(MP3CP1_PREFIX)_thread.o \
	$(KERNDIR)/$(MP3CP1_PREFIX)_string.o \
	$(KERNDIR)/$(MP3CP1_PREFIX)_device.o \
	$(KERNDIR)/$(MP3CP1_PREFIX)_filesys.o \
	$(MP3CP1_PREFIX)_glue.o 

ZORK_MP3_CP2_OBJS = \
	$(addprefix $(SRCDIR)/$(MP3CP2_PREFIX)_, $(ZORK_OBJS)) \
	$(USRDIR)/$(MP3CP2_PREFIX)_io.o \
	$(USRDIR)/$(MP3CP2_PREFIX)_heap.o \
	$(USRDIR)/$(MP3CP2_PREFIX)_start.o \
	$(USRDIR)/$(MP3CP2_PREFIX)_string.o \
	$(USRDIR)/$(MP3CP2_PREFIX)_syscall.o \
	$(MP3CP2_PREFIX)_glue.o 

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

all: zork-mp3cp2.elf

#################################################
## CP1

# Main binary
zork-mp3cp1.elf: $(ZORK_MP3_CP1_OBJS)
	$(LD) -T $(USRDIR)/no_umode.ld -o $@ $^

# Zork + glue binaries
$(SRCDIR)/$(MP3CP1_PREFIX)_%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

$(MP3CP1_PREFIX)_glue.o: glue.c
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

# Kernel binaries
$(KERNDIR)/$(MP3CP1_PREFIX)_%.o: $(KERNDIR)/%.c
	$(CC) $(CFLAGS) -include $(KERNDIR)/string.h -DAEE31 -DMP3CP1 -o $@ -c $<

# User binaries
$(USRDIR)/$(MP3CP1_PREFIX)_%.o: $(USRDIR)/%.c
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

$(USRDIR)/$(MP3CP1_PREFIX)_%.o: $(USRDIR)/%.s
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

#################################################

#################################################
## CP2 + CP3

# Main binary
zork-mp3cp2.elf: $(ZORK_MP3_CP2_OBJS)
	$(LD) -T $(USRDIR)/umode.ld -o $@ $^

# Zork + glue binaries
$(SRCDIR)/$(MP3CP2_PREFIX)_%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(MP3CP2_PREFIX)_glue.o: glue.c
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

# User binaries
$(USRDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.c
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

$(USRDIR)/$(MP3CP2_PREFIX)_%.o: $(USRDIR)/%.s
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

#################################################

clean:
	rm -rf *.o *.elf kernel/*.o source/*.o usr/*.o
