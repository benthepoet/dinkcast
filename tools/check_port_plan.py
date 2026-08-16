#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Structural check of DREAMCAST-PORT-PLAN.md (the shipped planning artifact)."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "DREAMCAST-PORT-PLAN.md"


def main() -> int:
    text = PLAN.read_text(encoding="utf-8")
    missing = []

    def need(label: str, pred: bool) -> None:
        if not pred:
            missing.append(label)

    low = text.lower()
    need("names Dink Smallwood", "dink smallwood" in low)
    need("names Sega Dreamcast", "sega dreamcast" in low)
    need("title screen as first visual", "first visual milestone" in low and "title" in low)
    need("640x480 or 320x240", "640×480" in text or "640x480" in low)
    need("16 MB main RAM", "16 mb" in low)
    need("8 MB VRAM", "8 mb" in low and "vram" in low)
    need("2 MB AICA", "2 mb" in low and "aica" in low)
    need("KallistiOS", "kallistios" in low)
    need("dink.dat", "dink.dat" in low)
    need("map.dat", "map.dat" in low)
    need("walk", "walk" in low)
    need("talk", "talk" in low)
    need("hit", "hit" in low)
    need("DinkC", "dinkc" in low)
    need("weapons", "weapon" in low)
    need("magic", "magic" in low)
    need("inventory", "inventory" in low)
    need("FreeDink or freeware data", "freedink" in low)
    need("original data required", "original game data" in low and "required" in low)
    need("numbered bites", "bite 3.4" in low)
    need("PowerVR2 / textured quad", "textured quad" in low or "powervr" in low)
    need("dinkc waves", "wave 1 commands" in low)
    need("hard.dat", "hard.dat" in low)
    need("yield vm", "wait(ms)" in low or "struct script" in low)
    need(
        "FreeDink interpreter good enough (no custom perf VM)",
        "good enough on sh-4" in low and "not" in low and "tuned" in low,
    )
    need("60 FPS target with 30 floor", "60 fps" in low and "30 fps is the floor" in low)

    i_title = low.find("first visual milestone")
    i_walk = low.find("bite 9.2")
    need("title bite before walk bite", 0 <= i_title < i_walk)

    bite_heads = [
        line
        for line in text.splitlines()
        if line.lower().lstrip("# ").startswith("bite ")
    ]
    need("at least 40 granular bites", len(bite_heads) >= 40)

    if missing:
        print("FAIL:", "; ".join(missing))
        return 1
    print("OK", PLAN)
    print("bites:", len(bite_heads))
    return 0


if __name__ == "__main__":
    sys.exit(main())
