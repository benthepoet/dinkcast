#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Drive tools/run_emu.py: no image and missing file must fail clearly."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "run_emu.py"


def run(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def main() -> int:
    empty = run(["--emu", "flycast", "--image", ""])
    if empty.returncode != 2 or "no image yet" not in empty.stderr:
        print("FAIL empty image:", empty.returncode, empty.stderr)
        return 1
    missing = run(["--emu", "flycast", "--image", str(ROOT / "no-such.cdi")])
    if missing.returncode != 2 or "not found" not in missing.stderr:
        print("FAIL missing image:", missing.returncode, missing.stderr)
        return 1
    print("OK", SCRIPT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
