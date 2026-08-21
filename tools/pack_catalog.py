#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""14.4a residency catalog. No game pixels in git. Over-cap prints 14.5: needed."""
from __future__ import annotations

import os
import re
import struct
import sys
from pathlib import Path

RECSIZE = 31280
SPRITE_OFF = 8020
SPRITE_STRIDE = 220
SCRIPT_OFF = 30240
DAT_IDENT = 20
SLOTS = 769
PIN = 80 * 1024
BLOB_CAP = int(4.5 * 1024 * 1024)
CPU_CAP = int(2.0 * 1024 * 1024)
TS_CAP = int(1.25 * 1024 * 1024)

ALWAYS_SEQ = [14]
ALWAYS_SEQ += [s for s in range(71, 80) if s != 75]
ALWAYS_SEQ += [312, 314, 316, 318]
ALWAYS_SEQ += [102, 104, 106, 108]
ALWAYS_SEQ += [30, 456, 457]


def find_ci(root: Path, rel: str) -> Path | None:
    cur = root
    for part in rel.replace("\\", "/").split("/"):
        if part == "":
            continue
        want = part.lower()
        hit = None
        try:
            for p in cur.iterdir():
                if p.name.lower() == want:
                    hit = p
                    break
        except FileNotFoundError:
            return None
        if hit is None:
            return None
        cur = hit
    return cur


def le_i32(b: bytes, off: int) -> int:
    return struct.unpack_from("<i", b, off)[0]


def le_u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def next_pow2(v: int) -> int:
    p = 1
    while p < v:
        p <<= 1
    return p if p > 0 else 1


def pot1555(w: int, h: int) -> int:
    return next_pow2(max(1, w)) * next_pow2(max(1, h)) * 2


def bmp_wh(data: bytes) -> tuple[int, int] | None:
    if len(data) < 26 or data[0:2] != b"BM":
        return None
    w = le_i32(data, 18)
    h = abs(le_i32(data, 22))
    if w < 1 or h < 1 or w > 4096 or h > 4096:
        return None
    return w, h


def parse_ini(root: Path) -> dict[int, str]:
    path = find_ci(root, "dink.ini")
    if path is None:
        return {}
    text = path.read_text(encoding="latin-1", errors="replace")
    seqs: dict[int, str] = {}
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("//") or s == "":
            continue
        low = s.lower()
        if not low.startswith("load_sequence"):
            continue
        # load_sequence[_now] path seq ...
        parts = s.replace("\\", "/").split()
        if len(parts) < 3:
            continue
        try:
            sid = int(parts[2])
        except ValueError:
            continue
        seqs[sid] = parts[1]
    return seqs


def pack_rel(prefix: str) -> str:
    p = prefix.replace("\\", "/").rstrip("-")
    slash = p.rfind("/")
    if slash < 0:
        return p + "/dir.ff"
    return p[:slash] + "/dir.ff"


def loose_bmp_rel(prefix: str, frame: int) -> str:
    p = prefix.replace("\\", "/")
    return f"{p}{frame:02d}.bmp" if frame < 10 else f"{p}{frame}.bmp"


def load_world_loc(root: Path) -> list[int]:
    path = find_ci(root, "dink.dat")
    if path is None:
        return [0] * SLOTS
    raw = path.read_bytes()
    loc = [0] * SLOTS
    off = DAT_IDENT
    for i in range(SLOTS):
        if off + 4 > len(raw):
            break
        loc[i] = le_i32(raw, off)
        off += 4
    return loc


def load_map_rec(root: Path, rec: int) -> bytes | None:
    path = find_ci(root, "map.dat")
    if path is None or rec < 1:
        return None
    with path.open("rb") as f:
        f.seek((rec - 1) * RECSIZE)
        b = f.read(RECSIZE)
    return b if len(b) == RECSIZE else None


