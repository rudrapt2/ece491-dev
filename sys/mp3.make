# mp3.make - MP3 makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump
UNIFDEF=unifdef

OBJS = \
	start.o \
	main.o \
	board/qvirt.o \
	console.o \
	dev/uart.o \
	dev/rtc.o \
	dev/virtio.o \
	dev/viorng.o \
	dev/vioblk.o \
	string.o \
	plic.o \
	string.o \
	error.o \
	thread.o \
	thrasm.o \
	trap.o \
	filesys.o \
	trap.o \
	excp.o \
	intr.o \
	heap.o \
	misc.o \
	device.o \
	elf.o \
	memory.o \
	process.o \
	cache.o \
	timer.o \
	device.o \
	sbi.o \
	fs/ngfs.o

CFLAGS = -Wall -Werror=implicit-function-declaration -Wno-unused-function
CFLAGS += -fno-omit-frame-pointer -ggdb3 -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.

# CFLAGS += -DDEBUG -DTRACE # Everything!
# CFLAGS += -DMEMORY_DEBUG -DMEMORY_TRACE
# CFLAGS += -DHEAP_DEBUG -DHEAP_TRACE
# CFLAGS += -DPROCESS_DEBUG -DPROCESS_TRACE
# CFLAGS += -DMAIN_DEBUG -DMAIN_TRACE
# CFLAGS += -DSYSCALL_DEBUG -DSYSCALL_TRACE
# CFLAGS += -DTHREAD_DEBUG -DTHREAD_TRACE
# CFLAGS += -DTIMER_DEBUG -DTIMER_TRACE
# CFLAGS += -DCACHE_DEBUG -DCACHE_TRACE
# CFLAGS += -DELF_DEBUG -DELF_TRACE


ASFLAGS = -march=rv64imazicsr

LDFLAGS = -melf64lriscv -T board/qvirt.ld

QEMUOPTS = -global virtio-mmio.force-legacy=false
QEMUOPTS += -machine virt -nographic
QEMUOPTS += -object rng-random,filename=/dev/urandom,id=rng0
QEMUOPTS += -device virtio-rng-device,rng=rng0
QEMUOPTS += -bios bios/osbi181q.bin
QEMUOPTS += -serial mon:stdio
QEMUOPTS += -serial pty
QEMUOPTS += -serial pty
QEMUOPTS += -device virtio-blk-device,drive=blk0
QEMUOPTS += -drive file=ngfs.raw,id=blk0,if=none,format=raw,readonly=false

VIDEO_QEMUOPTS = $(QEMUOPTS)
VIDEO_QEMUOPTS += -device virtio-gpu-device -display gtk

all: kernel.elf

clean:
	rm -rf board/*.o dev/*.o fs/*.o *.o *.elf

kernel.elf: $(OBJS) blob.o
	$(LD) $(LDFLAGS) -o $@ $^

run: kernel.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

debug: kernel.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -S -s

run-video: kernel.elf
	$(QEMU) $(VIDEO_QEMUOPTS) -m 16M -kernel $<

BLOB_OBJCOPY_FLAGS = \
	--add-section .rodata.blob=blob.raw \
	--set-section-flags .rodata.blob=alloc,contents,load,readonly

blob.o:
	echo .end | $(AS) $(ASFLAGS) -o blob.o
	[ ! -f blob.raw ] || $(OBJCOPY) $(BLOB_OBJCOPY_FLAGS) $@

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $<
