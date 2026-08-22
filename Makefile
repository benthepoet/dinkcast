# SPDX-License-Identifier: GPL-3.0-or-later
# Host checks always. Dreamcast ELF is Bite 0.2+ and needs KOS_BASE.
# Default emulator: Flycast (see DREAMCAST-PORT-PLAN.md).

-include local.mk

PYTHON ?= python3
HOSTCC ?= gcc
EMU ?= flycast
# First existing path wins once Bite 0.2+ produces artifacts.
EMU_IMAGE ?= $(firstword $(wildcard build/dinkcast.chd dinkcast.chd build/dinkcast.cdi dinkcast.cdi build/dinkcast.elf dinkcast.elf))
# Latest playtest SCIF/stdout (also printed). Override with EMU_LOG=.
EMU_LOG ?= build/emu.log

.PHONY: all host check data-check title-preview dc cdi chd docker-dc docker-cdi emu run clean

HOST_CFLAGS := -Wall -Wextra -Werror -Isrc

all: host

host: check tests/test_boot_const tools/test_fs_join tests/test_bmp tests/test_dink_dat_size tests/test_pad tests/test_world tests/test_tile_cell tests/test_ini tests/test_ff tests/test_io_once tests/test_sprite tests/test_player tests/test_edraw tests/test_talk tests/test_hit tests/test_script tests/test_dinkc_file tests/test_dinkc_lex tests/test_dinkc_parse tests/test_dinkc_vm tests/test_dinkc_var tests/test_font tests/test_saybox tests/test_choice tests/test_screen tests/test_brains tests/test_hurt tests/test_playtest tests/test_weapon tests/test_touch tests/test_inv tests/test_status tests/test_mem tests/test_leak tests/test_save tests/test_startmenu tools/bmp_info tools/dump_world tools/map_recsize tools/dump_screen tools/dump_ini

tests/test_boot_const: tests/test_boot_const.c src/boot.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_boot_const.c
	./$@

tools/test_fs_join: tools/test_fs_join.c src/fs.c src/residency.c src/ff.c src/le.c src/fs.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/test_fs_join.c src/fs.c src/residency.c src/ff.c src/le.c
	./$@

tests/test_bmp: tests/test_bmp.c src/bmp.c src/le.c src/rgb565.c src/bmp.h src/le.h src/rgb565.h src/boot.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_bmp.c src/bmp.c src/le.c src/rgb565.c
	./$@

tests/test_dink_dat_size: tests/test_dink_dat_size.c src/dinkdat.c src/fs.c src/residency.c src/ff.c src/le.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_dink_dat_size.c src/dinkdat.c src/fs.c src/residency.c src/ff.c src/le.c
	./$@

tests/test_pad: tests/test_pad.c src/pad.c src/pad.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_pad.c src/pad.c
	./$@

tests/test_player: tests/test_player.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c src/mapscr.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_player.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c src/mapscr.c
	./$@

tests/test_edraw: tests/test_edraw.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_edraw.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	DINK_DATA="$(DINK_DATA)" ./$@
	DINK_DATA="$(DINK_DATA)" $(PYTHON) tests/test_distill.py

tests/test_talk: tests/test_talk.c src/talk.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/player.c src/hurt.c src/hard.c src/pad.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_talk.c src/talk.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/player.c src/hurt.c src/hard.c src/pad.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	./$@

tests/test_hit: tests/test_hit.c src/hit.c src/talk.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/player.c src/hurt.c src/brains.c src/hard.c src/pad.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_hit.c src/hit.c src/talk.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/player.c src/hurt.c src/brains.c src/hard.c src/pad.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	./$@

