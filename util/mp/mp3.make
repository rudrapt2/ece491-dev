# Utility to generate mp3 gold and mp3 release

# =========================
# User-configurable variables
# =========================

SRC_DIR     := /home/rudrapt2/sp26_ece391/glycine-max
TARGET_DIR  := /home/rudrapt2/sp26_ece391/mp3-sp26/gold/cp2_3

# List of files to copy.
# Format: "source:destination" OR "path" (if source and dest are same)
FILES_FOR_COPYING   := \
    sys/bios/osbi181q.bin \
    sys/board/qvirt.ld \
    sys/board/qvirt.c \
    sys/dev/rtc.c \
    sys/dev/uart.c \
    sys/dev/vioblk.c \
    sys/dev/viorng.c \
    sys/dev/virtio.c \
    sys/dev/virtio.h \
    sys/fs/ngfs.c \
    sys/fs/ngfs.h \
    sys/cache.c \
    sys/cache.h \
    sys/conf.h \
    sys/console.c \
    sys/console.h \
    sys/device.c \
    sys/device.h \
    sys/elf.c \
    sys/elf.h \
    sys/error.c \
    sys/error.h \
    sys/excp.c \
    sys/filesys.c \
    sys/filesys.h \
    sys/fsimpl.h \
    sys/heap.c \
    sys/heap.h \
    sys/intr.c \
    sys/intr.h \
    sys/io.c \
    sys/io.h \
    sys/ioimpl.h \
    sys/main.c \
    sys/memory.c \
    sys/memory.h \
    sys/misc.c \
    sys/misc.h \
    sys/mp3-student.make:sys/Makefile \
    sys/plic.c \
    sys/plic.h \
    sys/process.c \
    sys/process.h \
    sys/riscv.h \
    sys/sbi.h \
    sys/sbi.s \
    sys/scnum.h \
    sys/start.s \
    sys/string.c \
    sys/string.h \
    sys/syscall.c \
    sys/thrasm.s \
    sys/thread.c \
    sys/thread.h \
    sys/timer.c \
    sys/timer.h \
    sys/trap.h \
    sys/trap.s \
    \
    usr/games/trek-mp3-cp1 \
    usr/games/trek \
    usr/games/zork \
    usr/games/rogue \
    usr/games/nudoku \
    usr/games/tetris \
    usr/progs/cat.c \
    usr/progs/date.c \
    usr/progs/echo.c \
    usr/progs/hello-mp3-cp1.c \
    usr/progs/hello.c \
    usr/progs/ls.c \
    usr/progs/rm.c \
    usr/progs/shell.c \
    usr/progs/touch.c \
    usr/progs/wc.c \
    usr/progs/xargs.c \
    usr/error.c \
    usr/error.h \
    usr/heap.c \
    usr/heap.h \
    usr/io.c \
    usr/io.h \
    usr/Makefile \
    usr/no_umode.ld \
    usr/scnum.h \
    usr/shell.h \
    usr/start.s \
    usr/string.c \
    usr/string.h \
    usr/syscall.h \
    usr/syscall.S \
    usr/umode.ld \
    util/fs/mkfs_ngfs \

# Assembly files to preprocess (Must match DESTINATION paths)
ASSEMBLY_FILES_FOR_PREPROCESSING := \
    sys/thrasm.s \
    sys/trap.s

# =========================
# Internal Functions & Variables
# =========================

GET_SRC = $(word 1, $(subst :, ,$(1)))
GET_DST = $(if $(word 2,$(subst :, ,$(1))),$(word 2,$(subst :, ,$(1))),$(1))

TARGET_FILES := $(foreach f,$(FILES_FOR_COPYING),$(TARGET_DIR)/$(call GET_DST,$(f)))
TARGET_ASM_PREPROCESSING_FILES := $(addprefix $(TARGET_DIR)/,$(ASSEMBLY_FILES_FOR_PREPROCESSING))

# -------------------------
# Macro: Run Unifdef
# -------------------------
# Usage: $(call run-unifdef, FLAGS)
# Logic: unifdef returns 1 if changes were made. We want to treat 0 and 1 as success,
# but allow 2 (syntax error) to fail the build.
# The syntax `cmd || [ $? -eq 1 ]` ensures the shell returns true (0) if unifdef returns 1.
define run-unifdef
	@echo "Running unifdef with flags: $(1)"
	@for f in $(UNIFDEF_FILES); do \
		unifdef $(1) -m "$$f" || [ $$? -eq 1 ] || { echo "unifdef failed on $$f"; exit 1; }; \
	done
endef

# =========================
# Phony targets
# =========================

.PHONY: mp3_gold_cp1 mp3_gold_cp2_3 mp3_release copy_files preprocess_assembly check_clean

# -------------------------
# Safety Prompt
# -------------------------
check_clean:
	@printf "Delete target directory ($(TARGET_DIR)) before processing? [y/N] "
	@read answer; \
	if [ "$$answer" = "y" ]; then \
		echo "Cleaning $(TARGET_DIR)..."; \
		rm -rf $(TARGET_DIR); \
	else \
		echo "Skipping clean (incremental update)."; \
	fi

# -------------------------
# Copy Rules
# -------------------------
define COPY_RULE
$(TARGET_DIR)/$(call GET_DST,$(1)): $(SRC_DIR)/$(call GET_SRC,$(1))
	@mkdir -p $$(dir $$@)
	cp $$< $$@
endef

$(foreach f,$(FILES_FOR_COPYING),$(eval $(call COPY_RULE,$(f))))

copy_files: $(TARGET_FILES)

# -------------------------
# Pre-process assembly
# -------------------------
preprocess_assembly: check_clean copy_files
	@echo "Preprocessing assembly files"
	@$(foreach file,$(TARGET_ASM_PREPROCESSING_FILES), \
		echo "  Processing $(file)"; \
		awk -v q="'" '{ \
			if ($$0 ~ /^.(ifdef|ifndef|else|endif)/) sub(/^[.]/, "#", $$0); \
			if ($$0 ~ /^#/ && $$0 !~ /^#[ \t]*(ifdef|ifndef|else|endif|define|include)/) gsub(q, "", $$0); \
			print \
		}' $(file) > $(file).tmp && mv $(file).tmp $(file); \
	)

# -------------------------
# Targets
# -------------------------

UNIFDEF_FILES := $(filter %.c %.h,$(TARGET_FILES)) $(TARGET_ASM_PREPROCESSING_FILES)

mp3_gold_cp1: preprocess_assembly
	$(call run-unifdef, -USTUDENT -UMP2 -DMP3CP1 -t)

mp3_gold_cp2_3: preprocess_assembly
	$(call run-unifdef, -USTUDENT -UMP2 -UMP3CP1 -t)

mp3_release: preprocess_assembly
	$(call run-unifdef, -DSTUDENT -UMP2 -UMP3CP1 -t)
