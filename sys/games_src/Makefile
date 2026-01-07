# Makefile for games 
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
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64im -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.

ASFLAGS += -march=rv64im -mabi=lp64

all: # rule30.o trek.o
	@echo "*** This build currently doesn't work :("
	@echo "*** Copying pre-built rule30.o and trek.o from ../../distr/games"
	cp ../../distr/games/trek.o .
	cp ../../distr/games/rule30.o .

rule30.o: rule30.c
	$(CC) $(CFLAGS) -o $@ -c $<

trek.o: trek.c trek-glue.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf *.o
