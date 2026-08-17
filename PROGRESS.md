# Progress

Living log of what landed on `master`. The bite *definitions* stay in [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md). Update **this file in the same PR** as the work.

**Statuses:** `done` — on master and verified the way the bite requires (host and/or Flycast). `source` — code on master, not seen on DC/Flycast. `next` — first unfinished bite. `pending` — not started.

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
| 2026-08-16 | **1.2–3.4** probe, BMP, official title still (host preview) | [#5](https://github.com/benthepoet/dinkcast/pull/5) `9fbad28` |
| 2026-08-16 | `make docker-cdi` / toolchain docs | [#6](https://github.com/benthepoet/dinkcast/pull/6) |
| 2026-08-16 | **3.4 verified in Flycast** (real BIOS, official Splash.bmp) | requester screenshot; `c43f761` twiddle fix |
| 2026-08-16 | Dreamcast KOS skill + gotcha log | `.grok/skills/dreamcast-kos`, [docs/GOTCHAS.md](docs/GOTCHAS.md) |
| 2026-08-16 | Screen-to-screen delay targets (CD vs Flycast) | plan binding + GOTCHAS |
| 2026-08-16 | Feasibility snapshot + log | this file, § Feasibility |
| 2026-08-16 | Alignment pass (docs agree: V1 done, next 4.1) | README/AGENTS/plan/PROGRESS |
| 2026-08-17 | **4.1–4.2** Maple port-0 Start/A leave title; `pvr_mem_free` title tex | [#10](https://github.com/benthepoet/dinkcast/pull/10) |
| 2026-08-17 | **5.1–6.3** world/map parse + start-screen 96-quad atlas (V2) | [#11](https://github.com/benthepoet/dinkcast/pull/11) |
| 2026-08-17 | **V2 accepted** | requester Flycast + “we're all good” |
| 2026-08-17 | **6.4–8.4** evict, hard.dat, ini, dir.ff, idle sprite (V3) | [#13](https://github.com/benthepoet/dinkcast/pull/13) |
| 2026-08-17 | Idle white key: PT list + PT bins (black slab) | [#14](https://github.com/benthepoet/dinkcast/pull/14) |
| 2026-08-17 | **V3 accepted** | requester screenshot + merge #14 |
| 2026-08-17 | **7.2–9.3** hardness stamp + walk (V4) | walk branch / merge |
| 2026-08-17 | **8.6** draw editor sprites (plan + code) | [#16](https://github.com/benthepoet/dinkcast/pull/16) |
| 2026-08-17 | 8.6 upload after `pvr_init` (leave-title shutdown) | [#17](https://github.com/benthepoet/dinkcast/pull/17) |
| 2026-08-17 | Editor `vision` + type 2 (intact house, not fire layer) | [#18](https://github.com/benthepoet/dinkcast/pull/18) |
| 2026-08-17 | FreeDink `SET_SPRITE_INFO` + `hard==0` stamp | [#19](https://github.com/benthepoet/dinkcast/pull/19) |
| 2026-08-17 | Move = `get_hard` point + exclusive stamp | [#20](https://github.com/benthepoet/dinkcast/pull/20) |
| 2026-08-17 | FreeDink audit: `que` rank, diag `speed-speed/3` | [#21](https://github.com/benthepoet/dinkcast/pull/21) |
| 2026-08-17 | `que` is sort key only (not draw y) | this PR |

## Bites

| Bite | Title | Status | Notes |
|---|---|---|---|
| 0.1 | Repo skeleton | done | `make host` / `make dc` without KOS exits 2 |
| 0.2 | Color field 640×480 `#5A3A1A` | done | Brown boot / HUD if data missing |
| 1.1 | Path resolver | done | `src/fs.c`; `tools/test_fs_join` |
| 1.2 | Existence probe `dink.dat` | done | `dink_dat_size`; red screen if missing |
| 2.1 | BMP header (host) | done | 8-bit + 24-bit; `tests/test_bmp` |
| 2.2 | BMP on DC | done | same loader; title still on Flycast |
| 3.1 | Identify official title file | done | `tiles/Splash.bmp` (640×480 8-bit) |
| 3.2 | CPU RGB565 | done | `src/rgb565.c` |
| 3.3 | PVR upload | done | twiddled RGB565 (not NONTWIDDLED) |
| 3.4 | **Title quad (first screenshot)** | done | Flycast + real BIOS; `tiles/Splash.bmp` |
| 4.1–4.2 | Start / leave title | done | #10 |
| 5.1–5.4 | `dink.dat` / `map.dat` | done | #11 |
| 6.1–6.3 | Atlas + 96 quads | done | V2 accepted |
| 6.4 | Evict | source | `tiles_evict` |
| 7.1–7.3 | hard.dat + stamp | source | 7.4 overlay still off |
| 8.1–8.5 | ini + dir.ff + idle | done | V3 accepted |
| 8.6 | Draw editor sprites | source | SET_SPRITE_INFO + vision |
| 9.1–9.3 | Walk | done | V4; point `get_hard` this PR |
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
| Native `KOS_BASE` | optional; Docker image used for ELF/CDI |
| GitHub `gh pr merge` | Fine-grained PAT often **403** on `mergePullRequest`. Human merges in the UI. **Do not** squash-push `master`. |
| KallistiOS / `.cdi` | `make docker-cdi` works; Flycast needs real `dc_boot.bin` |
| Human / visual gates | V1–**V4 accepted**. Next picture gate is **V5 (13.2 say)**. |

When you complete a bite, add a row under **On master** and set the bite **Status**. Do not delete old rows.

## Feasibility (ongoing)

Judgment of **can this ship**, not a burn-down. Percents are not CI. Update the snapshot **and** append a log row when a visual gate lands, a class of risk dies or appears, or the human asks to reassess. Do not rewrite old log rows.

**Difficulty (what is hard):** hardware is easy; **DinkC coverage** is hard; disc seeks and VRAM eviction are daily craft; AICA/VMU/real GD-ROM still unproven.

### Current (2026-08-16)

| | | |
|---|---|---|
| **Overall** | **~85%** | Playable opening-hours campaign on retail 16/8/2 MB |
| **Next picture (V2 tiles)** | **~90%** | Same blit path as splash |
| **Hardest remaining** | DinkC Wave 1 + fibers | No `story/*.c` on DC yet |
| **Difficulty** | Medium project, long pole = scripts | Not a “DC is too weak” project |

| Slice | Confidence | Why |
|---|---|---|
| Title / CDI / Flycast + BIOS | ~98% | V1 accepted |
| Tiles + Dink idle (V2–V3) | ~90% | Same BMP → twiddled quad |
| Walk + hardness (V4) | ~85% | Data/input, not GPU |
| Talk/hit + opening-village DinkC | ~75–80% | Interpreter not started |
| Weapons / magic / inventory | ~70–75% | Scripts + eviction + I/O |
| Full campaign + every MIDI | ~55–65% | Long tail |
| Real hardware / AICA / VMU | unknown | Not run |

### Log

| When | Overall | What moved it |
|---|---|---|
| 2026-08-16 (plan only) | ~80% campaign / ~95% title | Paper: DC can do 2D Dink; DinkC is the pole |
| 2026-08-16 (V1 accepted) | **~85%** / title **~98%** | Splash on Flycast+BIOS; Docker CDI works; ISO/twiddle/REIOS now in GOTCHAS. DinkC still untouched. |
