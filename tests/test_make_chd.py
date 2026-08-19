#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""mkdcdisc data LBA + GDI wrapper; chdman if present."""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "make_chd.sh"

# mkdcdisc: 4500 lead-in + 6750 first lead-out + 150 pregap + 302 audio.
MKDCDISC_DATA_LBA = 4500 + 6750 + 150 + 302


def main() -> int:
    if MKDCDISC_DATA_LBA != 11702:
        print("FAIL data LBA", MKDCDISC_DATA_LBA)
        return 1
    if not SCRIPT.is_file():
        print("FAIL missing", SCRIPT)
        return 1
    with tempfile.TemporaryDirectory() as td:
        tdir = Path(td)
        iso = tdir / "dinkcast.iso"
        iso.write_bytes(b"\0" * 2048 * 8)
        chd = tdir / "dinkcast.chd"
        env = os.environ.copy()
        r = subprocess.run(
            ["sh", str(SCRIPT), str(iso), str(chd)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            env=env,
        )
        if shutil.which(env.get("CHDMAN", "chdman")) is None:
            if r.returncode != 2 or "chdman not found" not in r.stderr:
                print("FAIL no-chdman rc", r.returncode, r.stderr)
                return 1
            print("OK", SCRIPT, "(chdman not installed; layout constant checked)")
            return 0
        if r.returncode != 0:
            print("FAIL make_chd", r.returncode, r.stdout, r.stderr)
            return 1
        gdi = tdir / "dinkcast.gdi"
        text = gdi.read_text(encoding="utf-8")
        if f"2 {MKDCDISC_DATA_LBA} 4 2048" not in text:
            print("FAIL gdi", text)
            return 1
        if "1 0 0 2352" not in text:
            print("FAIL audio track", text)
            return 1
        if not chd.is_file() or chd.stat().st_size < 64:
            print("FAIL chd missing")
            return 1
        if "0 tracks" in r.stdout.lower() or "Input tracks: 0" in r.stdout:
            print("FAIL zero tracks", r.stdout)
            return 1
        print("gdi", text.replace("\n", " | "))
        print("OK", SCRIPT)
        return 0


if __name__ == "__main__":
    sys.exit(main())
