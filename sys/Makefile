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
	start.o \
	sbi.o \
	misc.o \
	console.o \
	dev/uart.o \
	dev/rtc.o \
	dev/virtio.o \
	dev/viorng.o \
	string.o \
	plic.o \
	string.o \
	error.o \
	thrasm.o \
	trap.o \
	board/qemu-virt.o

MP2_OBJS = \
	$(BASE_OBJS) \
	mp2-main.o \
	mp2-thread.o \
	mp2-timer.o \
	mp2-device.o \
	mp2-intr.o \
	mp2-excp.o \
	mp2-heap.o \
	mp2-io.o 

MP3_OBJS = \
	$(BASE_OBJS) \
	main.o \
	dev/vioblk.o \
	filesys.o \
	trap.o \
	excp.o \
	heap.o \
	device.o \
	elf.o \
	intr.o \
	memory.o \
	process.o \
	thread.o \
	cache.o \
	timer.o \
	device.o \
	io.o \
	syscall.o \
	fs/ngfs.o \
	fs/ktfs.o 

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

LDFLAGS = -melf64lriscv

BASE_QEMUOPTS = -global virtio-mmio.force-legacy=false
BASE_QEMUOPTS += -machine virt -nographic
BASE_QEMUOPTS += -object rng-random,filename=/dev/urandom,id=rng0
BASE_QEMUOPTS += -device virtio-rng-device,rng=rng0
BASE_QEMUOPTS += -serial mon:stdio
BASE_QEMUOPTS += -bios bios/osbi181q.bin

MP2_QEMUOPTS = $(BASE_QEMUOPTS)
MP2_QEMUOPTS += -serial pty
MP2_QEMUOPTS += -serial pty

MP3_QEMUOPTS = $(BASE_QEMUOPTS)
MP3_QEMUOPTS += -serial pty
MP3_QEMUOPTS += -serial pty
MP3_QEMUOPTS += -device virtio-blk-device,drive=blk0
MP3_QEMUOPTS += -drive file=fs/ngfs.raw,id=blk0,if=none,format=raw,readonly=false
MP3_QEMUOPTS += -device virtio-blk-device,drive=blk1
MP3_QEMUOPTS += -drive file=fs/ktfs.raw,id=blk1,if=none,format=raw,readonly=false

MP3X_QEMUOPTS = $(MP3_QEMUOPTS)
MP3X_QEMUOPTS += -device virtio-gpu-device -display gtk

all: mp2-kernel.elf # mp3-kernel.elf

clean:
	rm -rf *.o dev/*.o test/*.o fs/*.o *.elf

# MP2
#

mp2-kernel.elf: $(MP2_OBJS)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-mp2: mp2-kernel.elf
	$(QEMU) $(MP2_QEMUOPTS) -m 8M -kernel $<

debug-mp2: mp2-kernel.elf
	$(QEMU) $(MP2_QEMUOPTS) -m 8M -kernel $< -S -s

mp2-%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) -DMP2 $<

# MP3
#

mp3-kernel.elf: $(MP3_OBJS) main.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-mp3: mp3-kernel.elf
	$(QEMU) $(MP3_QEMUOPTS) -m 8M -kernel $<

debug-mp3: mp3-kernel.elf
	$(QEMU) $(MP3_QEMUOPTS) -m 8M -kernel $< -S -s

BLOB_OBJCOPY_FLAGS = \
	--add-section .rodata.blob=blob.raw \
	--set-section-flags .rodata.blob=alloc,contents,load,readonly

blob.o:
	echo .end | $(AS) $(ASFLAGS) -o blob.o
	[ ! -f blob.raw ] || $(OBJCOPY) $(BLOB_OBJCOPY_FLAGS) $@
