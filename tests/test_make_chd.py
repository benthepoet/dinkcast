#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""mkdcdisc 302-sector audio + CUE MIL-CD; chdman if present (not CHGD)."""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "make_chd.sh"

# Flycast MIL-CD: 150 lead-in + 302 audio + 11400 session gap = 11852 FAD.
# 11852 - 150 = LBA 11702 = mkdcdisc ms_block.
MKDCDISC_AUDIO_SECTORS = 302
MKDCDISC_DATA_LBA = 4500 + 6750 + 150 + MKDCDISC_AUDIO_SECTORS
FLYCAST_MILCD_FAD = 150 + MKDCDISC_AUDIO_SECTORS + 11400


def main() -> int:
    if MKDCDISC_DATA_LBA != 11702 or FLYCAST_MILCD_FAD != 11852:
        print("FAIL LBA/FAD", MKDCDISC_DATA_LBA, FLYCAST_MILCD_FAD)
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
        cue = tdir / "dinkcast.cue"
        text = cue.read_text(encoding="utf-8")
        if "TRACK 01 AUDIO" not in text or "TRACK 02 MODE1/2048" not in text:
            print("FAIL cue", text)
            return 1
        audio = tdir / "dinkcast-track01.bin"
        if audio.stat().st_size != MKDCDISC_AUDIO_SECTORS * 2352:
            print("FAIL audio size", audio.stat().st_size)
            return 1
        if not chd.is_file() or chd.stat().st_size < 64:
            print("FAIL chd missing")
            return 1
        if "CHGD" in r.stdout:
            print("FAIL GD-ROM metadata", r.stdout)
            return 1
        if "0 tracks" in r.stdout.lower() or "Input tracks: 0" in r.stdout:
            print("FAIL zero tracks", r.stdout)
            return 1
        print("cue", text.replace("\n", " | "))
        print("OK", SCRIPT)
        return 0


if __name__ == "__main__":
    sys.exit(main())
