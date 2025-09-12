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
	assert.o \
	console.o \
	dev/uart.o \
	dev/rtc.o \
	dev/virtio.o \
	dev/viorng.o \
	device.o \
	filesys.o \
	elf.o \
	error.o \
	excp.o \
	heap0.o \
	intr.o \
	uio.o \
	memory.o \
	plic.o \
	process.o \
	see.o \
	start.o \
	string.o \
	thrasm.o \
	thread.o \
	timer.o \
	trap.o
#	syscall.o \
#	dev/viogpu.o \
#	dev/viorng.o \
#	dev/vioblk.o \
#	cache.o \
#	ktfs.o
#   tarfs.o 

CFLAGS = -Wall -Werror=implicit-function-declaration
CFLAGS += -fno-omit-frame-pointer -ggdb3 -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.

# CFLAGS += -DDEBUG -DTRACE # Everything!
#CFLAGS += -DMEMORY_DEBUG -DMEMORY_TRACE
#CFLAGS += -DHEAP_DEBUG -DHEAP_TRACE
#CFLAGS += -DEZFS_DEBUG -DEZFS_TRACE
#CFLAGS += -DLOCK_DEBUG -DLOCK_TRACE
#CFLAGS += -DPROCESS_DEBUG -DPROCESS_TRACE
#CFLAGS += -DMAIN_DEBUG -DMAIN_TRACE
#CFLAGS += -DSYSCALL_DEBUG -DSYSCALL_TRACE
#CFLAGS += -DTHREAD_DEBUG -DTHREAD_TRACE
#CFLAGS += -DTIMER_DEBUG -DTIMER_TRACE
#CFLAGS += -DVIOGPU_DEBUG -DVIOGPU_TRACE
#CFLAGS += -DCACHE_DEBUG -DCACHE_TRACE
#CFLAGS += -DKTFS_DEBUG -DKTFS_TRACE
#CFLAGS += -DELF_DEBUG -DELF_TRACE
#CFLAGS += -DTARFS_DEBUG -DTARFS_TRACE


ASFLAGS = -march=rv64imazicsr

LDFLAGS = -melf64lriscv

QEMUOPTS = -global virtio-mmio.force-legacy=false
QEMUOPTS += -machine virt -bios none -nographic

# the first rng-random is for reading "true" randomness from /dev/urandom
# the second one is for testing if they are correctly reading from a source
# of randomness with the entropy device
QEMUOPTS += -object rng-random,filename=/dev/urandom,id=rng0
# QEMUOPTS += -object rng-random,filename=test/rand,id=rng0
QEMUOPTS += -device virtio-rng-device,rng=rng0
QEMUOPTS += -serial mon:stdio
QEMUOPTS += -serial pty
QEMUOPTS += -serial pty
QEMUOPTS += -serial pty
QEMUOPTS += -drive file=ktfs.raw,id=blk0,if=none,format=raw,readonly=false
QEMUOPTS += -device virtio-blk-device,drive=blk0
#QEMUOPTS += -device virtio-gpu-device -display gtk

all: kernel.elf

kernel.elf: $(OBJS) main.o blob.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run: kernel.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

debug: kernel.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -S -s

BLOB_OBJCOPY_FLAGS = \
	--add-section .rodata.blob=blob.raw \
	--set-section-flags .rodata.blob=alloc,contents,load,readonly

blob.o:
	echo .end | $(AS) $(ASFLAGS) -o blob.o
	[ ! -f blob.raw ] || $(OBJCOPY) $(BLOB_OBJCOPY_FLAGS) $@

