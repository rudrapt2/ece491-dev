# Makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

OBJS = \
	../../misc.o \
	../../console.o \
	../../dev/uart.o \
	../../error.o \
	../../excp.o \
	../../heap.o \
	../../see.o \
	../../start.o \
	../../string.o \
	../../thrasm.o \
	../../thread.o \
	../../trap.o \
	../stubs.o

CFLAGS = -Wall -Werror=implicit-function-declaration
CFLAGS += -fno-omit-frame-pointer -ggdb3 -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I. -I../..

# CFLAGS += -DDEBUG -DTRACE # Everything!
# CFLAGS += -DMEMORY_DEBUG -DMEMORY_TRACE
# CFLAGS += -DHEAP_DEBUG -DHEAP_TRACE
# CFLAGS += -DEZFS_DEBUG -DEZFS_TRACE
# CFLAGS += -DLOCK_DEBUG -DLOCK_TRACE
# CFLAGS += -DPROCESS_DEBUG -DPROCESS_TRACE
# CFLAGS += -DMAIN_DEBUG -DMAIN_TRACE
# CFLAGS += -DSYSCALL_DEBUG -DSYSCALL_TRACE
# CFLAGS += -DUIO_DEBUG -DUIO_TRACE
# CFLAGS += -DTHREAD_DEBUG -DTHREAD_TRACE
# CFLAGS += -DTIMER_DEBUG -DTIMER_TRACE
# CFLAGS += -DVIOGPU_DEBUG -DVIOGPU_TRACE
# CFLAGS += -DCACHE_DEBUG -DCACHE_TRACE
# CFLAGS += -DKTFS_DEBUG -DKTFS_TRACE
# CFLAGS += -DELF_DEBUG -DELF_TRACE
# CFLAGS += -DTARFS_DEBUG -DTARFS_TRACE

ASFLAGS = -march=rv64imazicsr

LDFLAGS = -melf64lriscv

QEMUOPTS = -global virtio-mmio.force-legacy=false
QEMUOPTS += -machine virt -bios none -nographic

QEMUOPTS += -serial mon:stdio

run-all: \
	run-create-thread-returns \
	run-create-yield-once

create-thread-returns.elf: $(OBJS) create-thread-returns.o
	$(LD) $(LDFLAGS) -T ../../kernel.ld -o $@ $^

run-create-thread-returns: create-thread-returns.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

create-yield-once.elf: $(OBJS) create-yield-once.o
	$(LD) $(LDFLAGS) -T ../../kernel.ld -o $@ $^

run-create-yield-once: create-yield-once.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

clean:
	rm -rf *.o ../*.o *.elf

