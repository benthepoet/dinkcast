# SPDX-License-Identifier: GPL-3.0-or-later
# Host checks always. Dreamcast ELF is Bite 0.2+ and needs KOS_BASE.
# Default emulator: Flycast (see DREAMCAST-PORT-PLAN.md).

-include local.mk

PYTHON ?= python3
HOSTCC ?= gcc
EMU ?= flycast
# First existing path wins once Bite 0.2+ produces artifacts.
EMU_IMAGE ?= $(firstword $(wildcard build/dinkcast.cdi dinkcast.cdi build/dinkcast.elf dinkcast.elf))

.PHONY: all host check data-check title-preview dc cdi docker-dc docker-cdi emu run clean

HOST_CFLAGS := -Wall -Wextra -Werror -Isrc

all: host

host: check tests/test_boot_const tools/test_fs_join tests/test_bmp tests/test_dink_dat_size tests/test_pad tools/bmp_info

tests/test_boot_const: tests/test_boot_const.c src/boot.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_boot_const.c
	./$@

tools/test_fs_join: tools/test_fs_join.c src/fs.c src/fs.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/test_fs_join.c src/fs.c
	./$@

tests/test_bmp: tests/test_bmp.c src/bmp.c src/le.c src/rgb565.c src/bmp.h src/le.h src/rgb565.h src/boot.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_bmp.c src/bmp.c src/le.c src/rgb565.c
	./$@

tests/test_dink_dat_size: tests/test_dink_dat_size.c src/dinkdat.c src/fs.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_dink_dat_size.c src/dinkdat.c src/fs.c
	./$@

tests/test_pad: tests/test_pad.c src/pad.c src/pad.h
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tests/test_pad.c src/pad.c
	./$@

tools/bmp_info: tools/bmp_info.c src/bmp.c src/le.c
	$(HOSTCC) $(HOST_CFLAGS) -o $@ tools/bmp_info.c src/bmp.c src/le.c

TITLE_SRCS := tools/title_preview.c src/title.c src/bmp.c src/le.c src/rgb565.c src/dinkdat.c src/fs.c

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
	$(PYTHON) tests/test_check_dink_data.py
	$(PYTHON) tests/test_main_dc_path.py

# Optional: needs DINK_DATA (env or local.mk). Not part of `make host`.
data-check:
	DINK_DATA="$(DINK_DATA)" $(PYTHON) tools/check_dink_data.py
	$(MAKE) title-preview

# Bite 0.2 ELF. Requires sourced environ.sh so KOS_BASE is set.
dc:
	@if [ -z "$(KOS_BASE)" ]; then \
		echo "make dc: KOS_BASE is unset. Install dc-chain + KallistiOS, then:" >&2; \
		echo "  source \$$KOS_BASE/environ.sh && make dc" >&2; \
		exit 2; \
	fi
	$(MAKE) -f Makefile.dc

# Selfboot CDI: ELF + DINK_DATA as /cd/dink. See docs/TOOLCHAIN.md.
cdi:
	DINK_DATA="$(DINK_DATA)" sh tools/make_cdi.sh build/dinkcast.elf build/dinkcast.cdi

# KallistiOS via Docker (see docs/TOOLCHAIN.md). Needs a running daemon.
docker-dc:
	DINK_DATA="$(DINK_DATA)" sh tools/docker_kos.sh make dc

docker-cdi:
	DINK_DATA="$(DINK_DATA)" sh tools/docker_kos.sh 'make dc && make -e cdi'

# Launch Flycast (or EMU=...) on the built CDI/ELF. Does not build the ELF.
emu run:
	@$(PYTHON) tools/run_emu.py --emu "$(EMU)" --image "$(EMU_IMAGE)"

clean:
	rm -rf build tests/test_boot_const tools/test_fs_join tests/test_bmp \
		tests/test_dink_dat_size tests/test_pad tools/bmp_info tools/title_preview
	@if [ -n "$(KOS_BASE)" ]; then $(MAKE) -f Makefile.dc clean; fi
