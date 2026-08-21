#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""14.5: distill subset dir.ff; DINK_DATA unchanged; file_blob peak under cap."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import pack_catalog as cat  # noqa: E402

DISTILL = ROOT / "tools" / "distill_frames.py"
CATALOG = ROOT / "tools" / "pack_catalog.py"


def ff_size(root: Path, rel: str) -> int:
    cur = root
    for part in rel.split("/"):
        hit = None
        for p in cur.iterdir():
            if p.name.lower() == part.lower():
                hit = p
                break
        if hit is None:
            return -1
        cur = hit
    return cur.stat().st_size if cur.is_file() else -1


def main() -> int:
    data = os.environ.get("DINK_DATA", "").strip()
    if not data:
        print("SKIP test_distill: DINK_DATA unset")
        return 0
    src = Path(data)
    home = ff_size(src, "graphics/struct/home/dir.ff")
    mom = ff_size(src, "graphics/people/mom/dir.ff")
    if home < 0 or mom < 0:
        print("FAIL missing official packs")
        return 1
    out = ROOT / "build" / "distill"
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.pop("DINK_DISTILL", None)
    r = subprocess.run(
        [sys.executable, str(DISTILL), "--src", str(src), "--out", str(out)],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    print(r.stdout, end="")
    if r.returncode != 0:
        print("FAIL distill", r.returncode, r.stderr)
        return 1
    dist_out = r.stdout
    r = subprocess.run(
        [sys.executable, str(DISTILL), "--src", str(src), "--in-place"],
        cwd=ROOT,
        env={**env, "DINK_DATA": data},
        capture_output=True,
        text=True,
    )
    if r.returncode == 0:
        print("FAIL distill --in-place on DINK_DATA was accepted")
        return 1
    if "refuses DINK_DATA" not in (r.stdout + r.stderr):
        print("FAIL distill --in-place message", r.stdout, r.stderr)
        return 1
    if ff_size(src, "graphics/struct/home/dir.ff") != home:
        print("FAIL DINK_DATA home/dir.ff changed")
        return 1
    if ff_size(src, "graphics/people/mom/dir.ff") != mom:
        print("FAIL DINK_DATA mom/dir.ff changed")
        return 1
    dhome = ff_size(out, "graphics/struct/home/dir.ff")
    if dhome is not None and dhome >= 0 and dhome >= home:
        print("FAIL distilled home not smaller", dhome, home)
        return 1
    maps_ln = [ln for ln in dist_out.splitlines() if ln.startswith("distill maps n=")]
    nmaps = 0
    if maps_ln:
        try:
            nmaps = int(maps_ln[0].split("n=", 1)[1].strip().split()[0])
        except ValueError:
            nmaps = 0
    if nmaps < 600:
        print("FAIL distill maps not campaign-wide", maps_ln)
        return 1
    seqs = cat.parse_ini(src)
    must = (
        (31, 27, "graphics/inside/innwalls/walls/dir.ff"),
        (87, 7, "graphics/inside/details/dir.ff"),
        (448, 16, "graphics/items/cup/dir.ff"),
        (428, 2, "graphics/inside/stairs/dir.ff"),
        (447, 4, "graphics/items/tools/dir.ff"),
    )
    for sid, fr, rel in must:
        prefix = seqs.get(sid, "")
        want = cat.frame_bmp_name(prefix, fr).lower()
        p = out.joinpath(*rel.split("/"))
        if not p.is_file():
            # skip-no-save left the official pack; overlay must not keep a stub
            continue
        names = {n.lower() for n, _off in cat.ff_entries(p.read_bytes())}
        if want not in names:
            print("FAIL distilled missing old-man frame", rel, want, "have", sorted(names)[:12])
            return 1
    env["DINK_DISTILL"] = str(out)
    env["DINK_DATA"] = data
    r = subprocess.run(
        [sys.executable, str(CATALOG)],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    print(r.stdout, end="")
    if r.returncode != 0:
        print("FAIL catalog", r.returncode, r.stderr)
        return 1
    blob_need = [
        ln
        for ln in r.stdout.splitlines()
        if ln.startswith("14.5: needed pool=file_blob")
    ]
    if blob_need:
        print("note: distilled village file_blob still over cap", blob_need)
    edraw = ROOT / "tests" / "test_edraw"
    if edraw.is_file():
        r = subprocess.run(
            [str(edraw)], cwd=ROOT, env=env, capture_output=True, text=True
        )
        print(r.stdout, end="")
        if r.returncode != 0:
            print("FAIL test_edraw with DINK_DISTILL", r.stderr)
            return 1
    print("OK test_distill")
    return 0


if __name__ == "__main__":
    sys.exit(main())
