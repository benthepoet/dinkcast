#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""14.3: swap_ms + leak-20 mem_log, not a new eviction dialect."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.c"
MEM = ROOT / "src" / "mem.c"
MEMH = ROOT / "src" / "mem.h"


def main() -> int:
    main_c = MAIN.read_text(encoding="utf-8")
    mem_c = MEM.read_text(encoding="utf-8")
    mem_h = MEMH.read_text(encoding="utf-8")
    bad = []
    if "swap_ms" not in main_c:
        bad.append("main.c missing swap_ms")
    if "mem_now_ms" not in main_c:
        bad.append("main.c missing mem_now_ms")
    if "vram_free" not in mem_c:
        bad.append("mem.c missing vram_free")
    if "leak 20" not in mem_c:
        bad.append("mem.c missing leak 20")
    if "DINK_MEM_LEAK_DELTA" not in mem_h:
        bad.append("mem.h missing DINK_MEM_LEAK_DELTA")
    if "4096" not in mem_h:
        bad.append("mem.h missing 4 KB cap")
    if bad:
        print("FAIL 14.3 policy:", "; ".join(bad))
        return 1
    print("OK", MAIN, MEM, "14.3 leak log")
    return 0


if __name__ == "__main__":
    sys.exit(main())
