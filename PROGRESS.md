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
| 2026-08-18 | edraw copy sprites before memset | #53 |
| 2026-08-18 | Restore sprites after edraw for scripts | #54 |
| 2026-08-18 | Heap sprite snap | #55 |
| 2026-08-18 | Heap EdGfx; restore before attach | #56 |
| 2026-08-18 | PVR clear after leave-title | #57 |
| 2026-08-18 | Makefile.dc header deps for EditorSprite | #58 |
| 2026-08-18 | Swap: keep tile PVR until new atlas | #59 |
| 2026-08-19 | No brown clear on screen swap | this PR |
| 2026-08-19 | Editor `alt` crop (`get_box`) — extra pig fences | this PR |
| 2026-08-19 | Pin Dink walk/idle `dir.ff` (re-enter house hang) | this PR |
| 2026-08-19 | Keep `map.dat` open; LRU `dir.ff`; reuse edraw | this PR |
| 2026-08-19 | Keep unused editor CPU; cache ts sheets | this PR |
| 2026-08-19 | CD settle after slurp; drop dir.ff after decode | this PR |
| 2026-08-19 | Do not count bow/bottles dir.ff at boot | this PR |
| 2026-08-19 | Decode all frames of a seq while dir.ff is open | this PR |
| 2026-08-19 | Pin large dir.ff for the session (no reopen) | this PR |
| 2026-08-19 | Warm ts02/ts03 at first atlas (house-door hang) | dropped; blob cache |
| 2026-08-19 | Free unused sprite pixels; keep atlas on OOM | this PR |
| 2026-08-19 | Pin idle+walk dir.ff before house edraw | this PR |
| 2026-08-19 | Session blob cache; disc_opens; live-only swap decode | this PR |
| 2026-08-19 | Blob cache grows; never evict borrowed dir.ff/hard.dat | this PR |
| 2026-08-19 | Tile atlas is BSS; ts sheet LRU re-decodes from blob | this PR |
| 2026-08-19 | `make emu` loads CHD (GDI from mkdcdisc ISO); CDI stays for hardware | this PR |
| 2026-08-19 | hard.dat FILE* + rec reads; no mid-file yield; /cd mutex | this PR |
| 2026-08-19 | `make chd-redream` 3-track MODE1/2352 CHD for Redream | this PR |
| 2026-08-19 | Sector-pad staged disc files + boot binary (KOS #1492 stream abort) | #61 |
| 2026-08-19 | First-open /cd hang root-caused: [CD-HANG-ROOTCAUSE.md](docs/CD-HANG-ROOTCAUSE.md) | #61 |
| 2026-08-19 | First-open /cd hang retired in Flycast (3 clean cold boots) | requester report |
| 2026-08-20 | Idle ping-pong: `SET_FRAME_FRAME` 5→3, 6→2 (no 4→1 snap) | this PR |
| 2026-08-20 | Drop `make chd-redream` / Redream CHD | master |
| 2026-08-20 | Defer **14.3** leak check until after 15.x | this PR |
| 2026-08-20 | **15.1** brains (`update_frame`; pigs/ducks/people/repeat + rest) | #65 |
| 2026-08-20 | Sequence **11.10** (wave-1 live sprite cmds) after 15.1, before 15.2 | #65 |
| 2026-08-20 | **11.10** live `move`/`create_sprite`/`sp_kill`/NPC `sp_*` | #66 |
| 2026-08-20 | Screen `script` at 30240; `s1-gate`/`findduck` + `&vision` place | #67 |
| 2026-08-20 | Say box follows owner (`text_brain`) | #68 |
| 2026-08-20 | Keep `create_sprite`; skip editor-active slots; NPC say follow | #69 |
| 2026-08-20 | Say `font_colors` 1–15 (`` `5 `` Chealse magenta, not Dink yellow) | #70 |
| 2026-08-20 | Warp `parm_seq` before swap (house door `odor1-` 61) | #71 |
| 2026-08-20 | Talk/magic miss say (`human_brain` 6+6 lines; X mapped) | #72 |
| 2026-08-20 | Choice overlay: seq 30 box, hcenter, one-page vertical center, arrows 456/457 | #73 |
| 2026-08-20 | Start-house VRAM snapshot + sprite_tex packing notes | #74 |
| 2026-08-20 | Say `print_text_wrap` hcenter in the 150 box | #75 |
| 2026-08-20 | **15.2** damage: `hurt_thing`, HIT/DIE, corpse, push, `dinfo` DIE | #76 |
| 2026-08-21 | Duck first punch keeps headless 110 + flying head 120 | #77 |
| 2026-08-21 | Drop `magic/dir.ff` after decoding seq 164; do not pin it on mom hp | #78 |
| 2026-08-21 | Spec **14.4** residency + gated **14.5** distill; 14.3 after 14.4 | this PR |

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
| 8.1–8.5 | ini + dir.ff + idle | done | V3 accepted; `SET_FRAME_FRAME` ping-pong this PR |
| 8.6 | Draw editor sprites | done | Flycast house accepted |
| 9.1–9.3 | Walk | done | V4; point `get_hard` this PR |
| 10.1 | Talk probe | done | #29; requester accepted; miss say this PR |
| 10.2 | Hit probe | done | #30; punch ghost #31 |
| 10.3 | Hook table stubs | done | #32 |
| 11.0 | DinkC files on disc | source | preload unique sprite `.c`; screen `script` at 30240 |
| 11.1 | DinkC lexer | source | `//` comments; hyphen `&name` |
| 11.2 | DinkC parser | source | 0 fail on 381 stock `story/*.c` |
| 11.3 | DinkC VM yield | source | max 20; `wait` / say_stop / choice; no attach |
| 11.4 | DinkC variables | source | 1.08 local-then-global; MAIN.c list |
| 11.5 | Wave 1 commands | source | serial `say`; A = talk(); #39. Live sprite cmds leftover → **11.10** |
| 11.6 | Attach on enter | source | screen MAIN then type-1 `main()` rank; #40. Script field 30240 this PR |
| 11.7 | Wave 2 choices + items | source | numbered lines; `&result`; cmd stubs; #41 |
| 11.8 | Wave 3 combat/magic/map | source | `playmidi`/`draw_status` stub; #43 |
| 11.9 | Coverage log | source | `k_fn[]`; `DINKC_DUMP_FNS=1` |
| 11.10 | Wave 1 live sprite cmds | source | `move`/`create_sprite`/`sp_kill`/NPC `sp_*`; #66. Skip active editor; keep created this PR |
| 12.1–12.4 | AICA audio | pending | **after 16**; `playsound` stub until then |
| 13.1 | Font atlas | source | 128×64 ARGB1555 16 KB; #45 |
| 13.2 | Say box | source | `say_text` x-75 y-100 wrap 150; `print_text_wrap` hcenter; A/B; `text_brain` follow; `font_colors` 1–15 |
| 13.3 | Choice menu | source | D-pad + A; `&result` official #; seq 30 overlay + center + arrows this PR |
| 14.1–14.2 | Edge + warp swap | source | no fade; `loc==0` clamp; `parm_seq` wait #71 |
| 14.3 | Leak check 20 crossings | pending | after **14.4** (unbounded pack pin made the delta meaningless) |
| 14.4 | Residency catalog + policy | pending | spec this PR. **14.4a** catalog+`mem_log`; **14.4b** one policy. Before 15.3 |
| 14.5 | Distill frames (gated) | pending | only if 14.4a prints `14.5: needed` |
| 15.1 | Brains | source | `update_frame` switch; all 0–17 motion; #65 |
| 15.2 | Damage | source | #76. Duck first hit stays headless 110 + head 120 (#77). Seq 164 frames stay; magic pack dropped (#78) |
| 15.3–15.4 | Weapons / magic | pending | after **14.4b** under cap or **14.5 done**; 15.2 already landed |
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

### Current (2026-08-20)

| | | |
|---|---|---|
| **Overall** | **~90%** | CD first-read hang class retired (KOS #1492 + sector padding) |
| **Next picture (V6 inv/HUD)** | **~40%** | After 15–16. **14.4 residency** before 14.3 leak check and 15.3 |
| **Hardest remaining** | Weapons / magic / inventory **and** main-RAM residency | **14.4** then 15.3 then 16 |
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
| 2026-08-19 (CD hang root cause) | **~88%** (unmoved) | First-open /cd hang = KOS fs_iso9660 stream abort on non-2048-multiple files (KOS #1492, hardware-confirmed there). Images now sector-padded at build; hang retirement pending Flycast/hardware confirmation. |
| 2026-08-19 (CD hang retired) | **~90%** | 3 consecutive clean cold boots after #61 sector padding (leave-title, house door, village). Random-lockup class gone from the dev loop; hardware/ODE confirmation still open. DinkC coverage back as the pole. |
| 2026-08-20 (defer 14.3) | **~90%** | Leak check after 20 crossings postponed until after 15.x (or 18.x). Sequence next: 15.1 brains. |
| 2026-08-20 (11.10 sequenced) | **~90%** | Wave-1 live sprite cmds (`move`/`create_sprite`/`sp_kill`/NPC `sp_*`) after 15.1, before 15.2. |
| 2026-08-20 (11.10 source) | **~90%** | Live `BrainSpr` cmds + `move_stop` yield. Next **15.2**. |
| 2026-08-20 (screen script) | **~90%** | `map.dat` screen `script` was 30204 (zeros). Guard/`findduck` need 30240 + `*pvision`. |
| 2026-08-20 (say follow) | **~90%** | `say` snapshot-once; FreeDink `text_brain` follows owner each frame (`FINDDUCK`). |
| 2026-08-20 (gate girl) | **~90%** | `create_sprite` during MAIN was memset by `brains_enter` and stole type-0 slots. `s1-lg` text without sprite. |
| 2026-08-20 (say colors) | **~90%** | Draw only mapped 13/4/15; `` `5 `` Chealse fell through to Dink yellow. Full FreeDink `font_colors` 1–15. |
| 2026-08-20 (door parm_seq) | **~90%** | Instant warp skipped type-1 `parm_seq` 61 (village house door). `special_block` wait + preload frames. |
| 2026-08-20 (miss say) | **~90%** | A miss and X with no magic were silence; `human_brain` 6+6 `say_text` lines. |
| 2026-08-20 (choice overlay) | **~90%** | Choice list was left-aligned `>` with inverted colors. FreeDink centers over seq 30 `main-` + arrows 456/457. |
| 2026-08-20 (VRAM occupancy) | **~90%** | Start house ~42% of 8 MB / 42% of `sprite_tex`. Choice overlay 696 KB pinned. Per-frame POT atlas is a wash; residency (choice-on-open, current walk facing) is the pack. 14.3 still deferred. See [docs/canvases/](docs/canvases/). |
| 2026-08-20 (15.2 source) | **~90%** | Fists `hurt_thing` / HIT / DIE / corpse / push / life 0 `dinfo`. `sp_strength` bound; missile `get_box`; seq 164 preload. Next **15.3** weapons. |
| 2026-08-21 (duck vanish) | **~90%** | First punch on Ethel's duck (`s1-oldd`) ran DIE then skipped seq 111/113/117/119 (`duck/death` pack not cached). Headless 110 + head 120 must preload; duck stays. |
| 2026-08-21 (die pack OOM) | **~90%** | After the duck kill, walking to the pig pen hit `Out of memory` 200704 (`swap atlas fail`) then seq 63 425984. House mom hp had pinned `magic/dir.ff` (~600 KB) for the session. Decode 164 into EdGfx and drop the pack; people hp is not a die preload. |
| 2026-08-21 (14.4 spec) | **~90%** | Size-pin ≥80 KB is ~31 MB of official packs. Spec: 14.4a catalog+`mem_log` with real village bytes (Always 1.20 MB, 439 1.51, duck ts_rgb ~1.17); 14.4b one policy; 14.5 only if catalog `needed`. 14.3 after 14.4. Choice overlay is VRAM after upload. |
