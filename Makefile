# SPDX-License-Identifier: GPL-3.0-or-later
# Host checks always. Dreamcast ELF is Bite 0.2+ and needs KOS_BASE.
# Default emulator: Flycast (see DREAMCAST-PORT-PLAN.md).

-include local.mk

PYTHON ?= python3
HOSTCC ?= gcc
EMU ?= flycast
# First existing path wins once Bite 0.2+ produces artifacts.
EMU_IMAGE ?= $(firstword $(wildcard build/dinkcast.cdi dinkcast.cdi build/dinkcast.elf dinkcast.elf))

.PHONY: all host check data-check dc emu run clean

all: host

host: check tests/test_boot_const tools/test_fs_join

tests/test_boot_const: tests/test_boot_const.c src/boot.h
	$(HOSTCC) -Wall -Wextra -Werror -Isrc -o $@ tests/test_boot_const.c
	./$@

tools/test_fs_join: tools/test_fs_join.c src/fs.c src/fs.h
	$(HOSTCC) -Wall -Wextra -Werror -Isrc -o $@ tools/test_fs_join.c src/fs.c
	./$@

check:
	$(PYTHON) tools/check_port_plan.py
	$(PYTHON) tools/check_agents.py
	$(PYTHON) tests/test_run_emu.py
	$(PYTHON) tests/test_check_dink_data.py
	$(PYTHON) tests/test_main_dc_path.py

# Optional: needs DINK_DATA (env or local.mk). Not part of `make host`.
data-check:
	DINK_DATA="$(DINK_DATA)" $(PYTHON) tools/check_dink_data.py

# Bite 0.2 ELF. Requires sourced environ.sh so KOS_BASE is set.
dc:
	@if [ -z "$(KOS_BASE)" ]; then \
		echo "make dc: KOS_BASE is unset. Install dc-chain + KallistiOS, then:" >&2; \
		echo "  source \$$KOS_BASE/environ.sh && make dc" >&2; \
		exit 2; \
	fi
	$(MAKE) -f Makefile.dc

# Launch Flycast (or EMU=...) on the built CDI/ELF. Does not build the ELF.
emu run:
	@$(PYTHON) tools/run_emu.py --emu "$(EMU)" --image "$(EMU_IMAGE)"

clean:
	rm -rf build tests/test_boot_const tools/test_fs_join
	@if [ -n "$(KOS_BASE)" ]; then $(MAKE) -f Makefile.dc clean; fi
