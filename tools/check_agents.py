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
    need("adversarial reviewer", "adversarial" in low and "the only reviewer" in low)
    need("do not spawn extra reviewers", "do **not** spawn spec" in low or "do not spawn spec" in low)
    need("dreamcast expert reviewer", "dreamcast" in low and "kos" in low)
    need("feedback on the PR", "all review feedback lives on the pr" in low)
    need("implementer != adversarial", "implementer and adversarial" in low)
    need("orchestrator != adversarial", "orchestrator and adversarial" in low)
    need("gpl", "gpl-3.0-or-later" in low)
    need("make emu Flycast", "make emu" in low and "flycast" in low)
    need("human gate after merge", "after a pr is" in low and "merged" in low and "until the human" in low)
    need("PROGRESS.md required", "progress.md" in low)
    need("visual milestone gate", "visual-gate" in low and "6.3" in low)
    need("troubleshooting", "troubleshooting" in low and "debug orchestrator" in low)

    if missing:
        print("FAIL:", "; ".join(missing))
        return 1
    print("OK", AGENTS)
    return 0


if __name__ == "__main__":
    sys.exit(main())
