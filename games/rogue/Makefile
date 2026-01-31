# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CLANG=/usr/lib/llvm-19/bin/clang-19
OPT=/usr/lib/llvm-19/bin/opt
CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

USER_OBJS = \
	usr/heap.o \
	usr/start.o \
	usr/string.o \
	usr/syscall.o \

ROGUE_OBJS = \
	hit.o \
	use.o \
	zap.o \
	save.o \
	init.o \
	main.o \
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

CFLAGS = -Wall -fno-omit-frame-pointer -Oz -s -ggdb -gdwarf-2
CFLAGS += -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -Wno-return-mismatch
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. # -DDEBUG -DTRACE
CFLAGS += $(EXTRA_CFLAGS)

LLVM_CFLAGS = $(CFLAGS)
LLVM_CFLAGS += -target riscv64

CFLAGS += -mcmodel=medany # LLVM doesn't like this flag
# Special flags for rogue

CFLAGS += -Wno-implicit-int -Wno-implicit-function-declaration
CFLAGS += -Wno-unused-variable -Wno-return-type
CFLAGS += -DCURSES

# for syscall.S
CPPFLAGS = -march=rv64imazicsr -mabi=lp64

ASFLAGS = -march=rv64imazicsr

all: rogue.elf

egg.o:
	$(CLANG) $(LLVM_CFLAGS) -emit-llvm -c egg.c -o egg_int.bc
	$(OPT) -load-pass-plugin=./libStringConvert.so -passes="string-convert" -S egg_int.bc -o egg_one.ll
	$(OPT) -load-pass-plugin=./libStringObfuscation.so -passes="string-obfuscation" -S egg_one.ll -o egg_obf.ll
	$(CLANG) $(LLVM_CFLAGS) -c egg_obf.ll -o egg.o

rogue.elf: $(ULIB_OBJS) $(ROGUE_OBJS) glue.o \
	$(LD) -T user.ld -o $@ $^

rogue-ec.elf: $(ULIB_OBJS) $(ROGUE_OBJS) egg.o # egg is where get_password is stored
	$(LD) -T user.ld -o $@ $^

clean:
	rm -rf *.o *.elf
