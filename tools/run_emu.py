#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Launch the Dinkcast CDI/ELF in Flycast (default) or EMU override."""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

FLATPAK_ID = "org.flycast.Flycast"


def find_flycast() -> list[str] | None:
    env = os.environ.get("FLYCAST")
    if env:
        p = Path(env)
        if p.is_file() or shutil.which(env):
            return [env]
    for name in ("flycast", "flycast-emulator"):
        w = shutil.which(name)
        if w:
            return [w]
    if shutil.which("flatpak"):
        r = subprocess.run(
            ["flatpak", "info", FLATPAK_ID],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if r.returncode == 0:
            return ["flatpak", "run", FLATPAK_ID]
    return None


def resolve_emu(emu: str) -> list[str] | None:
    if emu in ("flycast", "default", ""):
        return find_flycast()
    w = shutil.which(emu)
    if w:
        return [w]
    p = Path(emu)
    if p.is_file():
        return [str(p)]
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--emu", default="flycast")
    ap.add_argument("--image", default="")
    args = ap.parse_args()

    image = (args.image or "").strip()
    if not image:
        print(
            "make emu: no image yet (looked for build/dinkcast.cdi, "
            "dinkcast.cdi, build/dinkcast.elf, dinkcast.elf). "
            "Build Bite 0.2+ first.",
            file=sys.stderr,
        )
        return 2
    ip = Path(image)
    if not ip.is_file():
        print(f"make emu: image not found: {image}", file=sys.stderr)
        return 2

    cmd = resolve_emu(args.emu)
    if not cmd:
        print(
            "make emu: Flycast not found. Install it (e.g. flatpak "
            f"{FLATPAK_ID}), or set FLYCAST=/path/to/flycast, or "
            "EMU=/path/to/other-emulator.",
            file=sys.stderr,
        )
        return 2

    full = cmd + [str(ip.resolve())]
    print("make emu:", " ".join(full))
    os.execvp(full[0], full)


if __name__ == "__main__":
    sys.exit(main())
