#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""14.4a catalog must print named screens. Over-cap is a print, not a fail."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "pack_catalog.py"


def main() -> int:
    env = os.environ.copy()
    if not env.get("DINK_DATA", "").strip():
        print("SKIP test_pack_catalog: DINK_DATA unset")
        return 0
    r = subprocess.run(
        [sys.executable, str(SCRIPT)],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    out = r.stdout + r.stderr
    print(r.stdout, end="")
    if r.returncode != 0:
        print("FAIL pack_catalog exit", r.returncode, r.stderr)
        return 1
    need = (
        "hard.dat FILE* not blob",
        "house vis 0",
        "outdoor 439",
        "duck 441 vis 2",
        "408 girl",
        "pig 407",
        "castle pack",
        "OK pack_catalog",
    )
    missing = [s for s in need if s not in out]
    if missing:
        print("FAIL missing", missing)
        return 1
    if "14.5: needed" in out:
        print("note: catalog recorded 14.5: needed (not a fail)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
