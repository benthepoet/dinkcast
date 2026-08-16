# SPDX-License-Identifier: GPL-3.0-or-later
# Host checks always. Dreamcast ELF is Bite 0.2+ and needs KOS_BASE.

PYTHON ?= python3

.PHONY: all host check dc clean

all: host

host: check

check:
	$(PYTHON) tools/check_port_plan.py
	$(PYTHON) tools/check_agents.py

# Fails clearly until Bite 0.2 wires kos-cc.
dc:
	@if [ -z "$(KOS_BASE)" ]; then \
		echo "make dc: KOS_BASE is unset. Install dc-chain/KallistiOS (plan Bite 0.2)." >&2; \
		exit 2; \
	fi
	@echo "make dc: no src/ yet (Bite 0.2). KOS_BASE=$(KOS_BASE)" >&2
	@exit 2

clean:
	rm -rf build
