# Progress

Living log of what landed on `master`. The bite *definitions* stay in [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md). Update **this file in the same PR** as the work.

**Statuses:** `done` — on master and host-checked (or DC-checked if the bite requires it). `source` — code on master; DC/Flycast not verified (no `KOS_BASE` yet). `next` — first unfinished bite. `open` — PR not merged. `pending` — not started.

## On master

| When | What | Evidence |
|---|---|---|
| 2026-08-16 | Repo bootstrap: plan, AGENTS, GPL-3.0-or-later | `343c74d` |
| 2026-08-16 | `make host`, PR template, Flycast `make emu`, `DINK_DATA` check | `bcb6649`…`41c40ee` |
| 2026-08-16 | Controller-only play | `1ec3bb7` |
| 2026-08-16 | **0.1** skeleton + **0.2** color-field source | [#1](https://github.com/benthepoet/dinkcast/pull/1) `875ffdd` |
| 2026-08-16 | Orchestrator is a dedicated subagent | [#2](https://github.com/benthepoet/dinkcast/pull/2) `ecf0a2d` |
| 2026-08-16 | **1.1** path resolver (case + 8.3, reject `..`) | [#3](https://github.com/benthepoet/dinkcast/pull/3) `c6610f0` |
| 2026-08-16 | **PROGRESS.md** work log | [#4](https://github.com/benthepoet/dinkcast/pull/4) `dcb58a8` |
| 2026-08-16 | **1.2–3.4** probe, BMP, official title still (host preview) | this PR |

## Bites

| Bite | Title | Status | Notes |
|---|---|---|---|
| 0.1 | Repo skeleton | done | `make host` / `make dc` without KOS exits 2 |
| 0.2 | Color field 640×480 `#5A3A1A` | source | Still used if data missing |
| 1.1 | Path resolver | done | `src/fs.c`; `tools/test_fs_join` |
| 1.2 | Existence probe `dink.dat` | done | `dink_dat_size`; red screen if missing |
| 2.1 | BMP header (host) | done | 8-bit + 24-bit; `tests/test_bmp` |
| 2.2 | BMP on DC | source | same loader; DC uses it for title |
| 3.1 | Identify official title file | done | `tiles/Splash.bmp` (640×480 8-bit) |
| 3.2 | CPU RGB565 | done | `src/rgb565.c` |
| 3.3 | PVR upload | source | `title_present_pvr` (needs KOS) |
| 3.4 | **Title quad (first screenshot)** | source | Host: `make title-preview` → `build/title_preview.ppm` |
| 4.1–4.2 | Start / leave title | next | |
| 5.1–5.4 | `dink.dat` / `map.dat` | pending | |
| 6.1–6.4 | Tiles + evict | pending | |
| 7.1–7.4 | Hardness | pending | |
| 8.1–8.5 | Sequences / Dink sprite | pending | |
| 9.1–9.3 | Walk | pending | |
| 10.1–10.3 | Talk / hit hooks | pending | |
| 11.0–11.9 | DinkC (FreeDink interpreter) | pending | |
| 12.1–12.4 | AICA audio | pending | |
| 13.1–13.3 | Text / choices | pending | |
| 14.1–14.3 | Screen transitions | pending | |
| 15.1–15.4 | Combat | pending | |
| 16.1–16.3 | Inventory / HUD | pending | |
| 17.1–17.3 | VMU save | pending | |
| 18.1–18.3 | Perf / disc / 240p | pending | |

## Blocked / outside bites

| Item | Status |
|---|---|
| `KOS_BASE` / dc-chain on a builder | missing — Flycast title not run here |
| GitHub `mergePullRequest` on PAT | 403 — land via local squash + push |
| Human gate | stop after **3.4** for requester screenshot review |

When you complete a bite, add a row under **On master** and set the bite **Status**. Do not delete old rows.
