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

RULE30_OBJS = rule30.o

all: rule30.o

rule30.o: rule30.c
	$(CC) $(CFLAGS) -o $@ -c $< -DAEE2

%.s: %.S
	$(CPP) $(CPPFLAGS) -o $@ -c $<

clean:
	rm -rf *.o *.elf *.asm
