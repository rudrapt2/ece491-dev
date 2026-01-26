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
	../../sbi.o \
	../panic-stubs.o

CFLAGS = -Wall -Werror=implicit-function-declaration -Wno-unused-function
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
QEMUOPTS += -machine virt -nographic

QEMUOPTS += -serial mon:stdio

run-all: \
	run-thread-returns \
	run-yield-once

thread-returns.elf: $(OBJS) thread-returns.o ../fake-memory.o ../fake-timer.o
	$(LD) $(LDFLAGS) -T ../../board/qemu-virt.ld -o $@ $^

run-thread-returns: thread-returns.elf
	timeout --foreground 5s $(QEMU) $(QEMUOPTS) -m 8M -kernel $<

yield-once.elf: $(OBJS) yield-once.o ../fake-memory.o ../fake-timer.o
	$(LD) $(LDFLAGS) -T ../../board/qemu-virt.ld -o $@ $^

run-yield-once: yield-once.elf
	timeout --foreground 5s $(QEMU) $(QEMUOPTS) -m 8M -kernel $<

yield-twice.elf: $(OBJS) yield-twice.o ../fake-memory.o ../fake-timer.o
	$(LD) $(LDFLAGS) -T ../../board/qemu-virt.ld -o $@ $^

run-yield-twice: yield-twice.elf
	timeout --foreground 5s $(QEMU) $(QEMUOPTS) -m 8M -kernel $<

yield-hotpotato.elf: $(OBJS) yield-hotpotato.o ../fake-memory.o ../fake-timer.o
	$(LD) $(LDFLAGS) -T ../../board/qemu-virt.ld -o $@ $^

run-yield-hotpotato: yield-hotpotato.elf
	timeout --foreground 5s $(QEMU) $(QEMUOPTS) -m 8M -kernel $<

correct-tids.elf: $(OBJS) correct-tids.o ../fake-memory.o ../fake-timer.o
	$(LD) $(LDFLAGS) -T ../../board/qemu-virt.ld -o $@ $^

correct-tids: correct-tids.elf
	timeout --foreground 5s $(QEMU) $(QEMUOPTS) -m 8M -kernel $<

clean:
	rm -rf *.o ../*.o *.elf

