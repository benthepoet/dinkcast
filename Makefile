# SPDX-License-Identifier: GPL-3.0-or-later
# Host checks always. Dreamcast ELF is Bite 0.2+ and needs KOS_BASE.
# Default emulator: Flycast (see DREAMCAST-PORT-PLAN.md).

PYTHON ?= python3
EMU ?= flycast
# First existing path wins once Bite 0.2+ produces artifacts.
EMU_IMAGE ?= $(firstword $(wildcard build/dinkcast.cdi dinkcast.cdi build/dinkcast.elf dinkcast.elf))

.PHONY: all host check dc emu run clean

all: host

host: check

check:
	$(PYTHON) tools/check_port_plan.py
	$(PYTHON) tools/check_agents.py
	$(PYTHON) tests/test_run_emu.py

# Fails clearly until Bite 0.2 wires kos-cc.
dc:
	@if [ -z "$(KOS_BASE)" ]; then \
		echo "make dc: KOS_BASE is unset. Install dc-chain/KallistiOS (plan Bite 0.2)." >&2; \
		exit 2; \
	fi
	@echo "make dc: no src/ yet (Bite 0.2). KOS_BASE=$(KOS_BASE)" >&2
	@exit 2

# Launch Flycast (or EMU=...) on the built CDI/ELF. Does not build the ELF.
emu run:
	@$(PYTHON) tools/run_emu.py --emu "$(EMU)" --image "$(EMU_IMAGE)"

clean:
	rm -rf build
