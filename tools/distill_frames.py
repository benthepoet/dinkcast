#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""14.5 distill: subset dir.ff of used 8-bit BMPs (original payloads).

Never writes DINK_DATA. Default out is build/distill/. --in-place rewrites a
staged copy (CDI) that still has official packs. Used-frame union is the
**stock campaign** (every nonempty map.dat screen), not the 14.4a village walk.
Always-resident packs and skip-no-save packs are left whole (host out unlinks
a stale subset).
"""
from __future__ import annotations

import argparse
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import pack_catalog as cat  # noqa: E402

ALWAYS_PREFIX = (
    "graphics/dink/idle/",
    "graphics/dink/walk/",
    "graphics/dink/push/",
    "graphics/dink/hit/",
    "graphics/dink/die/",
    "graphics/inter/text-box/",
    "graphics/inter/arrow/",
    "graphics/inter/menu/",
    "graphics/inter/status/",
    "graphics/inter/numbers/",
    "graphics/inter/health/",
    "graphics/inter/level#/",
    "dink.ini",
    "dink.dat",
    "story/",
)


def is_always(rel: str) -> bool:
    r = rel.replace("\\", "/").lower()
    return any(r == p or r.startswith(p) for p in ALWAYS_PREFIX)


def parse_ff(data: bytes) -> list[tuple[str, bytes]]:
    ents = cat.ff_entries(data)
    out: list[tuple[str, bytes]] = []
    for i, (name, eoff) in enumerate(ents):
        if not name:
            continue
        end = ents[i + 1][1] if i + 1 < len(ents) else len(data)
        if eoff > len(data) or end > len(data) or end < eoff:
            continue
        out.append((name, data[eoff:end]))
    return out


def write_ff(entries: list[tuple[str, bytes]]) -> bytes:
    nent = len(entries) + 1
    hdr = 4 + nent * 17
    off = hdr
    chunks = [struct.pack("<I", nent)]
    bodies: list[bytes] = []
    for name, blob in entries:
        raw = name.encode("latin-1", errors="replace")[:12]
        nm = raw + b"\0" * (13 - len(raw))
        chunks.append(struct.pack("<I", off) + nm)
        bodies.append(blob)
        off += len(blob)
    chunks.append(struct.pack("<I", off) + b"\0" * 13)
    return b"".join(chunks) + b"".join(bodies)


def find_bmp(ents: list[tuple[str, bytes]], want: str) -> tuple[str, bytes] | None:
    w = want.lower()
    for name, blob in ents:
        if name.lower() == w:
            return name, blob
    return None


def merge_need(
    a: dict[int, set[int] | None], b: dict[int, set[int] | None]
) -> dict[int, set[int] | None]:
    out = dict(a)
    for s, frs in b.items():
        if s not in out:
            out[s] = frs if frs is None else set(frs)
        elif out[s] is None or frs is None:
            out[s] = None
        else:
            out[s] = out[s] | frs
    return out


def campaign_need(root: Path) -> tuple[dict[int, str], dict[int, set[int] | None]]:
    """Used frames on every nonempty map.dat screen (stock campaign)."""
    seqs = cat.parse_ini(root)
    loc = cat.load_world_loc(root)
    need: dict[int, set[int] | None] = {}
    maps = cat.campaign_maps(loc)
    print(f"distill maps n={len(maps)}")
    for mmap in maps:
        rec = loc[mmap]
        raw = cat.load_map_rec(root, rec)
        if raw is None:
            continue
        sprites, script, _sheets = cat.parse_screen(raw)
        for vis in cat.screen_visions(sprites):
            need = merge_need(
                need, cat.screen_need(sprites, script, root, seqs, vis)
            )
    return seqs, need


def distill_root(src: Path, dst: Path, in_place: bool) -> int:
    os.environ.pop("DINK_DISTILL", None)
    seqs, need = campaign_need(src)
    keep: dict[str, list[tuple[str, bytes]]] = {}
    orig: dict[str, bytes] = {}
    for sid, frs in need.items():
        prefix = seqs[sid]
        rel, nbytes = cat.seq_pack_bytes(src, prefix, {})
        if is_always(rel):
            if not in_place:
                stale = dst.joinpath(*Path(rel.replace("\\", "/")).parts)
                if stale.is_file():
                    stale.unlink()
                    print(f"distill unlink always {rel}")
            continue
        if nbytes <= 0 or not rel.lower().endswith("dir.ff"):
            continue
        path = cat.find_ci(src, rel)
        if path is None:
            continue
        key = rel.replace("\\", "/").lower()
        if key not in orig:
            orig[key] = path.read_bytes()
            keep[key] = []
        ents = parse_ff(orig[key])
        seen = {n.lower() for n, _ in keep[key]}
        names: list[str] = []
        if frs is None:
            stem = cat.bmp_stem(prefix).lower()
            for name, _blob in ents:
                if name.lower().startswith(stem) and name.lower().endswith(".bmp"):
                    names.append(name)
        else:
            names = [cat.frame_bmp_name(prefix, fr) for fr in sorted(frs)]
        for want in names:
            hit = find_bmp(ents, want)
            if hit is None or hit[0].lower() in seen:
                continue
            keep[key].append(hit)
            seen.add(hit[0].lower())

    n_write = 0
    saved = 0
    for rel, entries in sorted(keep.items()):
        raw = orig[rel]
        if not entries:
            print(f"distill skip empty {rel}")
            if not in_place:
                stale = dst.joinpath(*Path(rel).parts)
                if stale.is_file():
                    stale.unlink()
            continue
        outb = write_ff(entries)
        if len(outb) >= len(raw):
            print(f"distill skip no-save {rel} {len(raw)}")
            # Stale overlay of a previous sparse pack would win on CDI.
            if not in_place:
                stale = dst.joinpath(*Path(rel).parts)
                if stale.is_file():
                    stale.unlink()
            continue
        if in_place:
            outp = cat.find_ci(src, rel)
            if outp is None:
                continue
        else:
            outp = dst.joinpath(*Path(rel).parts)
            outp.parent.mkdir(parents=True, exist_ok=True)
        outp.write_bytes(outb)
        n_write += 1
        saved += len(raw) - len(outb)
        print(f"distill {rel} {len(raw)} -> {len(outb)} n={len(entries)}")
    print(f"distill packs={n_write} saved={saved} out={'in-place' if in_place else dst}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.environ.get("DINK_DATA", "").strip())
    ap.add_argument("--out", default=str(ROOT / "build" / "distill"))
    ap.add_argument("--in-place", action="store_true")
    args = ap.parse_args()
    if not args.src:
        print("SKIP distill: DINK_DATA unset")
        return 0
    src = Path(args.src)
    data_env = os.environ.get("DINK_DATA", "").strip()
    if args.in_place and data_env:
        try:
            if src.resolve() == Path(data_env).resolve():
                print("FAIL distill --in-place refuses DINK_DATA", file=sys.stderr)
                return 1
        except OSError:
            print("FAIL distill --in-place cannot resolve DINK_DATA", file=sys.stderr)
            return 1
    if cat.find_ci(src, "dink.dat") is None:
        print("SKIP distill: no dink.dat (fixture stage)")
        return 0
    dst = Path(args.out)
    if not args.in_place:
        dst.mkdir(parents=True, exist_ok=True)
    return distill_root(src, dst, args.in_place)


if __name__ == "__main__":
    sys.exit(main())