tests/test_script: tests/test_script.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_script.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_dinkc_file: tests/test_dinkc_file.c src/dinkc_file.c src/fs.c src/residency.c src/ff.c src/le.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_dinkc_file.c src/dinkc_file.c src/fs.c src/residency.c src/ff.c src/le.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_dinkc_lex: tests/test_dinkc_lex.c src/dinkc_lex.c src/dinkc_file.c src/fs.c src/residency.c src/ff.c src/le.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_dinkc_lex.c src/dinkc_lex.c src/dinkc_file.c src/fs.c src/residency.c src/ff.c src/le.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_dinkc_parse: tests/test_dinkc_parse.c src/dinkc_parse.c src/dinkc_lex.c src/dinkc_file.c src/fs.c src/residency.c src/ff.c src/le.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_dinkc_parse.c src/dinkc_parse.c src/dinkc_lex.c src/dinkc_file.c src/fs.c src/residency.c src/ff.c src/le.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_dinkc_vm: tests/test_dinkc_vm.c src/dinkc_vm.c src/dinkc_lex.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/ini.c src/ff.c src/mapscr.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_dinkc_vm.c src/dinkc_vm.c src/dinkc_lex.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/ini.c src/ff.c src/mapscr.c src/le.c src/fs.c src/residency.c
	./$@

tests/test_dinkc_var: tests/test_dinkc_var.c src/dinkc_var.c src/dinkc_vm.c src/dinkc_lex.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/ini.c src/ff.c src/mapscr.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_dinkc_var.c src/dinkc_var.c src/dinkc_vm.c src/dinkc_lex.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/ini.c src/ff.c src/mapscr.c src/le.c src/fs.c src/residency.c
	./$@

tests/test_font: tests/test_font.c src/font.c src/font.h src/font_glyphs.inc
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_font.c src/font.c
	./$@

tests/test_saybox: tests/test_saybox.c src/saybox.c src/font.c src/mapscr.c src/le.c src/fs.c src/residency.c src/ff.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_saybox.c src/saybox.c src/font.c src/mapscr.c src/le.c src/fs.c src/residency.c src/ff.c
	./$@

tests/test_choice: tests/test_choice.c src/choice.c src/font.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_choice.c src/choice.c src/font.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/le.c src/fs.c src/residency.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_screen: tests/test_screen.c src/screen.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/ff.c src/hard.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_screen.c src/screen.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/ff.c src/hard.c
	./$@

tests/test_brains: tests/test_brains.c src/brains.c src/hurt.c src/player.c src/hard.c src/mapscr.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c src/world.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_brains.c src/brains.c src/hurt.c src/player.c src/hard.c src/mapscr.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c src/world.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_hurt: tests/test_hurt.c src/hurt.c src/hit.c src/brains.c src/player.c src/hard.c src/mapscr.c src/ini.c src/ff.c src/edraw.c src/sprite.c src/bmp.c src/le.c src/fs.c src/residency.c src/dinkc_cmd.c src/save.c src/dinkc_var.c src/dinkc_vm.c src/dinkc_lex.c src/saybox.c src/font.c src/mem.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_hurt.c src/hurt.c src/hit.c src/brains.c src/player.c src/hard.c src/mapscr.c src/ini.c src/ff.c src/edraw.c src/sprite.c src/bmp.c src/le.c src/fs.c src/residency.c src/dinkc_cmd.c src/save.c src/dinkc_var.c src/dinkc_vm.c src/dinkc_lex.c src/saybox.c src/font.c src/mem.c
	./$@

tests/test_playtest: tests/test_playtest.c src/hurt.c src/hit.c src/brains.c src/player.c src/hard.c src/mapscr.c src/ini.c src/ff.c src/edraw.c src/sprite.c src/bmp.c src/le.c src/fs.c src/residency.c src/mem.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_playtest.c src/hurt.c src/hit.c src/brains.c src/player.c src/hard.c src/mapscr.c src/ini.c src/ff.c src/edraw.c src/sprite.c src/bmp.c src/le.c src/fs.c src/residency.c src/mem.c
	./$@

