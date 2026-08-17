#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shipped src/main.c must be the Bite 0.2 color-field entry (KOS path)."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.c"


def main() -> int:
    text = MAIN.read_text(encoding="utf-8")
    need = [
        ("vid_set_mode", "vid_set_mode(DM_640x480, PM_RGB565)" in text),
        ("vid_clear", "vid_clear(DINK_BOOT_R, DINK_BOOT_G, DINK_BOOT_B)" in text),
        ("serial", "DINK_BOOT_MSG" in text),
        ("fs root", "dink_fs_root" in text),
        ("probe", "found dink.dat" in text and "NO DATA ROOT" in text),
        ("title", "title_present_pvr" in text or "title_load" in text),
        ("leave_title", "leave_title" in text),
        ("loading", "GAME_STATE_LOADING" in text),
        ("tiles", "tiles_draw_pvr" in text or "tiles_upload_pvr" in text),
        ("idle", "DINK_IDLE_SEQ" in text or "sprite_draw_pvr" in text),
        ("pt list", "PVR_LIST_PT_POLY" in text),
        ("walk", "player_step" in text),
        ("edraw", "edraw_load_screen" in text),
        ("kos guard", "#ifdef _arch_dreamcast" in text),
        ("include boot", '#include "boot.h"' in text),
    ]
    missing = [n for n, ok in need if not ok]
    if missing:
        print("FAIL", MAIN, missing)
        return 1
    print("OK", MAIN)
    return 0


if __name__ == "__main__":
    sys.exit(main())
