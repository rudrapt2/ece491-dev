# Utility to generate mp2 gold and mp2 release


# =========================
# User-configurable variables
# =========================

# Set this to the path to your glycine max repo
SRC_DIR     := /home/rudrapt2/sp26_ece391/glycine-max
# this is the directory in which the output files will be generated
TARGET_DIR  := /home/rudrapt2/tmp/mp2-sp26-release

# Space-separated list of file names (no paths)
FILE_LIST   := \
	sys/bios/osbi181q.bin \
	sys/board/qemu-virt.ld \
	sys/board/qemu-virt.c \
	sys/dev/rtc.c \
	sys/dev/uart.c \
	sys/dev/viorng.c \
	sys/dev/virtio.c \
	sys/dev/virtio.h \
	sys/conf.h \
	sys/console.c \
	sys/console.h \
	sys/device.c \
	sys/device.h \
	sys/error.c \
	sys/error.h \
	sys/excp.c \
	sys/fsimpl.h \
	sys/heap.c \
	sys/heap.h \
	sys/intr.c \
	sys/intr.h \
	sys/io.c \
	sys/io.h \
	sys/ioimpl.h \
	sys/main.c \
	sys/Makefile \
	sys/misc.c \
	sys/misc.h \
	sys/plic.c \
	sys/plic.h \
	sys/riscv.h \
	sys/sbi.h \
	sys/sbi.s \
	sys/scnum.h \
	sys/start.s \
	sys/string.c \
	sys/string.h \
	sys/thrasm.s \
	sys/thread.c \
	sys/thread.h \
	sys/timer.c \
	sys/timer.h \
	sys/trap.h \
	sys/trap.s

# =========================
# Internal variables
# =========================

SRC_FILES    := $(addprefix $(SRC_DIR)/,$(FILE_LIST))
TARGET_FILES := $(addprefix $(TARGET_DIR)/,$(FILE_LIST))

# =========================
# Phony targets
# =========================

.PHONY: mp2_gold mp2_release copy_files

# -------------------------
# Copy rule
# -------------------------

copy_files: $(TARGET_FILES)

$(TARGET_DIR)/%: $(SRC_DIR)/%
	@mkdir -p $(dir $@)
	cp $< $@

# -------------------------
# Targets
# -------------------------

UNIFDEF_FILES := $(filter %.c %.h,$(TARGET_FILES))


# note: unifdef returns 1 if any files were changed. You may see an error with no message; this is acceptable.
mp2_gold: copy_files
	@echo "Running unifdef for mp2_gold (MP2 defined)"
	unifdef -DMP2 -m $(UNIFDEF_FILES)

mp2_release: copy_files
	@echo "Running unifdef for mp2_release (MP2 + STUDENT defined)"
	unifdef -DMP2 -DSTUDENT -m $(UNIFDEF_FILES)
