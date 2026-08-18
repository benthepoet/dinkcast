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
| 2026-08-17 | `que` is sort key only (not draw y) | [#22](https://github.com/benthepoet/dinkcast/pull/22) |
| 2026-08-17 | SH-4: map/hard/edraw off the thread stack | [#23](https://github.com/benthepoet/dinkcast/pull/23) |
| 2026-08-17 | EditorSprite 4-aligned + heap snapshot | [#24](https://github.com/benthepoet/dinkcast/pull/24) |
| 2026-08-17 | **8.6 house accepted** (Flycast, unique 17) | requester “House shot is correct” |
| 2026-08-17 | Campaign systems table (stock 1.08, no D-Mods) | [#26](https://github.com/benthepoet/dinkcast/pull/26) |
| 2026-08-17 | Audio (12) after combat/inventory (16) | [#27](https://github.com/benthepoet/dinkcast/pull/27) |
| 2026-08-17 | Idle snap 1/3→2, 7/9→8 (`human_brain`) | #28 |
| 2026-08-17 | **10.1** talk probe (`run_through_tag_list_talk`) | #29; requester accepted |
| 2026-08-17 | **10.2** hit probe (`run_through_tag_list`) | #30 |
| 2026-08-17 | Punch→idle ghost: `pvr_wait_ready` before evict | #31; requester accepted |
| 2026-08-17 | **10.3** script hook stubs (log only) | #32 |
| 2026-08-17 | **11.0** DinkC `story/NAME.c` 32 KB load | #33 |
| 2026-08-17 | Preload unique sprite scripts (empty screen script) | #34 |
| 2026-08-17 | **11.1** DinkC lexer | #35 |
| 2026-08-17 | **11.2** DinkC parse stock `story/*.c` | #36 |
| 2026-08-17 | **11.3** DinkC fiber VM (`wait` yield) | #37 |
| 2026-08-18 | **11.4** DinkC vars (`attach` + MAIN.c) | #38 |
| 2026-08-18 | **11.5** wave-1 commands; A runs `talk()` | #39 |
| 2026-08-18 | **11.6** attach screen + sprite `main()` | #40 |
| 2026-08-18 | **11.7** choice lines + items prelude | #41 |
| 2026-08-18 | `make emu` tees SCIF to `build/emu.log` | #42 |
| 2026-08-18 | **11.8** wave-3 cmds (midi/status stubs) | #43 |
| 2026-08-18 | **11.9** DinkC command table + dump | #44 |
| 2026-08-18 | **13.1** embedded VGA 8×8 font atlas | #45 |
| 2026-08-18 | **13.2** say box (sprite-anchored) | #46 |
| 2026-08-18 | Freeze leftover after last `say_stop` | #47 |
| 2026-08-18 | **V5 accepted** (say box in Flycast) | requester “Looks good” |
| 2026-08-18 | **13.3** choice menu D-pad + A | #48 |
| 2026-08-18 | Talk lock: kill sprite fibers; wait clock | #49 |
| 2026-08-18 | **14.1–14.2** edge + warp swap | #50 |
| 2026-08-18 | Keep `hard.dat` only during stamp | #51 |
| 2026-08-18 | Snapshot sprites after map load | #52 |
| 2026-08-18 | edraw copy sprites before memset | this PR |

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
| 8.6 | Draw editor sprites | done | Flycast house accepted |
| 9.1–9.3 | Walk | done | V4; point `get_hard` this PR |
| 10.1 | Talk probe | done | #29; requester accepted |
| 10.2 | Hit probe | done | #30; punch ghost #31 |
| 10.3 | Hook table stubs | done | #32 |
| 11.0 | DinkC files on disc | source | preload unique sprite `.c`; start screen script empty |
| 11.1 | DinkC lexer | source | `//` comments; hyphen `&name` |
| 11.2 | DinkC parser | source | 0 fail on 381 stock `story/*.c` |
| 11.3 | DinkC VM yield | source | max 20; `wait` / say_stop / choice; no attach |
| 11.4 | DinkC variables | source | 1.08 local-then-global; MAIN.c list |
| 11.5 | Wave 1 commands | source | serial `say`; A = talk(); #39 |
| 11.6 | Attach on enter | source | screen MAIN then type-1 `main()` rank; #40 |
| 11.7 | Wave 2 choices + items | source | numbered lines; `&result`; cmd stubs; #41 |
| 11.8 | Wave 3 combat/magic/map | source | `playmidi`/`draw_status` stub; #43 |
| 11.9 | Coverage log | source | `k_fn[]`; `DINKC_DUMP_FNS=1` |
| 12.1–12.4 | AICA audio | pending | **after 16**; `playsound` stub until then |
| 13.1 | Font atlas | source | 128×64 ARGB1555 16 KB; #45 |
| 13.2 | Say box | source | `say_text` x-75 y-100 wrap 150; A/B |
| 13.3 | Choice menu | source | D-pad highlight; A → official `&result` |
| 14.1–14.2 | Edge + warp swap | source | no fade; `loc==0` clamp |
| 14.3 | Leak check 20 crossings | pending | |
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
| Human / visual gates | V1–**V5 accepted**. **8.6 house accepted**. Next picture gate **V6 (inv/HUD)**. |

When you complete a bite, add a row under **On master** and set the bite **Status**. Do not delete old rows.

## Feasibility (ongoing)

Judgment of **can this ship**, not a burn-down. Percents are not CI. Update the snapshot **and** append a log row when a visual gate lands, a class of risk dies or appears, or the human asks to reassess. Do not rewrite old log rows.

**Difficulty (what is hard):** hardware is easy; **DinkC coverage** is hard; disc seeks and VRAM eviction are daily craft; AICA/VMU/real GD-ROM still unproven.

### Current (2026-08-18)

| | | |
|---|---|---|
| **Overall** | **~88%** | Opening house + DinkC + on-screen say |
| **Next picture (V6 inv/HUD)** | **~40%** | After 14–15 |
| **Hardest remaining** | Screen change + brains | 14 / 15 |
| **Difficulty** | Medium project, long pole = scripts | Not a “DC is too weak” project |

| Slice | Confidence | Why |
|---|---|---|
| Title / CDI / Flycast + BIOS | ~98% | V1 accepted |
| Tiles + Dink idle (V2–V3) | ~98% | Accepted |
| Walk + hardness (V4) | ~95% | Accepted |
| Say box (V5) | ~95% | Accepted |
| Start-house sprites (8.6) | ~95% | Flycast accepted; SH-4 stack/align in GOTCHAS |
| Talk/hit + opening-village DinkC | ~75–80% | Interpreter not started |
| Weapons / magic / inventory | ~70–75% | Scripts + eviction + I/O |
| Full campaign + every MIDI | ~55–65% | Long tail |
| Real hardware / AICA / VMU | unknown | Not run |

### Log

| When | Overall | What moved it |
|---|---|---|
| 2026-08-16 (plan only) | ~80% campaign / ~95% title | Paper: DC can do 2D Dink; DinkC is the pole |
| 2026-08-16 (V1 accepted) | **~85%** / title **~98%** | Splash on Flycast+BIOS; Docker CDI works; ISO/twiddle/REIOS now in GOTCHAS. DinkC still untouched. |
| 2026-08-17 (8.6 house) | **~85%** / house **~95%** | Official start interior on Flycast. Stack smash + sprite align in GOTCHAS. DinkC still the pole. |
| 2026-08-18 (V5 say) | **~88%** / say **~95%** | On-screen `say_stop` + freeze thaw. Next picture V6. |
