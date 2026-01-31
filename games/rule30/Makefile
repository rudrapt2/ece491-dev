# Makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

CFLAGS = -Wall -Werror=implicit-function-declaration
CFLAGS += -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.

ASFLAGS += -march=rv64imazicsr -mabi=lp64

BINDIR=bin
KOBJDIR=kernel/build

RULE30_OBJS = \
	rule30.o \
	glue.o

RULE30_OBJS_MP2 = $(KOBJDIR)/mp2_rule30.o \
			$(KOBJDIR)/mp2_glue.o

#################################################

all: rule30-mp2

clean:
	rm -rf *.o *.elf *.asm
	rm -rf $(KOBJDIR)
	rm -rf $(BINDIR)

#######################MP0#######################

$(KOBJDIR):
	mkdir -p $(KOBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(KOBJDIR)/rule30_mp0.o: rule30.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE0 -o $@ -c $<

#######################MP2#######################

rule30-mp2: $(BINDIR)/rule30.o

$(KOBJDIR)/mp2_rule30.o: rule30.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE2 -o $@ -c $<

$(KOBJDIR)/mp2_glue.o: glue.c | $(KOBJDIR)
	$(CC) $(CFLAGS) -DAEE2 -o $@ -c $<

$(KOBJDIR)/rule30_mp2.o: $(RULE30_OBJS_MP2)
	$(LD) -r -o $@ $^

# Relocatable output for MP2 placed in kernel/build
$(KOBJDIR)/rule30_mp2: $(RULE30_OBJS_MP2)
	$(LD) -r -o $@ $^

# Final MP2 binary in bin/
$(BINDIR)/rule30.o: $(RULE30_OBJS_MP2) | $(BINDIR)
	$(LD) -r -o $@ $^

#######################MP3 CP1###################

mp3_cp1_%.o: %.c
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

mp3_cp1_%.o: %.s
	$(AS) $(ASFLAGS) -o $@ -c $<

# rule30_mp3_cp1.elf: mp3_cp1_rule30.o mp3_cp1_glue.o usr/uio.o
# 	$(LD) -T usr/no_umode.ld -o $@ $^

#######################MP3 CP2###################

mp3_cp2_%.o: %.c
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

%.s: %.S
	$(CPP) $(CPPFLAGS) -o $@ -c $<

mp3_cp2_%.o: %.s
	$(AS) $(ASFLAGS) -o $@ -c $<

# rule30_mp3_cp2.elf: mp3_cp2_rule30.o mp3_cp2_glue.o usr/start.o usr/syscall.o usr/string.o usr/heap.o
# 	$(LD) -T usr/umode.ld -o $@ $^

%.s: %.S
	$(CPP) $(CPPFLAGS) -o $@ -c $<

# pattern for preprocessing .S remains