tests/test_weapon: tests/test_weapon.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_weapon.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_touch: tests/test_touch.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c src/hit.c src/edraw.c src/sprite.c src/bmp.c src/mem.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_touch.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c src/hit.c src/edraw.c src/sprite.c src/bmp.c src/mem.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_inv: tests/test_inv.c src/inv.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c src/sprite.c src/bmp.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_inv.c src/inv.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c src/sprite.c src/bmp.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_status: tests/test_status.c src/status.c src/inv.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c src/sprite.c src/bmp.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_status.c src/status.c src/inv.c src/script.c src/dinkc_file.c src/dinkc_lex.c src/dinkc_parse.c src/dinkc_vm.c src/dinkc_var.c src/dinkc_cmd.c src/save.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/pad.c src/ini.c src/ff.c src/mapscr.c src/world.c src/le.c src/fs.c src/residency.c src/brains.c src/sprite.c src/bmp.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_mem: tests/test_mem.c src/mem.c src/fs.c src/residency.c src/ff.c src/le.c src/mem.h src/fs.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_mem.c src/mem.c src/fs.c src/residency.c src/ff.c src/le.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_leak: tests/test_leak.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c src/tiles.c src/rgb565.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_leak.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c src/tiles.c src/rgb565.c
	./$@

tests/test_save: tests/test_save.c src/save.c src/dinkc_cmd.c src/dinkc_var.c src/dinkc_vm.c src/dinkc_lex.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/ini.c src/ff.c src/mapscr.c src/le.c src/fs.c src/residency.c src/pad.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_save.c src/save.c src/dinkc_cmd.c src/dinkc_var.c src/dinkc_vm.c src/dinkc_lex.c src/saybox.c src/font.c src/player.c src/hurt.c src/hard.c src/ini.c src/ff.c src/mapscr.c src/le.c src/fs.c src/residency.c src/pad.c
	./$@

tests/test_startmenu: tests/test_startmenu.c src/startmenu.c src/pad.c src/startmenu.h src/pad.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_startmenu.c src/startmenu.c src/pad.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_world: tests/test_world.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/ff.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_world.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/ff.c
	./$@

tests/test_tile_cell: tests/test_tile_cell.c src/tiles.c src/mapscr.c src/bmp.c src/le.c src/rgb565.c src/fs.c src/residency.c src/ff.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_tile_cell.c src/tiles.c src/mapscr.c src/bmp.c src/le.c src/rgb565.c src/fs.c src/residency.c src/ff.c
	DINK_DATA="$(DINK_DATA)" ./$@

DUMP_COMMON := src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/ff.c src/dinkdat.c

tools/dump_world: tools/dump_world.c $(DUMP_COMMON)
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/dump_world.c $(DUMP_COMMON)

tools/map_recsize: tools/map_recsize.c $(DUMP_COMMON)
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/map_recsize.c $(DUMP_COMMON)

tools/dump_screen: tools/dump_screen.c $(DUMP_COMMON)
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/dump_screen.c $(DUMP_COMMON)

tests/test_ini: tests/test_ini.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_ini.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c
	./$@

tests/test_ff: tests/test_ff.c src/ff.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_ff.c src/ff.c src/le.c src/fs.c src/residency.c
	DINK_DATA="$(DINK_DATA)" ./$@

tests/test_io_once: tests/test_io_once.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/tiles.c src/rgb565.c src/hard.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_io_once.c src/edraw.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/tiles.c src/rgb565.c src/hard.c src/world.c src/mapscr.c src/le.c src/fs.c src/residency.c src/mem.c
	DINK_DATA="$(DINK_DATA)" ./$@

tools/dump_ini: tools/dump_ini.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/dump_ini.c src/ini.c src/ff.c src/le.c src/fs.c src/residency.c

tests/test_sprite: tests/test_sprite.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/le.c src/fs.c src/residency.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_sprite.c src/sprite.c src/ini.c src/ff.c src/bmp.c src/le.c src/fs.c src/residency.c
	DINK_DATA="$(DINK_DATA)" ./$@

tools/bmp_info: tools/bmp_info.c src/bmp.c src/le.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/bmp_info.c src/bmp.c src/le.c

TITLE_SRCS := tools/title_preview.c src/title.c src/bmp.c src/le.c src/rgb565.c src/dinkdat.c src/fs.c src/residency.c src/ff.c

title-preview: tools/title_preview
	mkdir -p build
	DINK_DATA="$(DINK_DATA)" ./tools/title_preview build/title_preview.ppm

