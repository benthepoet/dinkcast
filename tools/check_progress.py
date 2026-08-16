#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""PROGRESS.md must exist and record landed bites."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
PROG = ROOT / "PROGRESS.md"


def main() -> int:
    if not PROG.is_file():
        print("FAIL missing", PROG)
        return 1
    low = PROG.read_text(encoding="utf-8").lower()
    missing = []
    for s in ("0.1", "0.2", "1.1", "1.2", "3.1", "3.4", "splash", "#1", "#3", "next"):
        if s.lower() not in low:
            missing.append(s)
    if "done" not in low:
        missing.append("done status")
    if "feasibility" not in low or "overall" not in low:
        missing.append("feasibility snapshot")
    if missing:
        print("FAIL PROGRESS.md missing:", ", ".join(missing))
        return 1
    print("OK", PROG)
    return 0


if __name__ == "__main__":
    sys.exit(main())
