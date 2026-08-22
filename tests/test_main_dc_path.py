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
        ("talk", "talk_probe" in text),
        ("hit", "hit_tag_list" in text),
        ("script hooks", "script_on_talk" in text and "script_on_hit" in text),
        ("dinkc file", "script_enter_vision" in text or "script_on_main" in text),
        ("dinkc vm", "dinkc_vm_tick" in text),
        ("talk proc", "dinkc_vm_start_proc" in text or "script_on_talk" in text),
        ("font", "font_init" in text),
        ("saybox", "saybox_draw_pvr" in text),
                        ("choice menu", "saybox_draw_choices_pvr" in text),
                        ("choice overlay", "choice_load" in text and "choice_upload_pvr" in text),
        ("screen swap", "screen_try_cross" in text),
        ("brains", "brains_tick" in text and "brains_enter" in text),
        ("arm weapon", "give_start_fists" in text and "dinkc_cmd_weapon_use" in text),
        ("arm magic", "dinkc_cmd_magic_use" in text),
        ("sprite snap", "spr_snap" in text),
        ("edg heap", "edraw_gfx_alloc" in text),
        ("leave pvr", "tiles_draw_clear_pvr" in text),
        ("swap keep tiles", "swap atlas ok" in text),
        ("dinkc load_screen", "play_load_screen" in text and
         "play_draw_screen" in text and
         "dinkc_cmd_bind_load_screen" in text),
        ("start menu", "startmenu_present_pvr" in text and
         "give_start_fists" in text),
        ("start pause", "startpause_tick" in text and "STARTPAUSE_SAVE" in text),
        ("save load", "load_game" in text and "save_game" in text),
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
