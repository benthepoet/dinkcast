# AGENTS.md — Dinkcast

Instructions for humans and agents working in this repo.

**Port spec:** [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md) is the source of truth for bites, budgets, DinkC, 60 FPS, and legal/data rules. Do not fork those decisions here. If implementation and the plan disagree, change the plan in the same PR or follow the plan.

**Dreamcast skill:** [.grok/skills/dreamcast-kos/SKILL.md](.grok/skills/dreamcast-kos/SKILL.md). Learned failures live in [docs/GOTCHAS.md](docs/GOTCHAS.md) — read before FS/CDI/PVR/Docker work; append a class-of-mistake bullet when we learn one.

**Progress log:** [PROGRESS.md](PROGRESS.md) is what has landed. Every bite/fix PR must update it (master table + bite status) in the **same PR**. Do not treat a green `make host` as “tracked.”

**Product:** Port Dink Smallwood to the Sega Dreamcast (KallistiOS). Original game data is required and is **not** committed unless a file’s license allows it. Use `DINK_DATA`.

**Kickoff order:** (1) add `origin`, push `master`. (2) Orchestrator opens `bite/0.1-…` (then 0.2, 1.x, … 3.4 title). (3) Use `.github/PULL_REQUEST_TEMPLATE.md`. (4) `make host` must stay green on `master`.

**Human gate:** After a PR is **merged** to `master`, **stop**. Do not open the next bite, spawn the next implementer, or start more engine work until the human requester explicitly approves. Reviews and fixes on an *open* PR may continue.

**Exception (active):** requester authorized non-stop work through **Bite 3.4** (title quad). Stop again after 3.4 is on `master` and a host title preview exists for review.

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

### Orchestrator

Someone **must** hold **Orchestrator** on every PR. This role coordinates implementation and review; it is not a silent merge button.

| Does | Does not |
|---|---|
| Pick the next plan bite (or justified out-of-plan fix). Open or name the branch/PR. | Implement the bite on the same PR (unless the change is docs-only and they say so). |
| Assign the required reviewer roles. Chase missing reviews. | Wear **Adversarial** on a PR they orchestrate. |
| Confirm the merge bar (roles, unresolved threads, tests named). | Rubber-stamp. If a required review is missing, they wait or request it. |
| Merge to `master` (or record who merged) only when the bar is green. | Bypass PR comments; coordination notes go on the PR. |
| Sequence stacks (`bite/…` then follow-ups). Rebase policy. | Rewrite the port plan without a PR. |

**On the PR the orchestrator writes:** `orchestrator: @who` — bite id, required reviews for this diff, and later `bar: green` / `bar: blocked (why)` before merge.

**Default orchestrator is a dedicated subagent**, not the human requester and not the implementer. Spawn it for every PR. The human may override in writing on that PR. One agent may Orchestrate PR A and Implement PR B, not both on the same PR (docs-only exception above).

| Role | Job | Must write on the PR |
|---|---|---|
| **Orchestrator** | Sequence work, assign reviews, enforce the bar, merge. | `orchestrator:` assignment + `bar:` |
| **Implementer** | Code the bite. Follow the plan. Host tests first when the plan says so. | Description, test steps, budget notes |
| **Spec reviewer** | Diff vs [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md): title-before-gameplay, original data formats, no new DinkC dialect, 60 Hz logic, FreeDink interpreter graft. | Approve or list plan violations |
| **Adversarial reviewer** | Try to break it. Wrong endian, missing `dink.dat`, 8.3 names, no VMU, no controller, empty screen, freeze nest, busy-loop script, double evict, title path wrong. Assume the happy path is a lie. | Attack list + repro or “attempted X, held” |
| **Memory reviewer** | Every alloc has a free/evict. Screen change and leave-title must not leak. Check BMP decode buffers, PVR textures, script file buffers, AICA handles. Compare to plan caps. | Leak findings or `mem: no new unpaired alloc` |
| **Performance reviewer** | 60 FPS target, 30 floor. Flag CPU blit, per-frame re-lex of DinkC, preload-the-world, RGBA8888 textures, extra GD-ROM seeks. Do **not** ask for a custom DinkC JIT. | Perf notes or `perf: no 60 Hz regress suspected` |
| **Flaws reviewer** | Logic and compatibility: 1-based sprites, hardness, talk range, SH-4 alignment, little-endian readers, silent no-op vs skipped script. | Defects or `flaws: none blocking` |

**Minimum bar to merge**

- **Orchestrator** named on the PR + Implementer + **at least two** of: spec, adversarial, memory, performance, flaws.
- **Adversarial** is required on anything that touches `src/` runtime (not required for docs-only).
- **Memory** is required if the PR allocates, uploads textures, loads files, or attaches scripts.
- **Performance** is required if the PR is on the per-frame path or I/O during play/title.
- Docs-only PRs: spec reviewer is enough.

One human or agent may not wear Implementer and Adversarial on the same PR, and may not wear Orchestrator and Adversarial on the same PR.

Land: only the orchestrator merges (or explicitly delegates merge on the PR: `merge-delegate: @who`).

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
6. Orchestrator updates `bar:` when required verdicts are in and threads are resolved. Then merge or delegate.

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

- `make host` — plan + AGENTS structural checks (and later host unit tests).
- `make dc` — ELF; requires `KOS_BASE` after Bite 0.2.
- `make emu` / `make run` — Flycast on `build/dinkcast.cdi` (or `.elf`). Name the emulator and image if you claim a visual check.
- Do not claim DC boot works unless you ran the ELF (hardware or emulator) and say which.