def parse_screen(raw: bytes) -> tuple[list[dict], str, set[int]]:
    sprites: list[dict] = []
    for i in range(101):
        off = SPRITE_OFF + i * SPRITE_STRIDE
        if off + 200 > len(raw):
            break
        active = raw[off + 24]
        seq = le_i32(raw, off + 8)
        frame = le_i32(raw, off + 12)
        typ = le_i32(raw, off + 16)
        brain = le_i32(raw, off + 36)
        base_walk = le_i32(raw, off + 96)
        vision = le_i32(raw, off + 188)
        parm_seq = le_i32(raw, off + 156)
        base_die = le_i32(raw, off + 160)
        script = raw[off + 40 : off + 53].split(b"\0", 1)[0].decode(
            "latin-1", errors="replace"
        )
        sprites.append(
            {
                "i": i,
                "active": active,
                "seq": seq,
                "frame": frame if frame >= 1 else 1,
                "type": typ,
                "brain": brain,
                "base_walk": base_walk,
                "vision": vision,
                "parm_seq": parm_seq,
                "base_die": base_die,
                "script": script.strip(),
            }
        )
    name = raw[SCRIPT_OFF : SCRIPT_OFF + 20].split(b"\0", 1)[0].decode(
        "latin-1", errors="replace"
    ).strip()
    sheets: set[int] = set()
    off = 20
    for i in range(97):
        idx = le_i32(raw, off)
        if idx > 0:
            sheets.add(idx // 128)
        off += 80
    return sprites, name, sheets


def script_seqs(root: Path, name: str) -> set[int]:
    out: set[int] = set()
    if not name:
        return out
    rel = f"story/{name}.c"
    path = find_ci(root, rel)
    if path is None:
        path = find_ci(root, f"story/{name}")
    if path is None:
        return out
    text = path.read_text(encoding="latin-1", errors="replace")
    for m in re.finditer(r"preload_seq\s*\(\s*(\d+)", text, re.I):
        out.add(int(m.group(1)))
    for m in re.finditer(
        r"create_sprite\s*\([^;]*?,\s*(\d+)\s*,\s*\d+\s*\)", text, re.I
    ):
        out.add(int(m.group(1)))
    for m in re.finditer(r"sp_base_walk\s*\([^,]+,\s*(\d+)", text, re.I):
        base = int(m.group(1))
        for d in (1, 2, 3, 4, 6, 7, 8, 9):
            out.add(base + d)
    return out


def ff_entries(data: bytes) -> list[tuple[str, int]]:
    if len(data) < 4:
        return []
    nent = le_u32(data, 0)
    if nent < 2 or nent > 4096:
        return []
    off = 4
    ents: list[tuple[str, int]] = []
    for _ in range(nent):
        if off + 17 > len(data):
            break
        eoff = le_u32(data, off)
        name = data[off + 4 : off + 16].split(b"\0", 1)[0].decode(
            "latin-1", errors="replace"
        )
        ents.append((name, eoff))
        off += 17
    return ents


def pack_frame_pots(root: Path, prefix: str, cache: dict) -> list[int]:
    rel = pack_rel(prefix)
    key = rel.lower()
    if key not in cache:
        path = find_ci(root, rel)
        cache[key] = path.read_bytes() if path is not None else b""
    data = cache[key]
    pots: list[int] = []
    if data:
        ents = ff_entries(data)
        for i, (name, eoff) in enumerate(ents):
            end = ents[i + 1][1] if i + 1 < len(ents) else len(data)
            if eoff >= len(data) or not name.lower().endswith(".bmp"):
                continue
            wh = bmp_wh(data[eoff:end])
            if wh:
                pots.append(pot1555(wh[0], wh[1]))
        return pots
    for fr in range(1, 50):
        bp = find_ci(root, loose_bmp_rel(prefix, fr))
        if bp is None:
            break
        wh = bmp_wh(bp.read_bytes())
        if wh:
            pots.append(pot1555(wh[0], wh[1]))
    return pots


def seq_pack_bytes(root: Path, prefix: str, cache: dict) -> tuple[str, int]:
    rel = pack_rel(prefix)
    path = find_ci(root, rel)
    if path is not None:
        return rel.replace("\\", "/").lower(), path.stat().st_size
    total = 0
    n = 0
    for fr in range(1, 50):
        bp = find_ci(root, loose_bmp_rel(prefix, fr))
        if bp is None:
            break
        total += bp.stat().st_size
        n += 1
    if n:
        return prefix.replace("\\", "/").lower() + "*.bmp", total
    return rel.replace("\\", "/").lower(), 0


def walk_seqs(seqs: dict[int, str], base: int) -> set[int]:
    out: set[int] = set()
    if base <= 0:
        return out
    for d in (1, 2, 3, 4, 6, 7, 8, 9):
        s = base + d
        if s in seqs:
            out.add(s)
    return out


def screen_seqs(
    sprites: list[dict], script: str, root: Path, seqs: dict[int, str], vision: int
) -> set[int]:
    need: set[int] = set()
    names = [script]
    for sp in sprites:
        if not sp["active"]:
            continue
        if sp["vision"] not in (0, vision):
            continue
        if sp["type"] != 2 and sp["seq"] > 0:
            need.add(sp["seq"])
        if sp["parm_seq"] > 0:
            need.add(sp["parm_seq"])
        need |= walk_seqs(seqs, sp["base_walk"])
        if sp["base_die"] > 0:
            need.add(sp["base_die"])
        br = sp["brain"]
        if br in (3, 4, 9, 10):
            need.add(164)
        if br == 3:
            need |= walk_seqs(seqs, 110)
            need |= walk_seqs(seqs, 120)
        if sp["script"]:
            names.append(sp["script"])
    for nm in names:
        extra = script_seqs(root, nm)
        need |= extra
        for s in list(extra):
            # create_sprite seq may be a facing; also treat as base
            need |= walk_seqs(seqs, s)
            need |= walk_seqs(seqs, s - (s % 10))
    return {s for s in need if s in seqs}


def sheet_info(root: Path, sheet0: int) -> tuple[int, int]:
    rel = f"tiles/ts{sheet0 + 1:02d}.bmp"
    path = find_ci(root, rel)
    if path is None:
        return 0, 0
    blob = path.stat().st_size
    wh = bmp_wh(path.read_bytes()[:64] if path.stat().st_size >= 64 else b"")
    if wh is None:
        data = path.read_bytes()
        wh = bmp_wh(data)
    rgb = 0
    if wh:
        rgb = wh[0] * wh[1] * 2
    return blob, rgb


def always_packs(root: Path, seqs: dict[int, str], cache: dict) -> dict[str, int]:
    packs: dict[str, int] = {}
    for s in ALWAYS_SEQ:
        if s not in seqs:
            continue
        rel, n = seq_pack_bytes(root, seqs[s], cache)
        if n:
            packs[rel] = n
    for name, rel in (("dink.ini", "dink.ini"), ("dink.dat", "dink.dat")):
        p = find_ci(root, rel)
        if p is not None:
            packs[rel] = p.stat().st_size
    return packs


def catalog_screen(
    root: Path,
    seqs: dict[int, str],
    loc: list[int],
    player_map: int,
    vision: int,
    cache: dict,
    prev_packs: dict[str, int] | None,
) -> dict:
    rec = loc[player_map] if 0 <= player_map < len(loc) else 0
    raw = load_map_rec(root, rec)
    if raw is None:
        return {"error": f"no map rec for {player_map}"}
    sprites, script, sheets = parse_screen(raw)
    need = screen_seqs(sprites, script, root, seqs, vision)
    packs: dict[str, int] = {}
    cpu = 0
    seq_rows = []
    counted_pots: set[str] = set()
    for s in sorted(need):
        rel, nbytes = seq_pack_bytes(root, seqs[s], cache)
        packs[rel] = nbytes
        pots = pack_frame_pots(root, seqs[s], cache)
        if rel not in counted_pots:
            cpu += sum(pots)
            counted_pots.add(rel)
        seq_rows.append((s, rel, nbytes, len(pots), sum(pots)))
    ts_blob = 0
    ts_rgb = 0
    ts_list = []
    for sh in sorted(sheets):
        b, rgb = sheet_info(root, sh)
        ts_blob += b
        ts_rgb += rgb
        ts_list.append((sh + 1, b, rgb))
    alw = always_packs(root, seqs, cache)
    union = dict(alw)
    union.update(packs)
    keep = sum(union.values()) + ts_blob
    drop_blob = sum(alw.values()) + ts_blob
    prev = prev_packs or {}
    peak_union = dict(alw)
    peak_union.update(packs)
    peak_union.update(prev)
    peak = sum(peak_union.values()) + ts_blob
    # union prev and screen packs for keep-whole
    return {
        "map": player_map,
        "rec": rec,
        "vision": vision,
        "script": script,
        "packs": packs,
        "seq_rows": seq_rows,
        "ts": ts_list,
        "ts_blob": ts_blob,
        "ts_rgb": ts_rgb,
        "cpu": cpu,
        "keep_whole": keep,
        "drop_after": drop_blob,
        "peak": peak,
        "always": sum(alw.values()),
        "alw": alw,
        "prev": sum(prev.values()),
    }


def print_screen(label: str, c: dict) -> None:
    print(f"=== {label} map={c['map']} rec={c['rec']} vis={c['vision']} "
          f"script={c['script']!r} ===")
    print(f"  always_packs={c['always']} screen_packs={sum(c['packs'].values())} "
          f"prev_packs={c['prev']} ts_blob={c['ts_blob']}")
    print(f"  keep_whole file_blob={c['keep_whole']} "
          f"drop_after_decode file_blob={c['drop_after']} "
          f"peak_during_load={c['peak']}")
    print(f"  cpu_pixels_all_frames={c['cpu']} ts_rgb={c['ts_rgb']}")
    for s, rel, n, nf, pot in c["seq_rows"]:
        print(f"  seq {s} {rel} pack={n} frames={nf} pot1555={pot}")
    for ts, b, rgb in c["ts"]:
        print(f"  ts{ts:02d} blob={b} rgb565={rgb}")
    for pool, val, cap in (
        ("file_blob", c["peak"], BLOB_CAP),
        ("cpu_pixels", c["cpu"], CPU_CAP),
        ("ts_rgb", c["ts_rgb"], TS_CAP),
    ):
        if val > cap:
            print(f"14.5: needed pool={pool} bytes={val} cap={cap}")


def main() -> int:
    raw = os.environ.get("DINK_DATA", "").strip()
    if not raw:
        print("SKIP pack_catalog: DINK_DATA unset")
        return 0
    root = Path(raw)
    if find_ci(root, "dink.dat") is None:
        print("FAIL no dink.dat in DINK_DATA", file=sys.stderr)
        return 1
    seqs = parse_ini(root)
    loc = load_world_loc(root)
    cache: dict = {}
    hard = find_ci(root, "hard.dat")
    print("hard.dat FILE* not blob", hard.stat().st_size if hard else 0)

    house = catalog_screen(root, seqs, loc, 1, 0, cache, None)
    print_screen("house vis 0", house)
    m439 = catalog_screen(root, seqs, loc, 439, 0, cache, house["packs"])
    print_screen("outdoor 439", m439)
    duck = catalog_screen(root, seqs, loc, 441, 2, cache, m439["packs"])
    print_screen("duck 441 vis 2", duck)
    m408 = catalog_screen(root, seqs, loc, 408, 0, cache, duck["packs"])
    print_screen("408 girl", m408)
    pig = catalog_screen(root, seqs, loc, 407, 0, cache, m408["packs"])
    print_screen("pig 407", pig)

    castle = find_ci(root, "graphics/struct/Castle/dir.ff")
    print("castle pack", castle.stat().st_size if castle else 0)

    # Would-reopen ≥80 KB under two-screen-old drop (14.4b model).
    m439b = catalog_screen(root, seqs, loc, 439, 0, cache, pig["packs"])
    houseb = catalog_screen(root, seqs, loc, 1, 0, cache, m439b["packs"])
    walk = [
        ("house", house),
        ("439", m439),
        ("441v2", duck),
        ("408", m408),
        ("407", pig),
        ("439b", m439b),
        ("houseb", houseb),
    ]
    seen: dict[str, int] = {}
    prev: set[str] = set()
    alw = set(house["alw"].keys())
    print("=== would-reopen hops (14.4b two-screen-old drop, ≥80KB) ===")
    for name, c in walk:
        cur = set(c["packs"].keys())
        reopen = []
        for rel in sorted(cur):
            n = c["packs"][rel]
            if rel in alw or n < PIN:
                continue
            if rel in seen and rel not in cur.intersection(prev) and rel not in alw:
                if rel not in prev:
                    reopen.append((rel, n))
        for rel, n in reopen:
            print(f"  hop {name} reopen {rel} {n}")
        for rel, n in c["packs"].items():
            seen[rel] = n
        prev = cur
    print("OK pack_catalog")
    return 0


if __name__ == "__main__":
    sys.exit(main())
