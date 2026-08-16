#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify DINK_DATA is the game tree (dink.dat etc.), not the tarball root."""
from __future__ import annotations

import os
import sys
from pathlib import Path

NEEDED = ("dink.dat", "map.dat", "hard.dat", "dink.ini", "story", "tiles", "graphics")


def find_ci(root: Path, name: str) -> Path | None:
    want = name.lower()
    try:
        for p in root.iterdir():
            if p.name.lower() == want:
                return p
    except FileNotFoundError:
        return None
    return None


def main() -> int:
    raw = os.environ.get("DINK_DATA", "").strip()
    if not raw:
        print(
            "DINK_DATA is unset. Copy local.mk.example to local.mk "
            "or export DINK_DATA to the directory that contains Dink.dat.",
            file=sys.stderr,
        )
        return 2
    root = Path(raw).expanduser()
    if not root.is_dir():
        print(f"DINK_DATA is not a directory: {root}", file=sys.stderr)
        return 2
    missing = [n for n in NEEDED if find_ci(root, n) is None]
    if missing:
        inner = find_ci(root, "dink")
        hint = ""
        if inner and find_ci(inner, "dink.dat"):
            hint = f"\nUse the inner tree instead: DINK_DATA={inner}"
        print(
            f"DINK_DATA={root} is missing: {', '.join(missing)}.{hint}",
            file=sys.stderr,
        )
        return 1
    print("OK DINK_DATA", root)
    for n in NEEDED:
        p = find_ci(root, n)
        print(f"  {n} -> {p.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