clean:
	rm -rf *.o dev/*.o test/*.o demo/*.o *.elf

# TEST TARGETS
#

test-heap.elf: $(OBJS) test/heap.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-heap: test-heap.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

test-string.elf: $(OBJS) test/string.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-string: test-string.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

test-thread.elf: $(OBJS) test/thread.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-thread: test-thread.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

test-pipe.elf: $(OBJS) test/pipe.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-pipe: test-pipe.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

test-tarfs.elf: $(OBJS) test/tarfs.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

TEST_TARFS_QEMUOPTS = $(QEMUOPTS)
TEST_TARFS_QEMUOPTS += -drive file=test/testfs.tar,id=blk0,if=none,format=raw,readonly=false
TEST_TARFS_QEMUOPTS += -device virtio-blk-device,drive=blk0

test-tarfs: test-tarfs.elf
	$(QEMU) $(TEST_TARFS_QEMUOPTS) -m 8M -kernel $<

demo-uart-echo.elf: $(OBJS) demo/uart-echo.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

demo-uart-echo: demo-uart-echo.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -serial pty

test-viorng.elf: $(OBJS) test/viorng.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-viorng: test-viorng.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< | python3 test/viorng.py

TEST_KTFS_QEMUOPTS = $(QEMUOPTS)
TEST_KTFS_QEMUOPTS += -drive file=test/ktfs.raw,id=blk0,if=none,format=raw,readonly=false
TEST_KTFS_QEMUOPTS += -device virtio-blk-device,drive=blk0

test-readat-ktfs.elf: $(OBJS) test/ktfs_test_readat.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-readat-ktfs: test-readat-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< 
	
debug-ktfs: test-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< -S -s

test-read.elf: $(OBJS) test/test_read.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-read.elf: CFLAGS += $(MY_CFLAGS)

test-read: test-read.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

test-read-debug: test-read.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< -S -s

test-write-ktfs.elf: $(OBJS) test/ktfs_test_write.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-write-ktfs: test-write-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< 
	
debug-write-ktfs: test-write-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< -S -s

test-create-ktfs.elf: $(OBJS) test/ktfs_test_create.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-create-ktfs: test-create-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< 
	
debug-create-ktfs: test-create-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< -S -s

test-delete-ktfs.elf: $(OBJS) test/ktfs_test_delete.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-delete-ktfs: test-delete-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< 
	
debug-delete-ktfs: test-delete-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< -S -s

# Using heap0.c without memory management
# ELF_LD_OBJS += $(OBJS)
ELF_LD_OBJS = $(filter-out heap0.o, $(OBJS))
ELF_LD_OBJS += cp1testcases/elfloadTest/elf_gold.o
ELF_LD_OBJS += cp1testcases/elfloadTest/heap0.o

TEST_ELFLD_QEMUOPTS = $(QEMUOPTS)
TEST_ELFLD_QEMUOPTS += -drive file=cp1testcases/elfloadTest/ktfs.raw,id=blk0,if=none,format=raw,readonly=false
TEST_ELFLD_QEMUOPTS += -device virtio-blk-device,drive=blk0

elf_test.elf: $(ELF_LD_OBJS) cp1testcases/elfloadTest/elfloadTest.o 
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

elf_test: elf_test.elf
	$(QEMU) $(TEST_ELFLD_QEMUOPTS) -m 8M -kernel $<
	
debug-elf_test: elf_test.elf
	$(QEMU) $(TEST_ELFLD_QEMUOPTS) -m 8M -kernel $< -S -s

ZORK_QEMUOPTS = $(TEST_KTFS_QEMUOPTS)
ZORK_QEMUOPTS += -serial pty
ZORK_QEMUOPTS += -monitor pty

run-zork.elf: $(OBJS) demo/run-zork.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-zork: run-zork.elf
	$(QEMU) $(ZORK_QEMUOPTS) -m 8M -kernel $< 
	
debug-zork: run-zork.elf
	$(QEMU) $(ZORK_QEMUOPTS) -m 8M -kernel $< -S -s

TREK_QEMUOPTS = $(ZORK_QEMUOPTS)
run-trek.elf: $(OBJS) demo/run-trek.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-trek: run-trek.elf
	$(QEMU) $(TREK_QEMUOPTS) -m 8M -kernel $< 
	
debug-trek: run-trek.elf
	$(QEMU) $(TREK_QEMUOPTS) -m 8M -kernel $< -S -s

TREK_USER_QEMUOPTS = $(ZORK_QEMUOPTS)
run-trek-user.elf: $(OBJS) demo/run-trek-user.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-trek-user: run-trek-user.elf
	$(QEMU) $(TREK_USER_QEMUOPTS) -m 8M -kernel $< 
	
debug-trek-user: run-trek-user.elf
	$(QEMU) $(TREK_USER_QEMUOPTS) -m 8M -kernel $< -S -s



memio-readat.elf: $(OBJS) cp1testcases/memio_readat.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-memio-readat: memio-readat.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

debug-memio-readat: memio-readat.elf
	$(QEMU) $(TREK_USER_QEMUOPTS) -m 8M -kernel $< -S -s



memio-writeat.elf: $(OBJS) cp1testcases/memio_writeat.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-memio-writeat: memio-writeat.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

debug-memio-writeat: memio-writeat.elf
	$(QEMU) $(TREK_USER_QEMUOPTS) -m 8M -kernel $< -S -s


memio-ctl.elf: $(OBJS) cp1testcases/memio_ctl.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-memio-ctl: memio-ctl.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

debug-memio-ctl: memio-ctl.elf
	$(QEMU) $(TREK_USER_QEMUOPTS) -m 8M -kernel $< -S -s

test-readat-ktfs-block-2: test-readat-ktfs-block-2.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

test-readat-ktfs-non-block.elf : $(OBJS) cp1testcases/ktfs_test_readat_non_block.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-readat-ktfs-non-block: test-readat-ktfs-non-block.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

test-readat-ktfs-indirect.elf: $(OBJS) cp1testcases/ktfs_test_readat_indirect.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-readat-ktfs-indirect: test-readat-ktfs-indirect.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

test-readat-ktfs-dindirect.elf: $(OBJS) cp1testcases/ktfs_test_readat_dindirect.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-readat-ktfs-dindirect: test-readat-ktfs-dindirect.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

test-readat-ktfs-direct_to_indirect.elf: $(OBJS) cp1testcases/ktfs_test_readat_direct_to_indirect.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-readat-ktfs-direct_to_indirect: test-readat-ktfs-direct_to_indirect.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

test-readat-ktfs-two-files.elf: $(OBJS) cp1testcases/ktfs_test_readat_two_files.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-readat-ktfs-two-files: test-readat-ktfs-two-files.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $<

test-cntl-ktfs.elf: $(OBJS) cp1testcases/ktfs_test_cntl.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-cntl-ktfs: test-cntl-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< 

test-close-ktfs.elf: $(OBJS) cp1testcases/ktfs_test_close.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-close-ktfs: test-close-ktfs.elf
	$(QEMU) $(TEST_KTFS_QEMUOPTS) -m 8M -kernel $< 
	

# CP2_TESTSUITES_FOR_KTFS

TEST_CP2_KTFS_QEMUOPTS = $(QEMUOPTS)
TEST_CP2_KTFS_QEMUOPTS += -drive file=cp2testcases/ktfs.raw,id=blk0,if=none,format=raw,readonly=false
TEST_CP2_KTFS_QEMUOPTS += -device virtio-blk-device,drive=blk0

test-cp2-ktfs: test-writeat-ktfs

test-cp2-writeat-ktfs.elf: $(OBJS) cp2testcases/ktfs_test_writeat.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-cp2-writeat-ktfs: test-cp2-writeat-ktfs.elf
	$(QEMU) $(TEST_CP2_KTFS_QEMUOPTS) -m 8M -kernel $<

test-cp2-write-persists-ktfs.elf: $(OBJS) cp2testcases/ktfs_test_write_persists.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-cp2-write-persists-ktfs: test-cp2-write-persists-ktfs.elf
	$(QEMU) $(TEST_CP2_KTFS_QEMUOPTS) -m 8M -kernel $<

test-cp2-create-ktfs.elf: $(OBJS) cp2testcases/ktfs_test_create.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-cp2-create-ktfs: test-cp2-create-ktfs.elf
	$(QEMU) $(TEST_CP2_KTFS_QEMUOPTS) -m 8M -kernel $<

debug-cp2-create-ktfs: test-cp2-create-ktfs.elf
	$(QEMU) $(TEST_CP2_KTFS_QEMUOPTS) -m 8M -kernel $< -S -s

test-cp2-delete-ktfs.elf: $(OBJS) cp2testcases/ktfs_test_delete.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-cp2-delete-ktfs: test-cp2-delete-ktfs.elf
	$(QEMU) $(TEST_CP2_KTFS_QEMUOPTS) -m 8M -kernel $<

AG_VIOBLK_TEST_READAT_SIMPLE_0_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_READAT_SIMPLE_0_QEMUOPTS += -drive file=test/ag_cp1_drives/vioblk_test_readat_simple_0_drive,id=blk0,if=none,format=raw,readonly=false
AG_VIOBLK_TEST_READAT_SIMPLE_0_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-readat-simple-0.elf: $(OBJS) test/vioblk_test_readat_simple_0.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-readat-simple-0: ag-vioblk-test-readat-simple-0.elf
	$(QEMU) $(AG_VIOBLK_TEST_READAT_SIMPLE_0_QEMUOPTS) -m 8M -kernel $<

AG_VIOBLK_TEST_READAT_SIMPLE_1_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_READAT_SIMPLE_1_QEMUOPTS += -drive file=test/ag_cp1_drives/vioblk_test_readat_simple_1_drive,id=blk0,if=none,format=raw,readonly=false
AG_VIOBLK_TEST_READAT_SIMPLE_1_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-readat-simple-1.elf: $(OBJS) test/vioblk_test_readat_simple_1.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-readat-simple-1: ag-vioblk-test-readat-simple-1.elf
	$(QEMU) $(AG_VIOBLK_TEST_READAT_SIMPLE_1_QEMUOPTS) -m 8M -kernel $<



AG_VIOBLK_TEST_READAT_SIMPLE_2_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_READAT_SIMPLE_2_QEMUOPTS += -drive file=test/ag_cp1_drives/vioblk_test_readat_simple_2_drive,id=blk0,if=none,format=raw,readonly=false
AG_VIOBLK_TEST_READAT_SIMPLE_2_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-readat-simple-2.elf: $(OBJS) test/vioblk_test_readat_simple_2.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-readat-simple-2: ag-vioblk-test-readat-simple-2.elf
	$(QEMU) $(AG_VIOBLK_TEST_READAT_SIMPLE_2_QEMUOPTS) -m 8M -kernel $<


AG_VIOBLK_TEST_WRITEAT_SIMPLE_0_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_WRITEAT_SIMPLE_0_QEMUOPTS += -drive file=test/ag_test_drive/vioblk_test_writeat_simple_0,id=blk0,if=none,format=raw,readonly=false 
AG_VIOBLK_TEST_WRITEAT_SIMPLE_0_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-writeat-simple-0.elf: $(OBJS) test/vioblk_test_writeat_simple_0.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-writeat-simple-0: ag-vioblk-test-writeat-simple-0.elf
	$(QEMU) $(AG_VIOBLK_TEST_WRITEAT_SIMPLE_0_QEMUOPTS) -m 8M -kernel $<

debug-ag-vioblk-test-writeat-simple-0: ag-vioblk-test-writeat-simple-0.elf
	$(QEMU) $(AG_VIOBLK_TEST_WRITEAT_SIMPLE_0_QEMUOPTS) -m 8M -kernel $< -S -s

AG_VIOBLK_TEST_WRITEAT_SIMPLE_1_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_WRITEAT_SIMPLE_1_QEMUOPTS += -drive file=test/ag_test_drive/vioblk_test_writeat_simple_1,id=blk0,if=none,format=raw,readonly=false 
AG_VIOBLK_TEST_WRITEAT_SIMPLE_1_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-writeat-simple-1.elf: $(OBJS) test/vioblk_test_writeat_simple_1.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-writeat-simple-1: ag-vioblk-test-writeat-simple-1.elf
	$(QEMU) $(AG_VIOBLK_TEST_WRITEAT_SIMPLE_1_QEMUOPTS) -m 8M -kernel $<

debug-ag-vioblk-test-writeat-simple-1: ag-vioblk-test-writeat-simple-1.elf
	$(QEMU) $(AG_VIOBLK_TEST_WRITEAT_SIMPLE_1_QEMUOPTS) -m 8M -kernel $< -S -s

AG_VIOBLK_TEST_WRITEAT_SIMPLE_2_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_WRITEAT_SIMPLE_2_QEMUOPTS += -drive file=test/ag_test_drive/vioblk_test_writeat_simple_2,id=blk0,if=none,format=raw,readonly=false 
AG_VIOBLK_TEST_WRITEAT_SIMPLE_2_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-writeat-simple-2.elf: $(OBJS) test/vioblk_test_writeat_simple_2.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-writeat-simple-2: ag-vioblk-test-writeat-simple-2.elf
	$(QEMU) $(AG_VIOBLK_TEST_WRITEAT_SIMPLE_2_QEMUOPTS) -m 8M -kernel $<

debug-ag-vioblk-test-writeat-simple-2: ag-vioblk-test-writeat-simple-2.elf
	$(QEMU) $(AG_VIOBLK_TEST_WRITEAT_SIMPLE_2_QEMUOPTS) -m 8M -kernel $< -S -s

AG_VIOBLK_TEST_CLOSE_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_CLOSE_QEMUOPTS += -drive file=test/ag_test_drive/vioblk_test_close_drive,id=blk0,if=none,format=raw,readonly=false
AG_VIOBLK_TEST_CLOSE_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-close.elf: $(OBJS) test/vioblk_test_close.o 
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-close: ag-vioblk-test-close.elf
	$(QEMU) $(AG_VIOBLK_TEST_CLOSE_QEMUOPTS) -m 8M -kernel $< 

debug-ag-vioblk-test-close: ag-vioblk-test-close.elf
	$(QEMU) $(AG_VIOBLK_TEST_CLOSE_QEMUOPTS) -m 8M -kernel $< -S -s

AG_VIOBLK_TEST_CNTL_QEMUOPTS = $(QEMUOPTS)
AG_VIOBLK_TEST_CNTL_QEMUOPTS += -drive file=test/ag_test_drive/vioblk_test_cntl_drive,id=blk0,if=none,format=raw,readonly=false
AG_VIOBLK_TEST_CNTL_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-vioblk-test-cntl.elf: $(OBJS) test/vioblk_test_cntl.o 
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-vioblk-test-cntl: ag-vioblk-test-cntl.elf
	$(QEMU) $(AG_VIOBLK_TEST_CNTL_QEMUOPTS) -m 8M -kernel $< 

debug-ag-vioblk-test-cntl: ag-vioblk-test-cntl.elf
	$(QEMU) $(AG_VIOBLK_TEST_CNTL_QEMUOPTS) -m 8M -kernel $< -S -s


CACHE_TEST_GET_BLOCK_0_QEMUOPTS = $(QEMUOPTS)
CACHE_TEST_GET_BLOCK_0_QEMUOPTS += -drive file=test/ag_cp1_drives/cache_drive,id=blk0,if=none,format=raw,readonly=false
CACHE_TEST_GET_BLOCK_0_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-cache-test-get-block-0.elf: $(OBJS) test/cache_test_get_block_0.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-cache-test-get-block-0.elf: ag-cache-test-get-block-0.elf
	$(QEMU) $(CACHE_TEST_GET_BLOCK_0_QEMUOPTS) -m 8M -kernel $< 



CACHE_TEST_GET_BLOCK_1_QEMUOPTS = $(QEMUOPTS)
CACHE_TEST_GET_BLOCK_1_QEMUOPTS += -drive file=test/ag_cp1_drives/cache_drive,id=blk0,if=none,format=raw,readonly=false
CACHE_TEST_GET_BLOCK_1_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-cache-test-get-block-1.elf: $(OBJS) test/cache_test_get_block_1.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-cache-test-get-block-1.elf: ag-cache-test-get-block-1.elf
	$(QEMU) $(CACHE_TEST_GET_BLOCK_1_QEMUOPTS) -m 8M -kernel $<



CACHE_TEST_RELEASE_BLOCK_0_QEMUOPTS = $(QEMUOPTS)
CACHE_TEST_RELEASE_BLOCK_0_QEMUOPTS += -drive file=test/ag_cp1_drives/cache_drive,id=blk0,if=none,format=raw,readonly=false
CACHE_TEST_RELEASE_BLOCK_0_QEMUOPTS += -device virtio-blk-device,drive=blk0
ag-cache-test-release-block-0.elf: $(OBJS) test/cache_test_release_block_0.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-ag-cache-test-release-block-0.elf: ag-cache-test-release-block-0.elf
	$(QEMU) $(CACHE_TEST_RELEASE_BLOCK_0_QEMUOPTS) -m 8M -kernel $<
