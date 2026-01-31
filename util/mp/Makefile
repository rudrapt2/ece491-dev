# Utility to generate mp2 gold and mp2 release

# =========================
# User-configurable variables
# =========================

SRC_DIR     := /home/rudrapt2/sp26_ece391/glycine-max
TARGET_DIR  := /home/rudrapt2/sp26_ece391/mp2-sp26/release

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
    sys/misc.c \
    sys/misc.h \
	sys/mp2.make:sys/Makefile \
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
	unifdef $(1) -m $(UNIFDEF_FILES) || [ $$? -eq 1 ]
endef

# =========================
# Phony targets
# =========================

.PHONY: mp2_gold mp2_release copy_files preprocess_assembly check_clean

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

mp2_gold: preprocess_assembly
	$(call run-unifdef,-DMP2 -t)

mp2_release: preprocess_assembly
	$(call run-unifdef,-DMP2 -DSTUDENT -t)