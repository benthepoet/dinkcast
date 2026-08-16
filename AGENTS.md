# AGENTS.md — Dinkcast

Instructions for humans and agents working in this repo.

**Port spec:** [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md) is the source of truth for bites, budgets, DinkC, 60 FPS, and legal/data rules. Do not fork those decisions here. If implementation and the plan disagree, change the plan in the same PR or follow the plan.

**Product:** Port Dink Smallwood to the Sega Dreamcast (KallistiOS). Original game data is required and is **not** committed unless a file’s license allows it. Use `DINK_DATA`.

---

## Git workflow

- **Primary branch:** `master`. It must stay buildable (`make host`; `make dc` when `KOS_BASE` is set).
- **No direct work on `master`.** Every change is a **feature branch + pull request**.
- Branch names: `bite/3.4-title-quad`, `fix/vram-leak-screen-swap`, `chore/makefile-host`. One concern per branch.
- Rebase on `master` before requesting review. No merge commits on feature branches unless the PR is explicitly integrating a stack.
- Commits: imperative, scoped (`dinkc: yield on say_stop`). Do not mix a formatter sweep with a parser change.
- Land only via PR merge to `master` after the review bar below is green.
- Do not force-push `master`. Force-push a feature branch only to replace a rebase you own, and only before others have based work on it.

```
master
  └── bite/3.4-title-quad          # PR #N
        └── fix/title-letterbox    # stacked PR only if needed
```

---

## Pull requests

Every unit of work is a PR against `master`.

**PR body must include:**

1. **Bite / intent** — plan bite id(s) or “out of plan” with why.
2. **How to test** — host command and/or DC/emulator steps. Name the `DINK_DATA` file used if any.
3. **Budgets** — if the change touches RAM, VRAM, AICA, or disc: expected counters vs plan §1.2.
4. **Risks** — leaks, timing, DinkC compatibility, ISO9660.

**All review feedback lives on the PR.** No side-channel “LGTM” that is not a PR comment or review. Verbal / chat findings are pasted onto the PR before merge. Requested changes are resolved on the PR (reply + code) or explicitly declined on the PR with a reason.

Do not merge with unanswered review threads unless a maintainer records `wontfix: …` on that thread.

---

## Subagent team

Use separate agents (or clearly separated passes) with **different roles**. The implementer does not mark their own PR complete.

| Role | Job | Must write on the PR |
|---|---|---|
| **Implementer** | Code the bite. Follow the plan. Host tests first when the plan says so. | Description, test steps, budget notes |
| **Spec reviewer** | Diff vs [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md): title-before-gameplay, original data formats, no new DinkC dialect, 60 Hz logic, FreeDink interpreter graft. | Approve or list plan violations |
| **Adversarial reviewer** | Try to break it. Wrong endian, missing `dink.dat`, 8.3 names, no VMU, no controller, empty screen, freeze nest, busy-loop script, double evict, title path wrong. Assume the happy path is a lie. | Attack list + repro or “attempted X, held” |
| **Memory reviewer** | Every alloc has a free/evict. Screen change and leave-title must not leak. Check BMP decode buffers, PVR textures, script file buffers, AICA handles. Compare to plan caps. | Leak findings or `mem: no new unpaired alloc` |
| **Performance reviewer** | 60 FPS target, 30 floor. Flag CPU blit, per-frame re-lex of DinkC, preload-the-world, RGBA8888 textures, extra GD-ROM seeks. Do **not** ask for a custom DinkC JIT. | Perf notes or `perf: no 60 Hz regress suspected` |
| **Flaws reviewer** | Logic and compatibility: 1-based sprites, hardness, talk range, SH-4 alignment, little-endian readers, silent no-op vs skipped script. | Defects or `flaws: none blocking` |

**Minimum bar to merge**

- Implementer + **at least two** of: spec, adversarial, memory, performance, flaws.
- **Adversarial** is required on anything that touches `src/` runtime (not required for docs-only).
- **Memory** is required if the PR allocates, uploads textures, loads files, or attaches scripts.
- **Performance** is required if the PR is on the per-frame path or I/O during play/title.
- Docs-only PRs: spec reviewer is enough.

One human or agent may not wear Implementer and Adversarial on the same PR.

---

## How to run a review pass

1. Check out the PR branch; do not review `master` by mistake.
2. Read the PR description, then the diff, then the plan bites named in the PR.
3. For adversarial / memory / perf / flaws: leave **one review** (or a clearly labeled comment block) with severity:

   | Severity | Meaning |
   |---|---|
   | `block` | Must fix or `wontfix` before merge |
   | `should` | Fix in this PR unless schedule is cited |
   | `nit` | Style; author may land and follow up |

4. End with `verdict: request-changes` or `verdict: approve` for that role.
5. Implementer answers on the same threads. Re-review the delta, do not rubber-stamp.

---

## Coding rules (short)

- Plan bites in order for engine work. **No gameplay before Bite 3.4** (official title still).
- Graft FreeDink DinkC; do not invent a language or a “faster” interpreter.
- Little-endian explicit reads; no packed-struct `fread` on SH-4 without a size lock.
- PVR textured quads only — no SDL-style CPU blit as the renderer.
- Log unimplemented DinkC; never skip a script file silently.
- Keep game assets out of git unless license-cleared.
- New source files: SPDX `GPL-3.0-or-later` (see [LICENSE](LICENSE)). Do not add MIT/Apache-only code that cannot be combined with a FreeDink graft.

---

## Checks

- `python3 tools/check_port_plan.py` — plan still has required decisions.
- `make host` — tools and host tests (once the Makefile exists).
- Do not claim DC boot works unless you ran the ELF (hardware or emulator) and say which.
