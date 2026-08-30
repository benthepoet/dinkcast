#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""14.4c: EdGfx victims are Always/Screen/Sticky, not seq-id ranges."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
EDRAW = ROOT / "src" / "edraw.c"


def main() -> int:
    text = EDRAW.read_text(encoding="utf-8")
    bad = []
    if "s >= 110 && s <= 129" in text or "s >= 110 &&" in text:
        bad.append("duck death seq-id range")
    if "s >= 200" in text:
        bad.append("seq>=200 victim pick")
    if "class=%s" not in text and 'class=screen' not in text:
        if "pixel_class" not in text:
            bad.append("no pixel_class")
    if "PIX_ALWAYS" not in text or "PIX_STICKY" not in text:
        bad.append("missing Always/Sticky pixel classes")
    if "edraw evict class=" not in text:
        bad.append("evict log missing class=")
    if "current+next" not in text and "edraw_loop_next_frame" not in text:
        bad.append("loop current+next trim")
    if "edraw_warm_held" not in text or "residency_is_held" not in text:
        bad.append("held treefire warmup")
    if bad:
        print("FAIL", EDRAW, ":", "; ".join(bad))
        return 1
    print("OK", EDRAW, "14.4c pixel classes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
