#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Drive tools/check_dink_data.py on missing and tarball-root-shaped trees."""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "check_dink_data.py"


def run(env: dict[str, str]) -> subprocess.CompletedProcess[str]:
    e = os.environ.copy()
    e.update(env)
    return subprocess.run(
        [sys.executable, str(SCRIPT)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        env=e,
    )


def main() -> int:
    unset = run({"DINK_DATA": ""})
    if unset.returncode != 2 or "unset" not in unset.stderr:
        print("FAIL unset", unset.returncode, unset.stderr)
        return 1
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        (root / "README.txt").write_text("nope")
        inner = root / "dink"
        inner.mkdir()
        (inner / "Dink.dat").write_bytes(b"x")
        bad = run({"DINK_DATA": str(root)})
        if bad.returncode != 1 or "inner tree" not in bad.stderr:
            print("FAIL tarball root", bad.returncode, bad.stderr)
            return 1
    print("OK", SCRIPT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
