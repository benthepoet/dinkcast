#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""MODE1/2352 wrap and ISO LBA rebase helpers (no full 620 MiB ISO)."""
from __future__ import annotations

import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gdrom_from_msiso as g  # noqa: E402


def main() -> int:
    hdr = g.lba_to_header(45000)
    if len(hdr) != 4 or hdr[3] != 0x01:
        print("FAIL header", hdr)
        return 1
    iso = bytes(2048)
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "t.bin"
        n = g.write_mode1_2352(iso, p, 45000)
        if n != 1 or p.stat().st_size != 2352:
            print("FAIL 2352 size", n, p.stat().st_size)
            return 1
        raw = p.read_bytes()
        if raw[:12] != g.SYNC or raw[16:2064] != iso:
            print("FAIL 2352 layout")
            return 1
    # Tiny fake PVD+root dir at session 11702: file sector 0 = LBA 11702.
    session, gd = 11702, 45000
    buf = bytearray(2048 * 24)
    pvd = 16 * 2048
    buf[pvd] = 1
    buf[pvd + 1 : pvd + 6] = b"CD001"
    g.put_both_u32(buf, pvd + 80, 24)
    g.put_both_u32(buf, pvd + 132, 10)
    struct.pack_into("<I", buf, pvd + 140, session + 20)
    struct.pack_into(">I", buf, pvd + 148, session + 20)
    # root dir record inside PVD at 156: length 34, extent session+19
    buf[pvd + 156] = 34
    g.put_both_u32(buf, pvd + 156 + 2, session + 19)
    g.put_both_u32(buf, pvd + 156 + 10, 2048)
    buf[pvd + 156 + 25] = 2
    buf[pvd + 156 + 32] = 1
    buf[pvd + 156 + 33] = 0
    # root directory sector 19: . and ..
    d = (19) * 2048
    buf[d] = 34
    g.put_both_u32(buf, d + 2, session + 19)
    g.put_both_u32(buf, d + 10, 2048)
    buf[d + 25] = 2
    buf[d + 32] = 1
    buf[d + 33] = 0
    buf[d + 34] = 34
    g.put_both_u32(buf, d + 36, session + 19)
    g.put_both_u32(buf, d + 44, 2048)
    buf[d + 59] = 2
    buf[d + 66] = 1
    buf[d + 67] = 1
    g.rebase_iso(buf, session, gd)
    root = g.both_u32(buf, pvd + 156 + 2)
    if root != gd + 19:
        print("FAIL rebased root 45000", root)
        return 1
    session, gd0 = 11702, 0
    buf0 = bytearray(2048 * 24)
    buf0[pvd] = 1
    buf0[pvd + 1 : pvd + 6] = b"CD001"
    g.put_both_u32(buf0, pvd + 80, 24)
    g.put_both_u32(buf0, pvd + 132, 10)
    struct.pack_into("<I", buf0, pvd + 140, session + 20)
    struct.pack_into(">I", buf0, pvd + 148, session + 20)
    buf0[pvd + 156] = 34
    g.put_both_u32(buf0, pvd + 156 + 2, session + 19)
    g.put_both_u32(buf0, pvd + 156 + 10, 2048)
    buf0[pvd + 156 + 25] = 2
    buf0[pvd + 156 + 32] = 1
    buf0[pvd + 156 + 33] = 0
    buf0[d] = 34
    g.put_both_u32(buf0, d + 2, session + 19)
    g.put_both_u32(buf0, d + 10, 2048)
    buf0[d + 25] = 2
    buf0[d + 32] = 1
    buf0[d + 33] = 0
    buf0[d + 34] = 34
    g.put_both_u32(buf0, d + 36, session + 19)
    g.put_both_u32(buf0, d + 44, 2048)
    buf0[d + 59] = 2
    buf0[d + 66] = 1
    buf0[d + 67] = 1
    g.rebase_iso(buf0, session, gd0)
    root0 = g.both_u32(buf0, pvd + 156 + 2)
    if root0 != 19:
        print("FAIL rebased root 0", root0)
        return 1
    print("OK test_gdrom_from_msiso")
    return 0


if __name__ == "__main__":
    sys.exit(main())
