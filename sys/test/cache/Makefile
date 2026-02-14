# Makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump
UNIFDEF=unifdef

BASE_OBJS = \
	../../start.o \
	../../sbi.o \
	../../misc.o \
	../../console.o \
	../../dev/uart.o \
	../../dev/rtc.o \
	../../dev/virtio.o \
	../../dev/viorng.o \
	../../string.o \
	../../plic.o \
	../../string.o \
	../../error.o \
	../../thrasm.o \
	../../trap.o \
	../../board/qvirt.o

MP3_OBJS = \
	$(BASE_OBJS) \
	../../dev/vioblk.o \
	../../filesys.o \
	../../trap.o \
	../../excp.o \
	../../heap.o \
	../../device.o \
	../../elf.o \
	../../intr.o \
	../../memory.o \
	../../process.o \
	../../thread.o \
	../../cache.o \
	../../timer.o \
	../../device.o \
	../../io.o \
	../../syscall.o \
	../../fs/ngfs.o \
	../../fs/ktfs.o 

CFLAGS = -Wall -Werror=implicit-function-declaration -Wno-unused-function
CFLAGS += -fno-omit-frame-pointer -ggdb3 -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.

# CFLAGS += -DDEBUG -DTRACE # Everything!

# CFLAGS += -DMEMORY_DEBUG -DMEMORY_TRACE
# CFLAGS += -DHEAP_DEBUG -DHEAP_TRACE

# CFLAGS += -DTHREAD_DEBUG -DTHREAD_TRACE
# CFLAGS += -DLOCK_DEBUG -DLOCK_TRACE
# CFLAGS += -DPROCESS_DEBUG -DPROCESS_TRACE
# CFLAGS += -DSYSCALL_DEBUG -DSYSCALL_TRACE
# CFLAGS += -DELF_DEBUG -DELF_TRACE

# CFLAGS += -DUIO_DEBUG -DUIO_TRACE
# CFLAGS += -DVIRTIO_DEBUG -DVIRTIO_TRACE
# CFLAGS += -DTIMER_DEBUG -DTIMER_TRACE
# CFLAGS += -DVIOGPU_DEBUG -DVIOGPU_TRACE
# CFLAGS += -DVIOBLK_DEBUG -DVIOBLK_TRACE

# CFLAGS += -DCACHE_DEBUG -DCACHE_TRACE
# CFLAGS += -DKTFS_DEBUG -DKTFS_TRACE
# CFLAGS += -DNGFS_DEBUG -DNGFS_TRACE

# CFLAGS += -DMAIN_DEBUG -DMAIN_TRACE

ASFLAGS = -march=rv64imazicsr

LDFLAGS = -melf64lriscv -T ../../board/qvirt.ld

BASE_QEMUOPTS = -global virtio-mmio.force-legacy=false
BASE_QEMUOPTS += -machine virt -nographic
BASE_QEMUOPTS += -serial mon:stdio
BASE_QEMUOPTS += -bios ../../bios/osbi181q.bin

all: test.elf

clean:
	rm -rf *.o ../../*.o ../../dev/*.o ../*.o ../../fs/*.o *.elf

test.elf: $(MP3_OBJS) main.o blob.o
	$(LD) $(LDFLAGS) -o $@ $^

run-test: test.elf
	$(QEMU) $(BASE_QEMUOPTS) -m 8M -kernel $<

debug-test: test.elf
	$(QEMU) $(BASE_QEMUOPTS) -m 8M -kernel $< -S -s

BLOB_OBJCOPY_FLAGS = \
	--add-section .rodata.blob=blob.raw \
	--set-section-flags .rodata.blob=alloc,contents,load,readonly

blob.o:
	echo .end | $(AS) $(ASFLAGS) -o blob.o
	[ ! -f blob.raw ] || $(OBJCOPY) $(BLOB_OBJCOPY_FLAGS) $@
