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

TREK_OBJS = glue.o \
			trek.o 

TREK_OBJS_MP3_CP1 = $(addprefix mp3_cp1_, $(TREK_OBJS))
TREK_OBJS_MP3_CP1 += mp3_cp1_kern_string.o
TREK_OBJS_MP3_CP1 += uio.o

TREK_OBJS_MP3_CP2 = $(addprefix mp3_cp2_, $(TREK_OBJS))
TREK_OBJS_MP3_CP2 += mp3_cp2_user_string.o
TREK_OBJS_MP3_CP2 += mp3_cp2_syscall.o
TREK_OBJS_MP3_CP2 += mp3_cp2_heap.o
TREK_OBJS_MP3_CP2 += start.o

TREK_OBJS_MP2 = mp2_trek.o \
			mp2_glue.o

all: trek_mp0.o

# MP0
trek_mp0.o: trek.c
	$(CC) $(CFLAGS) -DAEE0 -o $@ -c $<

# MP2
mp2_trek.o: trek.c
	$(CC) $(CFLAGS) -DMP2 -o $@ -c $<

mp2_glue.o: glue.c
	$(CC) $(CFLAGS) -DMP2 -o $@ -c $<

trek_mp2.o: $(TREK_OBJS_MP2)
	$(LD) -r -o $@ $^

# MP3CP1
mp3_cp1_%.o: %.c
	$(CC) $(CFLAGS) -DAEE31 -o $@ -c $<

mp3_cp1_%.o: %.s
	$(AS) $(ASFLAGS) -o $@ -c $<

trek_mp3_cp1.elf: $(TREK_OBJS_MP3_CP1)
	$(LD) -T no_umode.ld -o $@ $^

# MP3CP2
mp3_cp2_%.o: %.c
	$(CC) $(CFLAGS) -DAEE32 -o $@ -c $<

%.s: %.S
	$(CPP) $(CPPFLAGS) -o $@ -c $<

mp3_cp2_%.o: %.s
	$(AS) $(ASFLAGS) -o $@ -c $<

trek_mp3_cp2.elf: $(TREK_OBJS_MP3_CP2)
	$(LD) -T umode.ld -o $@ $^


clean:
	rm -rf *.o *.elf *.asm
