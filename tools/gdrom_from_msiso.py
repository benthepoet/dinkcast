#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rebase a mkdcdisc multi-session ISO to LBA 0 and emit MODE1/2352 tracks.

mkdcdisc -I writes 2048-byte sectors whose ISO9660 LBAs start at session
LBA 11702. Redream boots track 1 at LBA 0; an empty dummy there is the BIOS
menu. Put the game ISO on track 1 (2352, rebased to 0) and tiny dummy
tracks after it so the CHD has 3 tracks. Flycast `make emu` still uses the
MIL-CD CUE CHD, not this output.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

SYNC = b"\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00"
MKDCDISC_SESSION = 11702
GD_HD_LBA = 45000


def bcd(n: int) -> int:
    return ((n // 10) % 10) << 4 | (n % 10)


def lba_to_header(lba: int) -> bytes:
    t = lba + 150
    m, rem = divmod(t, 75 * 60)
    s, f = divmod(rem, 75)
    return bytes([bcd(m), bcd(s), bcd(f), 0x01])


def both_u32(buf: bytearray, off: int) -> int:
    le = struct.unpack_from("<I", buf, off)[0]
    be = struct.unpack_from(">I", buf, off + 4)[0]
    if le != be:
        raise ValueError(f"both-endian mismatch at {off}: {le} vs {be}")
    return le


def put_both_u32(buf: bytearray, off: int, val: int) -> None:
    struct.pack_into("<I", buf, off, val)
    struct.pack_into(">I", buf, off + 4, val)


def rebase_iso(iso: bytearray, session: int, gd_start: int) -> None:
    if len(iso) % 2048 != 0:
        raise ValueError("ISO size is not a multiple of 2048")
    nsec = len(iso) // 2048
    delta = gd_start - session
    lo, hi = session, session + nsec

    def in_vol(lba: int) -> bool:
        return lo <= lba < hi

    def foff(lba: int) -> int:
        return (lba - session) * 2048

    def bump_loc(buf: bytearray, off: int) -> int:
        old = both_u32(buf, off)
        if not in_vol(old):
            return old
        new = old + delta
        put_both_u32(buf, off, new)
        return new

    seen_dirs: set[int] = set()

    def walk_dir(lba: int, size: int) -> None:
        if lba in seen_dirs or not in_vol(lba) or size <= 0:
            return
        seen_dirs.add(lba)
        data = memoryview(iso)[foff(lba) : foff(lba) + size]
        pos = 0
        while pos + 33 <= size:
            rec_len = data[pos]
            if rec_len == 0:
                pos = (pos + 2048) // 2048 * 2048
                continue
            if pos + rec_len > size:
                break
            rec = bytearray(iso[foff(lba) + pos : foff(lba) + pos + rec_len])
            ext = both_u32(rec, 2)
            dsz = both_u32(rec, 10)
            flags = rec[25]
            namelen = rec[32]
            name = bytes(rec[33 : 33 + namelen])
            bump_loc(iso, foff(lba) + pos + 2)
            if (flags & 2) and name not in (b"\x00", b"\x01"):
                walk_dir(ext, dsz)
            pos += rec_len

    def patch_path_table(lba: int, size: int, big: bool) -> None:
        if not in_vol(lba) or size <= 0:
            return
        off = foff(lba)
        end = off + size
        pos = off
        while pos + 8 <= end:
            namelen = iso[pos]
            if namelen == 0:
                break
            fmt = ">I" if big else "<I"
            old = struct.unpack_from(fmt, iso, pos + 2)[0]
            if in_vol(old):
                struct.pack_into(fmt, iso, pos + 2, old + delta)
            rec = 8 + namelen + (namelen & 1)
            pos += rec

    for sec in range(16, 32):
        base = sec * 2048
        if iso[base + 1 : base + 6] != b"CD001":
            continue
        vtype = iso[base]
        if vtype == 255:
            break
        if vtype not in (1, 2):
            continue
        pt_size = both_u32(iso, base + 132)
        pt_le = struct.unpack_from("<I", iso, base + 140)[0]
        pt_le_opt = struct.unpack_from("<I", iso, base + 144)[0]
        pt_be = struct.unpack_from(">I", iso, base + 148)[0]
        pt_be_opt = struct.unpack_from(">I", iso, base + 152)[0]
        root_ext = both_u32(iso, base + 156 + 2)
        root_sz = both_u32(iso, base + 156 + 10)
        put_both_u32(iso, base + 80, gd_start + nsec)
        if in_vol(pt_le):
            struct.pack_into("<I", iso, base + 140, pt_le + delta)
        if in_vol(pt_le_opt):
            struct.pack_into("<I", iso, base + 144, pt_le_opt + delta)
        if in_vol(pt_be):
            struct.pack_into(">I", iso, base + 148, pt_be + delta)
        if in_vol(pt_be_opt):
            struct.pack_into(">I", iso, base + 152, pt_be_opt + delta)
        bump_loc(iso, base + 156 + 2)
        walk_dir(root_ext, root_sz)
        if in_vol(pt_le):
            patch_path_table(pt_le, pt_size, False)
        if in_vol(pt_le_opt):
            patch_path_table(pt_le_opt, pt_size, False)
        if in_vol(pt_be):
            patch_path_table(pt_be, pt_size, True)
        if in_vol(pt_be_opt):
            patch_path_table(pt_be_opt, pt_size, True)


def write_mode1_2352(iso_2048: bytes, out: Path, start_lba: int) -> int:
    nsec = len(iso_2048) // 2048
    out.parent.mkdir(parents=True, exist_ok=True)
    pad = bytes(288)
    with out.open("wb") as f:
        for i in range(nsec):
            sector = iso_2048[i * 2048 : (i + 1) * 2048]
            f.write(SYNC)
            f.write(lba_to_header(start_lba + i))
            f.write(sector)
            f.write(pad)
    return nsec


def write_dummy_data_2352(path: Path, frames: int, start_lba: int) -> None:
    empty = bytes(2048)
    pad = bytes(288)
    with path.open("wb") as f:
        for i in range(frames):
            f.write(SYNC)
            f.write(lba_to_header(start_lba + i))
            f.write(empty)
            f.write(pad)


def write_dummy_audio_2352(path: Path, frames: int) -> None:
    with path.open("wb") as f:
        f.write(bytes(frames * 2352))


def write_gdi(path: Path, t1: str, t2: str, t3: str, data_lba: int, data_frames: int) -> None:
    end = data_lba + data_frames
    path.write_text(
        "3\n"
        f"1 {data_lba} 4 2352 {t1} 0\n"
        f"2 {end} 0 2352 {t2} 0\n"
        f"3 {end} 4 2352 {t3} 0\n",
        encoding="ascii",
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("iso")
    ap.add_argument("outdir")
    ap.add_argument("--session-start", type=int, default=MKDCDISC_SESSION)
    ap.add_argument("--gd-start", type=int, default=0)
    ap.add_argument("--ld-frames", type=int, default=300)
    ap.add_argument("--audio-frames", type=int, default=300)
    args = ap.parse_args()
    iso_path = Path(args.iso)
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    raw = bytearray(iso_path.read_bytes())
    pvd = 16 * 2048
    orig_root = both_u32(raw, pvd + 156 + 2)
    rebase_iso(raw, args.session_start, args.gd_start)
    root = both_u32(raw, pvd + 156 + 2)
    want = orig_root + (args.gd_start - args.session_start)
    if root != want:
        print(f"FAIL rebase root {root} want {want}", file=sys.stderr)
        return 1
    t1 = "track01.bin"
    t2 = "track02.raw"
    t3 = "track03.bin"
    n = write_mode1_2352(bytes(raw), outdir / t1, args.gd_start)
    write_dummy_audio_2352(outdir / t2, args.audio_frames)
    write_dummy_data_2352(outdir / t3, args.ld_frames, args.gd_start + n)
    write_gdi(outdir / "dinkcast-redream.gdi", t1, t2, t3, args.gd_start, n)
    print(f"redream iso sectors {n} root LBA {root} gdi {outdir / 'dinkcast-redream.gdi'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
