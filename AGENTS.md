# AGENTS.md — Dinkcast

Instructions for humans and agents working in this repo.

**Port spec:** [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md) is the source of truth for bites, budgets, DinkC, 60 FPS, and legal/data rules. Do not fork those decisions here. If implementation and the plan disagree, change the plan in the same PR or follow the plan.

**Dreamcast skill:** [.grok/skills/dreamcast-kos/SKILL.md](.grok/skills/dreamcast-kos/SKILL.md). Learned failures live in [docs/GOTCHAS.md](docs/GOTCHAS.md) — read before FS/CDI/PVR/Docker work; append a class-of-mistake bullet when we learn one.

**Progress log:** [PROGRESS.md](PROGRESS.md) is what has landed **and** the living **feasibility** snapshot. Every bite/fix PR must update the bite table in the **same PR**. After a **visual gate**, a retired/new risk class, or a human “reassess,” update **Feasibility** (current table + a new log row). Do not treat a green `make host` as “tracked.”

**Product:** Port Dink Smallwood to the Sega Dreamcast (KallistiOS). Original game data is required and is **not** committed unless a file’s license allows it. Use `DINK_DATA`.

**Now:** V1–**V5** + **8.6 house** accepted. This PR: Makefile.dc mapscr.h deps so script.o matches EditorSprite. Do not start 14.3 unless the requester says go. Do not `@`-mention anyone.

**Human gate (every merge):** After a PR is **merged** to `master`, **stop**. Do not open the next bite or start more engine work until the human requester explicitly approves. Reviews and fixes on an *open* PR may continue.

**Visual milestone gate (stronger):** These bites are **showable**. After the PR that first delivers one lands, **do not start the following bite** until the human has **seen it in Flycast or on hardware** and said it is accepted. A host unit test is not enough.

| Gate | Bite | What they must see |
|---|---|---|
| V1 | **3.4** | Official splash (`tiles/Splash.bmp`) | **accepted** |
| V2 | **6.3** | One official screen of tiles |
| V3 | **8.4** | Dink idle sprite on that screen |
| V4 | **9.3** | Walk cycle + hardness |
| V5 | **13.2** | `say` / talk box with real script text | **accepted** |
| V6 | **16.2** / **16.3** | Inventory and HUD |

Post `visual-gate: waiting (V#)` on the PR after merge. **Never `@`-mention a GitHub user** (including the requester). Clear the gate only with `visual-gate: accepted V#` from the requester (or a comment that unambiguously accepts that picture).

On the PR write `orchestrator:` as a label, not an `@` ping.

---

## Git workflow

- **Primary branch:** `master`. It must stay buildable (`make host`; `make docker-cdi` when Docker is up).
- **No direct work on `master`.** Every change is a **feature branch + pull request**.
- Branch names: `bite/3.4-title-quad`, `fix/vram-leak-screen-swap`, `chore/makefile-host`. One concern per branch.
- Rebase on `master` before requesting review. No merge commits on feature branches unless the PR is explicitly integrating a stack.
- Commits: imperative, scoped (`dinkc: yield on say_stop`). Do not mix a formatter sweep with a parser change.
- Land only via **GitHub PR merge** to `master` after the review bar is green (`gh pr merge` or the Merge button). **Do not** squash, cherry-pick, or fast-forward onto `master` locally and push to “close” a PR.
- If `gh pr merge` returns **403** (`mergePullRequest` / “Resource not accessible by personal access token”), **stop**. Comment on the PR that the human must merge in the UI. Do not push `master`.
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

**On the PR the orchestrator writes:** `orchestrator:` plus bite id and required reviews (no `@` username). Later `bar: green` / `bar: blocked (why)` before merge.

**Default orchestrator is a dedicated subagent**, not the human requester and not the implementer. Spawn it for every PR. The human may override in writing on that PR. One agent may Orchestrate PR A and Implement PR B, not both on the same PR (docs-only exception above).

| Role | Job | Must write on the PR |
|---|---|---|
| **Orchestrator** | Sequence work, assign reviews, enforce the bar, merge. | `orchestrator:` assignment + `bar:` |
| **Implementer** | Code the bite. Follow the plan. Host tests first when the plan says so. | Description, test steps, budget notes |
| **Spec reviewer** | Diff vs [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md): visual gates, original data formats, no new DinkC dialect, 60 Hz, FreeDink graft, GOTCHAS. | Approve or list plan violations |
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

### Troubleshooting team (when something is wrong on screen or disc)

When Flycast/hardware misbehaves (red HUD, stripes, no boot, missing files, wrong picture), **do not** have one agent guess. Spawn a **debug orchestrator** (not the implementer who last touched the bug, not **Adversarial-review** on a PR they authored) plus **at least two** of these specialists. They work in **parallel**. Findings go on the **open PR** or a new `fix/…` PR — same as review comments (`block` / `should` / `nit` + `verdict:`).

| Role | Looks at | Must write |
|---|---|---|
| **Debug orchestrator** | Assigns the other roles, synthesizes, picks the first fix. Does not implement the first theory. | `debug-orch:` + assigned roles + `bar:` |
| **Disc/FS** | `mkdcdisc -d` basename, `/cd` listing, ISO 8.3, `dink.dat` root, GD-ROM wait | What `/cd` contains vs what we probe |
| **PVR/video** | Twiddle vs `NONTWIDDLED`, POT pad/UVs, `vid_set_mode`, brown vs stripe vs red | Format/load vs draw mismatch |
| **Boot/emu** | REIOS vs `dc_boot.bin`, IP.BIN / `1ST_READ.BIN`, Flycast log ≠ SCIF | Whether the ELF even ran |
| **Data/format** | BMP bpp/bottom-up, `title_path.h`, map/ini once those exist | File identity and decode |
| **Adversarial debug** | The first diagnosis is a lie. Alternate causes from [docs/GOTCHAS.md](docs/GOTCHAS.md). | Attack list on the *theory* |

Read GOTCHAS and the HUD **before** proposing a patch. After a confirmed new class of failure, add one bullet to GOTCHAS in the fix PR.

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

- Plan bites in order. **3.4 is done.** Do not start past a **visual gate** without human accept.
- Graft FreeDink DinkC; do not invent a language or a “faster” interpreter.
- **Do not guess.** Sprite centers, hardboxes, vision, `hard==0`, and move tests come from FreeDink source. If you cannot name the FreeDink function, stop and look it up. Inventing a box or filter is a bug.
- Little-endian explicit reads; no packed-struct `fread` on SH-4 without a size lock.
- PVR textured quads only — no SDL-style CPU blit as the renderer.
- Log unimplemented DinkC; never skip a script file silently.
- Keep game assets out of git unless license-cleared.
- New source files: SPDX `GPL-3.0-or-later` (see [LICENSE](LICENSE)). Do not add MIT/Apache-only code that cannot be combined with a FreeDink graft.

---

## Checks

- `make host` — plan + AGENTS structural checks (and later host unit tests).
- `make docker-cdi` — ELF + CDI (preferred). Native `make dc` if `KOS_BASE` is set.
- `make emu` — Flycast + real BIOS on `build/dinkcast.cdi`. Name BIOS vs REIOS if you claim a visual check.
- Do not claim DC boot works unless you ran the ELF (hardware or emulator) and say which.
