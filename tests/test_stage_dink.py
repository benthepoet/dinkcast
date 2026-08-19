#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""stage_dink.sh copies DINK_DATA and sector-pads every file (KOS #1492)."""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "tools" / "stage_dink.sh"
PAD = ROOT / "tools" / "pad2048.sh"

FIXTURE = {
    "graphics/struct/home/dir.ff": (b"FFPK" + bytes(range(256))) * 45 + b"tail",  # odd, binary
    "story/s1-h1-m.c": b"void main(void) {\n  say(\"hi\", 1);\n}\n",  # odd, text
    "story/UPPER.C": b"void main(void) {}\n",  # uppercase ext is still text
    "dink.ini": b"SET_SPRITE_INFO 1 1 0 0 0 0 0 0 0 0\n",  # odd, text
    "map.dat": b"\x01" * 4096,  # already aligned
    "notes.txt": b"hello",  # odd, text
    "empty.bin": b"",  # 0 is a multiple of 2048
    "readonly.bin": b"RO" * 1000,  # CD-ripped data is often 0444
}

TEXT_EXT = ("c", "ini", "txt")


def build_fixture(src: Path) -> None:
    for rel, data in FIXTURE.items():
        p = src / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    (src / "readonly.bin").chmod(0o444)


def main() -> int:
    for tool in (STAGE, PAD):
        if not tool.is_file():
            print("FAIL missing", tool)
            return 1
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "dink"
        dst = Path(td) / "iso" / "dink"
        build_fixture(src)
        r = subprocess.run(
            ["sh", str(STAGE), str(src), str(dst)],
            cwd=ROOT, capture_output=True, text=True,
        )
        if r.returncode != 0:
            print("FAIL stage_dink rc", r.returncode, r.stdout, r.stderr)
            return 1
        for rel, data in FIXTURE.items():
            out = (dst / rel).read_bytes()
            if len(out) % 2048 != 0:
                print("FAIL not padded", rel, len(data), "->", len(out))
                return 1
            if out[: len(data)] != data:
                print("FAIL content changed", rel)
                return 1
            tail = out[len(data):]
            want = b" " if rel.rsplit(".", 1)[-1].lower() in TEXT_EXT else b"\0"
            if tail and tail != want * len(tail):
                print("FAIL wrong pad bytes", rel, tail[:16])
                return 1
        # Padding must be idempotent.
        victim = dst / "graphics" / "struct" / "home" / "dir.ff"
        size = victim.stat().st_size
        r = subprocess.run(
            ["sh", str(PAD), str(victim)], cwd=ROOT, capture_output=True, text=True,
        )
        if r.returncode != 0 or victim.stat().st_size != size:
            print("FAIL pad not idempotent", r.returncode, r.stdout, r.stderr)
            return 1
        # Staging into the source tree must be refused, not rm -rf'd.
        r = subprocess.run(
            ["sh", str(STAGE), str(src), str(src)], cwd=ROOT, capture_output=True, text=True,
        )
        if r.returncode == 0:
            print("FAIL DST == SRC accepted")
            return 1
        src.joinpath("story", "UPPER.C").read_bytes()  # source tree survived
        print("OK", STAGE, f"({len(FIXTURE)} fixture files, all 2048-aligned)")
        return 0


if __name__ == "__main__":
    sys.exit(main())
