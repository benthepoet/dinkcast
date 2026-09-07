#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""14.4a catalog must print named screens. Over-cap is a print, not a fail."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "pack_catalog.py"


def main() -> int:
    env = os.environ.copy()
    env.pop("DINK_DISTILL", None)
    if not env.get("DINK_DATA", "").strip():
        print("SKIP test_pack_catalog: DINK_DATA unset")
        return 0
    r = subprocess.run(
        [sys.executable, str(SCRIPT)],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    out = r.stdout + r.stderr
    print(r.stdout, end="")
    if r.returncode != 0:
        print("FAIL pack_catalog exit", r.returncode, r.stderr)
        return 1
    need = (
        "hard.dat FILE* not blob",
        "house vis 0",
        "outdoor 439",
        "duck 441 vis 2",
        "408 girl",
        "pig 407",
        "castle pack",
        "campaign screens=",
        "Always+Screen+Prev peak",
        "OK pack_catalog",
    )
    missing = [s for s in need if s not in out]
    if missing:
        print("FAIL missing", missing)
        return 1
    if "14.5: needed" in out:
        print("note: catalog recorded 14.5: needed (not a fail)")
    subset = None
    for ln in out.splitlines():
        if "toc=" not in ln or "pack=" not in ln or not ln.strip().startswith("seq "):
            continue
        toc = pack = 0
        for part in ln.split():
            if part.startswith("toc="):
                toc = int(part.split("=", 1)[1])
            if part.startswith("pack="):
                pack = int(part.split("=", 1)[1])
        if toc >= 4 and pack > toc:
            subset = (toc, pack, ln)
            break
    if subset is None:
        print("FAIL no seq toc= < pack=")
        return 1
    print("14.6 subset pack file_blob=toc", subset[0], "pack", subset[1])
    peak = [
        ln
        for ln in out.splitlines()
        if ln.startswith("campaign Always+Screen+Prev peak")
    ]
    if peak:
        blob = 0
        for part in peak[0].split():
            if part.startswith("file_blob="):
                blob = int(part.split("=", 1)[1])
        if blob > 4718592:
            print("note: 14.6 catalog peak still over cap", blob)
        else:
            print("14.6 catalog Always+Screen+Prev peak under 4.5 MB", blob)
    sys.path.insert(0, str(ROOT / "tools"))
    import pack_catalog as cat

    bar = cat.script_seqs(Path(env["DINK_DATA"]), "s2-bar")
    if 728 not in bar:
        print("FAIL s2-bar missing attack dir 728", sorted(bar)[-12:])
        return 1
    print("s2-bar attack dirs include 728")
    return 0


if __name__ == "__main__":
    sys.exit(main())
