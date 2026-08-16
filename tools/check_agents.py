#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Structural check of AGENTS.md (workflow contract)."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
AGENTS = ROOT / "AGENTS.md"


def main() -> int:
    text = AGENTS.read_text(encoding="utf-8")
    low = text.lower()
    missing = []

    def need(label: str, pred: bool) -> None:
        if not pred:
            missing.append(label)

    need("primary branch master", "primary branch" in low and "`master`" in text)
    need("feature branch + pull request", "feature branch" in low and "pull request" in low)
    need("orchestrator role", "### orchestrator" in low or "**orchestrator**" in low)
    need("orchestrator named on PR", "orchestrator:" in low)
    need("bar: green", "bar: green" in low)
    need("adversarial reviewer", "adversarial" in low)
    need("memory reviewer", "memory reviewer" in low)
    need("performance reviewer", "performance reviewer" in low)
    need("feedback on the PR", "all review feedback lives on the pr" in low)
    need("implementer != adversarial", "implementer and adversarial" in low)
    need("orchestrator != adversarial", "orchestrator and adversarial" in low)
    need("gpl", "gpl-3.0-or-later" in low)
    need("make emu Flycast", "make emu" in low and "flycast" in low)

    if missing:
        print("FAIL:", "; ".join(missing))
        return 1
    print("OK", AGENTS)
    return 0


if __name__ == "__main__":
    sys.exit(main())
