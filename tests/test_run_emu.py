#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Drive tools/run_emu.py: no image fails; Flycast argv toggles SCIF serial."""
import importlib.util
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "run_emu.py"


def python_bin() -> str:
    exe = sys.executable
    if "python" in Path(exe).name.lower() and "appimage" not in exe.lower():
        return exe
    for name in ("python3.14", "python3"):
        w = shutil.which(name)
        if w and "appimage" not in w.lower():
            return w
    return exe


def run(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [python_bin(), str(SCRIPT), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def main() -> int:
    empty = run(["--emu", "flycast", "--image", ""])
    if empty.returncode != 2 or "no image yet" not in empty.stderr:
        print("FAIL empty image:", empty.returncode, empty.stderr)
        return 1
    missing = run(["--emu", "flycast", "--image", str(ROOT / "no-such.cdi")])
    if missing.returncode != 2 or "not found" not in missing.stderr:
        print("FAIL missing image:", missing.returncode, missing.stderr)
        return 1
    spec = importlib.util.spec_from_file_location("run_emu", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    argv = mod.flycast_cmd(["/usr/bin/flycast"], Path("/tmp/x.cdi"))
    joined = " ".join(argv)
    if "Debug.SerialConsoleEnabled=yes" not in joined:
        print("FAIL serial flag missing:", argv)
        return 1
    quiet = mod.flycast_cmd(["/usr/bin/flycast"], Path("/tmp/x.cdi"), serial=False)
    qj = " ".join(quiet)
    if "Debug.SerialConsoleEnabled=no" not in qj:
        print("FAIL serial off missing:", quiet)
        return 1
    if "Debug.SerialConsoleEnabled=yes" in qj:
        print("FAIL serial still on:", quiet)
        return 1
    if str(Path("/tmp/x.cdi")) not in argv:
        print("FAIL image missing:", argv)
        return 1
    logp = ROOT / "build" / "test-emu-tee.log"
    if logp.exists():
        logp.unlink()
    rc = mod.tee_run([python_bin(), "-c", "print('emu-tee-ok', flush=True)"], logp)
    if rc != 0:
        print("FAIL tee rc", rc)
        return 1
    text = logp.read_text(encoding="utf-8")
    if "emu-tee-ok" not in text:
        print("FAIL tee log", text)
        return 1
    logp.unlink(missing_ok=True)
    if getattr(mod, "DEFAULT_LOG", "") != "build/emu.log":
        print("FAIL default log", getattr(mod, "DEFAULT_LOG", None))
        return 1
    print("OK", SCRIPT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
