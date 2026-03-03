################################################################
#
# $Id:$
#
# $Log:$
#

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

ULIB_OBJS = \
	../usr/build/io.o \
	../usr/build/heap.o \
	../usr/build/start.o \
	../usr/build/error.o \
	../usr/build/string.o \
	../usr/build/syscall.o

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. -I.. -I../usr -DUMODE

ULIB_LD = ../usr/umode.ld

# subdirectory for objects
OBJDIR=build
OUTPUT=doomgeneric

SRC_DOOM = \
 info.o \
 sha1.o \
 dummy.o \
 d_net.o \
 memio.o \
 p_map.o \
 r_bsp.o \
 r_sky.o \
 w_wad.o \
 am_map.o \
 d_iwad.o \
 d_loop.o \
 d_main.o \
 d_mode.o \
 f_wipe.o \
 g_game.o \
 hu_lib.o \
 m_argv.o \
 m_bbox.o \
 m_menu.o \
 m_misc.o \
 p_mobj.o \
 p_pspr.o \
 p_spec.o \
 p_tick.o \
 p_user.o \
 r_data.o \
 r_draw.o \
 r_main.o \
 r_segs.o \
 sounds.o \
 st_lib.o \
 tables.o \
 w_file.o \
 w_main.o \
 z_zone.o \
 doomdef.o \
 d_event.o \
 d_items.o \
 i_cdmus.o \
 i_scale.o \
 i_sound.o \
 i_timer.o \
 m_cheat.o \
 m_fixed.o \
 p_doors.o \
 p_enemy.o \
 p_floor.o \
 p_inter.o \
 p_plats.o \
 p_saveg.o \
 p_setup.o \
 p_sight.o \
 r_plane.o \
 s_sound.o \
 v_video.o \
 i_input.o \
 i_video.o \
 doomstat.o \
 dstrings.o \
 f_finale.o \
 hu_stuff.o \
 i_endoom.o \
 i_system.o \
 m_config.o \
 m_random.o \
 p_ceilng.o \
 p_lights.o \
 p_maputl.o \
 p_switch.o \
 p_telept.o \
 r_things.o \
 statdump.o \
 st_stuff.o \
 wi_stuff.o \
 i_joystick.o \
 m_controls.o \
 w_checksum.o \
 w_file_stdc.o \
 doomgeneric.o \
 doomgeneric_illinix.o

OBJS += $(addprefix $(OBJDIR)/, $(SRC_DOOM)) \
		../glue.o

all:	 $(OUTPUT)
	mv doomgeneric ../bin/doom

clean:
	rm -rf $(OBJDIR)
	rm -f $(OUTPUT)
	rm -f $(OUTPUT).gdb
	rm -f $(OUTPUT).map

$(OUTPUT):	$(OBJS)
	@echo [Linking $@]
	$(VB)$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) \
	-T $(ULIB_LD) -o $(OUTPUT) $(ULIB_OBJS)

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c
	@echo [Compiling $<]
	$(VB)$(CC) $(CFLAGS) -c $< -o $@

print:
	@echo OBJS: $(OBJS)