tools/title_preview: $(TITLE_SRCS)
	$(HOSTCC) $(HOST_CFLAGS) -o $@ $(TITLE_SRCS)

check:
	$(PYTHON) tools/check_port_plan.py
	$(PYTHON) tools/check_agents.py
	$(PYTHON) tools/check_progress.py
	$(PYTHON) tests/test_run_emu.py
	$(PYTHON) tests/test_make_chd.py
	$(PYTHON) tests/test_stage_dink.py
	$(PYTHON) tests/test_check_dink_data.py
	$(PYTHON) tests/test_main_dc_path.py
	DINK_DATA="$(DINK_DATA)" $(PYTHON) tests/test_pack_catalog.py
	DINK_DATA="$(DINK_DATA)" $(PYTHON) tests/test_distill.py
	$(PYTHON) tests/test_edraw_policy.py
	$(PYTHON) tests/test_leak_policy.py

# Optional: needs DINK_DATA (env or local.mk). Not part of `make host`.
data-check:
	DINK_DATA="$(DINK_DATA)" $(PYTHON) tools/check_dink_data.py
	$(MAKE) title-preview
	DINK_DATA="$(DINK_DATA)" ./tools/dump_world
	DINK_DATA="$(DINK_DATA)" ./tools/map_recsize
	DINK_DATA="$(DINK_DATA)" ./tools/dump_screen

# Bite 0.2 ELF. Requires sourced environ.sh so KOS_BASE is set.
dc:
	@if [ -z "$(KOS_BASE)" ]; then \
		echo "make dc: KOS_BASE is unset. Install dc-chain + KallistiOS, then:" >&2; \
		echo "  source \$$KOS_BASE/environ.sh && make dc" >&2; \
		exit 2; \
	fi
	$(MAKE) -f Makefile.dc

# Selfboot CDI + data-track ISO: ELF + DINK_DATA as /cd/dink. See docs/TOOLCHAIN.md.
cdi:
	DINK_DATA="$(DINK_DATA)" sh tools/make_cdi.sh build/dinkcast.elf build/dinkcast.cdi

# Flycast image: CUE (mkdcdisc ISO + dummy audio) compressed to a MIL-CD CHD. Needs chdman (mame-tools).
chd:
	sh tools/make_chd.sh build/dinkcast.iso build/dinkcast.chd

# KallistiOS via Docker (see docs/TOOLCHAIN.md). Needs a running daemon.
docker-dc:
	DINK_DATA="$(DINK_DATA)" sh tools/docker_kos.sh make dc

docker-cdi:
	DINK_DATA="$(DINK_DATA)" $(PYTHON) tools/distill_frames.py --out build/distill
	DINK_DATA="$(DINK_DATA)" sh tools/docker_kos.sh 'make dc && make -e cdi'
	$(MAKE) chd

# Launch Flycast (or EMU=...) on the built CHD (preferred) / CDI / ELF.
emu run:
	@$(PYTHON) tools/run_emu.py --emu "$(EMU)" --image "$(EMU_IMAGE)" --log "$(EMU_LOG)"

clean:
	rm -rf build tests/test_boot_const tools/test_fs_join tests/test_bmp \
		tests/test_dink_dat_size tests/test_pad tests/test_world tests/test_tile_cell \
		tests/test_ini tests/test_ff tests/test_io_once tests/test_sprite tests/test_player \
		tests/test_edraw tests/test_talk tests/test_hit tests/test_script \
		tests/test_dinkc_file tests/test_dinkc_lex tests/test_dinkc_parse \
		tests/test_dinkc_vm tests/test_dinkc_var tests/test_font \
		tests/test_saybox tests/test_choice tests/test_screen tests/test_brains \
		tests/test_hurt tests/test_playtest tests/test_weapon tests/test_touch tests/test_inv tests/test_status tests/test_mem tests/test_leak \
		tests/test_save tests/test_startmenu \
		tools/bmp_info tools/title_preview tools/dump_world tools/map_recsize \
		tools/dump_screen tools/dump_ini
	@if [ -n "$(KOS_BASE)" ]; then $(MAKE) -f Makefile.dc clean; fi
