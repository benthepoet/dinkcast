# Progress

Living log of what landed on `master`. The bite *definitions* stay in [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md). Update **this file in the same PR** as the work.

**Statuses:** `done` — on master and verified the way the bite requires (host and/or Flycast). `source` — code on master, not seen on DC/Flycast. `next` — first unfinished bite. `pending` — not started.

## Releases

Git tags (`vMAJOR.MINOR.PATCH`) are product versions. Bite **0.1** is the repo skeleton, not a release.

| Tag | When | What |
|---|---|---|
| [v0.1.0](https://github.com/benthepoet/dinkcast/releases/tag/v0.1.0) | 2026-08-22 | First tagged snapshot. V1–V6 + 8.6 house. Village playable in Flycast. #98. |
| [v0.2.0](https://github.com/benthepoet/dinkcast/releases/tag/v0.2.0) | 2026-08-22 | Campaign DinkC host #102–#108 + playtest #109. Flycast Done-when still Open. |
| [v0.3.0](https://github.com/benthepoet/dinkcast/releases/tag/v0.3.0) | 2026-08-23 | START + VMU **17** + playtest #117. |
| [v0.4.0](https://github.com/benthepoet/dinkcast/releases/tag/v0.4.0) | 2026-08-30 | AICA **12.1–12.4** SFX + streamed MIDI. |

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
| 2026-08-21 | Spec **14.4** residency + gated **14.5** distill; 14.3 after 14.4 | #79 |
| 2026-08-21 | One Adversarial reviewer (Dreamcast); drop spec/mem/perf/flaws agents | #80 |
| 2026-08-21 | **14.4a** host catalog + `mem_log`; size-pin unchanged; 164 drop stays | #81 |
| 2026-08-21 | **14.4b** Always/Screen/Prev; retire size-pin; sticky 164 pack drop | #82 |
| 2026-08-21 | Reopen-hang never confirmed; treat as KOS #1492 sector-pad | #83 |
| 2026-08-21 | **14.5** subset dir.ff of used 8-bit BMPs; CDI/DINK_DISTILL overlay | #84 |
| 2026-08-21 | Distill warp interiors (old-man map 3 missing innwall frames) | #85 |
| 2026-08-21 | Distill used frames from every nonempty campaign screen | #86 |
| 2026-08-21 | Defer per-frame `dir.ff` reads to **14.6** (after 16 / full campaign) | #87 |
| 2026-08-21 | Duck death seqs 117/123 before people walks (Ethel house 96-slot) | #88 |
| 2026-08-21 | Name **14.4c** pixel class eviction (not seq-id `EdGfx` victims) | #89 |
| 2026-08-21 | **14.4c** Always/Screen/Sticky `EdGfx` victims; `cpu_pixels` cap | #90 |
| 2026-08-21 | **14.3** 20-crossing leak check; `swap_ms` / `vram_free` | #91 |
| 2026-08-21 | **15.3–15.4** weapons / magic / item USE | #92 |
| 2026-08-21 | **16.1** touch / pickup | #93 |
| 2026-08-21 | **16.2** inventory grid | #94 |
| 2026-08-21 | **16.3** HUD | #95 |
| 2026-08-21 | Pickup shrink / blood / barrel smash / pig-pen enter | #96 |
| 2026-08-21 | Occupancy: smash pack open, sticky 164, `EdGfx` 128 | #97 |
| 2026-08-21 | Playtest picture tracker; pig blood/HP confirmed | #98 |
| 2026-08-21 | Grain USE: distill follows `add_item` (seq 430/431) | #98 |
| 2026-08-21 | Grain toss: upload PVR after play-path `preload_seq` | #98 |
| 2026-08-21 | Pig pen: drop aged Prev before Screen fopen | #98 |
| 2026-08-21 | Grain ARM walk: cx/cy even if hardbox omitted | #98 |
| 2026-08-21 | Pig kill: brain 7 hides editor snap | #98 |
| 2026-08-21 | preload_seq: frame 1, not whole walk | #98 |
| 2026-08-22 | Playtest stability: 409 slurp, smash bake/hard, create_sprite `hard=1` | #98 |
| 2026-08-22 | **v0.1.0** tagged | #99 |
| 2026-08-22 | `say()` TTL + `edraw_mark_need` (callers were in #98) | #100 |
| 2026-08-22 | Campaign graft audit; village PLAYTEST Open empty | #101 |
| 2026-08-22 | VM `goto` / labels (`locate_goto`) | #102 |
| 2026-08-22 | Keep generated canvases in `docs/canvases/` (v0.2 + campaign audit) | #103 |
| 2026-08-22 | DinkC `spawn` (`dc_spawn`, sprite 1000, no parent yield) | #104 |
| 2026-08-22 | DinkC `load_screen` + `draw_screen` (`game_load_screen` / `draw_screen_game`) | #105 |
| 2026-08-22 | DinkC `screenlock` + `get_hard` clamp | #106 |
| 2026-08-22 | `sp_follow` / `sp_target` on `BrainSpr` (`process_follow` / `process_target`) | #107 |
| 2026-08-22 | DinkC `get_sprite_with_this_brain` (+ rand / next) | #108 |
| 2026-08-22 | Playtest HUD: LEFTALIGN / `blitNoColorKey` digits + chrome | #109 |
| 2026-08-22 | Playtest HUD: level 442 loose BMP white key | #109 |
| 2026-08-22 | Playtest wizard: Screen live is this-tick draw set | #109 |
| 2026-08-22 | Playtest AlkNuts: `free_items` counts empty slots | #109 |
| 2026-08-22 | Playtest: burning-house exit logged (no patch) | #109 |
| 2026-08-22 | **v0.2.0** tagged | #109 |
| 2026-08-22 | **v0.3.0** plan (START menu + VMU **17** before **12**) | this PR |
| 2026-08-22 | START hover seq 199/200/198 (`live_sprite_animate` + noclip) | this PR |
| 2026-08-22 | START: no Quit; buttons stay pframe 1 | this PR |
| 2026-08-22 | START: pframe 2 glow, idle word stays put | this PR |
| 2026-08-22 | START Continue: choice overlay + 10 slots + Nevermind | this PR |
| 2026-08-22 | start-2 empty slot `Say_xy` (0, 390) | this PR |
| 2026-08-22 | editor slot 1 `sp_*` is not Dink (408 bar-sh) | this PR |
| 2026-08-23 | Tilesheet slurp drops Prev `dir.ff` (376 path after 408) | #114 |
| 2026-08-23 | Playtest: 376 pathway after Load confirmed | this PR |
| 2026-08-22 | Playtest: 408 bar-sh smash is barrel-only | this PR |
| 2026-08-23 | `s1-h1-s` fire exit: `get_hard_play` + `special_block` while frozen | #116 |
| 2026-08-23 | `force_vision` is sprite 1000 + `fill_whole_hard` + `draw_screen_game` (11.8) | #116 |
| 2026-08-23 | 14.4c: remake Screen live before `create_sprite` frame-1 | #116 |
| 2026-08-23 | Playtest: burning-house exit + 439 crowd confirmed | #116 |
| 2026-08-23 | Playtest: no type 0/1 hardness without pixels (pie table) | this PR |
| 2026-08-23 | `draw_hard_map` + type 1 hardness is live-only (S1-H1-4 table/beds) | this PR |
| 2026-08-23 | `make emu-fast` / `EMU_SERIAL=0` disables Flycast SCIF | this PR |
| 2026-08-23 | Indoor fire hitch with SCIF off; come back later | this PR |
| 2026-08-23 | 14.4c: brain-6 current+next (not nfr / not seq 161 pin) | this PR |
| 2026-08-23 | 14.4c: drop Screen CPU after PVR (fire loop cache) | this PR |
| 2026-08-23 | Playtest: indoor fire hitch confirmed | this PR |
| 2026-08-23 | 14.4b: create_sprite marks before editor unique (257) | this PR |
| 2026-08-23 | Playtest: `fade_down` / `fade_up` truecolor clock (11.8) | this PR |
| 2026-08-23 | Playtest: S1-H1-O fade confirmed | this PR |
| 2026-08-23 | Playtest: burned-house hardness + Libby confirmed | this PR |
| 2026-08-23 | Choice Down does not walk (`player_walk_pad`; SAVEBOT slots) | this PR |
| 2026-08-23 | `edraw upload fail` after CPU drop is a second pass, not a miss | this PR |
| 2026-08-23 | Start pause Continue/Title uses the seq 30 choice overlay | this PR |
| 2026-08-23 | `draw_screen` yield=3 uses caller sprite, not last nested bind | this PR |
| 2026-08-23 | Playtest: letter fade_up onto 439 confirmed | this PR |
| 2026-08-23 | `sp_disabled` skip draw (`spr.disabled`; Terris bridge dummy) | this PR |
| 2026-08-23 | Say after HUD chrome (`text_draw` after background status) | this PR |
| 2026-08-23 | **v0.3.0** tagged | this PR |
| 2026-08-23 | Campaign audit remainder: frame_delay, count/kill item, say_stop_xy, last_map, editor 6/7/8 timers | this PR |
| 2026-08-23 | `player_init` zeros `frame_delay` (seq delay until MAIN.c) | this PR |
| 2026-08-23 | `sp_nodraw` skip draw (`s1-nopas` lean-to dummy) | this PR |
| 2026-08-23 | repeat_brain: create_sprite food not seq-animated (BAR-F1) | this PR |
| 2026-08-23 | ini hardbox: right and bottom independent (421 food touch) | this PR |
| 2026-08-23 | map.dat +100/+104/+108 `base_idle`/`base_attack`/`base_hit`; `brains_enter` copies (`game_place_sprites`) | this PR |
| 2026-08-23 | `create_sprite` `load_frame` (S1-MH-M seq 56 bottle; play-path ensure skips) | this PR |
| 2026-08-23 | Cave unique: combat type-1 before type-0; pill/dragon all 8 dirs + `base_attack`; distill `external` | this PR |
| 2026-08-23 | Unique `base_die` dirs (Bonca corpse 551/559; `add_kill_sprite` is not DinkC create) | this PR |
| 2026-08-23 | NPC `preload_seq` is Screen, not Always (s1-rob vis 1 knights) | this PR |
| 2026-08-23 | missile skips Dink when `brain_parm==1` (ITEM-FB fireball) | this PR |
| 2026-08-23 | Always `graphics/dink/die/` seq 436 (`DINFO.c` death pose) | this PR |
| 2026-08-23 | missile `DAMAGE` (`DAM-FIRE` seq 70 brain 7); `sp_seq` `load_frame` | this PR |
| 2026-08-23 | ARM `preload_seq` Always only for named prefixes (AlkNut west ts35 / seq 421) | this PR |
| 2026-08-23 | `make_room` drops Screen `dir.ff` after Prev (live burned tree seq 20) | this PR |
| 2026-08-30 | `S1-HOLE` crawl: hold last `pseq` while frozen; `sp_seq(1)` is Dink on editor-1 hole | this PR |
| 2026-08-30 | `load_sequence` numeric delay reuses frame 1 origin (crawl 452 vs crack) | this PR |
| 2026-08-30 | `Makefile.dc` `$(OBJS)` depends on `ini.h`/`player.h` (`SeqInfo` stride; splash→black) | this PR |
| 2026-08-30 | Place loop editor `sound` (hearth 23); warp default OPEN bank 7 | this PR |
| 2026-08-30 | Halt hearth loop on START; place again after title pick | this PR |
| 2026-08-30 | **12.4** `snd_stream` MIDI; START `1003.mid`; mom `dance.mid`; pump on `fread_all` | this PR |
| 2026-08-30 | `sp_speed`/`sp_dir` `changedir` (Chealse hit zip) | this PR |
| 2026-08-30 | **v0.4.0** tagged | this PR |
| 2026-08-30 | `player_step` animates `seq>0` while frozen (`DINFO` die 436) | this PR |

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
| 11.3 | DinkC VM yield | source | max 20; `wait` / say_stop / choice; `locate_goto` #102 |
| 11.4 | DinkC variables | source | 1.08 local-then-global; MAIN.c list |
| 11.5 | Wave 1 commands | source | serial `say`; A = talk(); #39. Live sprite cmds leftover → **11.10** |
| 11.6 | Attach on enter | source | screen MAIN then type-1 `main()` rank; #40. Script field 30240 this PR |
| 11.7 | Wave 2 choices + items | source | numbered lines; `&result`; cmd stubs; #41 |
| 11.8 | Wave 3 combat/magic/map | source | `playmidi`/`draw_status` stub; #43. `force_vision` #116. `fade_*` truecolor clock (this PR). `fill_screen` still instant |
| 11.9 | Coverage log | source | `k_fn[]`; `DINKC_DUMP_FNS=1` |
| 11.10 | Wave 1 live sprite cmds | source | `move`/`create_sprite`/`sp_kill`/NPC `sp_*`; #66. `sp_disabled` skip draw (this PR) |
| 12.1 | WAV → AICA ADPCM | source | `tools/wav_to_adpcm`; PCM &lt; 8 KiB stays PCM; stage overlay; not committed |
| 12.2 | SFX bank | source | START.c `load_sound` table; ≤512 KB; skip missing freeware slots |
| 12.3 | `playsound` | source | DinkC bank=channel+1; status gold/exp; choice 11/17; missile 9 |
| 12.4 | music stream | source | `snd_stream` 256 KiB PCM ring; poll from RAM on `fread_all`/`load_one`; callback silence on underrun (bytes, not samples); START menu `1003.mid`; mom `dance.mid`; `music==0` keep; `make music-bank` |
| 13.1 | Font atlas | source | 128×64 ARGB1555 16 KB; #45 |
| 13.2 | Say box | source | `say_text` x-75 y-100 wrap 150; `print_text_wrap` hcenter; A/B; `text_brain` follow; `font_colors` 1–15 |
| 13.3 | Choice menu | source | D-pad + A; `&result` official #; seq 30 overlay + center + arrows this PR |
| 14.1–14.2 | Edge + warp swap | source | no fade; `loc==0` clamp; `parm_seq` wait #71 |
| 14.3 | Leak check 20 crossings | source | host ping-pong 439↔441; `file_blob`/`always`/`ts_rgb` ≤ 4 KB after warm; `swap_ms` (Flycast may be ~0). Hardware delay is ODE/burn |
| 14.4 | Residency catalog + policy | source | spec #79. **14.4a** #81. **14.4b** packs #82. **14.4c** pixels #90. Tilesheet slurp uses the same Prev `dir.ff` drop as packs (376 `ts41` after 408) |
| 14.4c | Pixel working set | source | Always/Screen/Sticky victims; `cpu_pixels` bytes not 96 slots. Host: Ethel 117/123 + full-table ensure. Enter-path remakes Screen live before `create_sprite` `load_frame` (439 crowd). Brain 6 is current+next (not `nfr`; house vis 1 161). DC Screen drops CPU after PVR so unused loop frames do not refuse. Flycast punch still the human check |
| 14.5 | Distill frames (gated) | source | campaign used-frame union on disc (#84–#86). Heavy-screen `file_blob` over cap until **14.6** |
| 14.6 | Per-frame `dir.ff` reads | pending | after **16** + requester full-campaign go. Enter-path TOC/offset; not next after 14.5 |
| 15.1 | Brains | source | `update_frame` switch; all 0–17 motion; #65 |
| 15.2 | Damage | source | #76. Duck first hit stays headless 110 + head 120 (#77). Seq 164 frames stay; magic pack dropped (#78). House duck death 117/123 before people walks (#88). Pixel victims Always/Screen/Sticky (**14.4c**) |
| 15.3–15.4 | Weapons / magic | source | `add_item`/`arm_weapon`/`arm_magic`; START-1 fists; B USE; X mana; `init` seq rewrite; sword/bow Always. Bow charge later |
| 16.1 | Touch / pickup | source | `run_through_touch_damage_list`; `s1-sack` `TOUCH` → `item-pig`; `editor_type` 1 on **re-enter**; live `scale_brain` |
| 16.2 | Inventory UI | source | `process_item`; Y toggle; seq 423 blit; A `arm_weapon`/`arm_magic`. HUD is 16.3. #94 |
| 16.3 | HUD | source | `draw_status_all` / `update_status_all`; digit atlas 128 KB; L map `button6` / seq 165. V6. LEFTALIGN / `blitNoColorKey` paper (this PR) |
| 17.1–17.3 | VMU save | source | Compact LE blob &lt; 8 KB; host `build/saveNN.bin`; Maple first VMU + 32×32 icon; `save_game` / `load_game` / `game_exist` / `&savegameinfo`. Soft-fail if no VMU. Flycast stamp pending |
| 18.1–18.3 | Perf / disc / 240p | pending | |

## Blocked / outside bites

| Item | Status |
|---|---|
| Native `KOS_BASE` | optional; Docker image used for ELF/CDI |
| GitHub `gh pr merge` | Fine-grained PAT often **403** on `mergePullRequest`. Human merges in the UI. **Do not** squash-push `master`. |
| KallistiOS / `.cdi` | `make docker-cdi` works; Flycast needs real `dc_boot.bin` |
| Human / visual gates | V1–**V6 accepted**. **8.6 house accepted**. |
| Playtest pictures | [docs/PLAYTEST.md](docs/PLAYTEST.md) — HUD + wizard + AlkNuts + 376 pathway after Load confirmed; burning-house exit Open |
| v0.3.0 | tagged 2026-08-23. START + VMU **17**. Next: requester go. |
| v0.4.0 | tagged 2026-08-30. Audio **12** in Flycast. **14.6** gated. |

When you complete a bite, add a row under **On master** and set the bite **Status**. Do not delete old rows.

## Feasibility (ongoing)

Judgment of **can this ship**, not a burn-down. Percents are not CI. Update the snapshot **and** append a log row when a visual gate lands, a class of risk dies or appears, or the human asks to reassess. Do not rewrite old log rows.

**Difficulty (what is hard):** hardware is easy; **DinkC coverage** is hard; disc seeks and VRAM eviction are daily craft; AICA/VMU/real GD-ROM still unproven.

### Current (2026-08-30)

| | | |
|---|---|---|
| **Overall** | **~92%** | v0.4.0: SFX + MIDI stream in Flycast; hardware/ODE still pending |
| **Next picture** | requester go | **14.6** gated |
| **Hardest remaining** | DinkC long tail | then 14.6 RAM |
| **Difficulty** | Medium project, long pole = scripts | Not a “DC is too weak” project |

| Slice | Confidence | Why |
|---|---|---|
| Title / CDI / Flycast + BIOS | ~98% | V1 accepted |
| Tiles + Dink idle (V2–V3) | ~98% | Accepted |
| Walk + hardness (V4) | ~95% | Accepted |
| Say box (V5) | ~95% | Accepted |
| Start-house sprites (8.6) | ~95% | Flycast accepted; SH-4 stack/align in GOTCHAS |
| Talk/hit + opening-village DinkC | ~75–80% | Interpreter live; coverage is the pole |
| Weapons / magic / inventory / HUD | ~95% | V6 accepted; bow still instant-100 |
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
| 2026-08-21 (14.4b policy) | **~90%** | Size-pin retired. Always named list + Screen + one Prev. Sticky 164 still drops the pack after frames complete. 14.4a already printed `14.5: needed`; this bite does not claim the opening-village `file_blob` peak is under 4.5 MB. |
| 2026-08-21 (reopen hang unconfirmed) | **~90%** | “Third `trees/dir.ff` reopen hang” was never confirmed. Hangs match KOS #1492 (non-2048 sizes); sector-pad retired that class in Flycast. 14.4b Prev is RAM, not hang insurance. 14.5 stays gated on catalog over-cap. |
| 2026-08-21 (14.5 distill) | **~90%** | Subset `dir.ff` of used 8-bit BMPs. Distilled village `file_blob` peak under 4.5 MB. `DINK_DATA` unchanged. Catalog without overlay still prints whole-pack `14.5: needed`. |
| 2026-08-21 (14.5 nframes holes) | **~90%** | Old-man house (map 3 via cabin warp) skipped innwalls/details/cup because distill only listed map 2. Warp BFS (#85) still village-bound. |
| 2026-08-21 (14.5 campaign distill) | **~90%** | Distill + catalog campaign scan use every nonempty `map.dat` screen (~644). Village-only used-frame union is not the disc policy. |
| 2026-08-21 (14.6 deferred) | **~90%** | Per-frame `dir.ff` TOC/offset reads wait until base (through 16) and a human full-campaign go. Not the next 14.5 patch. |
| 2026-08-21 (ethel duck full skip) | **~90%** | Killing the returned duck in Ethel’s house (map 2 vis 1) printed `edraw full skip` seq 117/123 every tick: oldman walk frames filled the 96-slot table before duck death. Combat 110/120 first; people walks frame 1 + `ensure_frame`. Seq-id `EdGfx` eviction (`110..129`, `>= 200`) is a stopgap — **14.4c**. |
| 2026-08-21 (14.4c named) | **~90%** | 14.4b is packs (Always/Screen/Prev). Pixel working set is Always/Screen/Sticky class eviction (not LRU, not Prev), `cpu_pixels` bytes not 96 slots — **14.4c**, before 14.3. Distill/14.6 do not skip it. |
| 2026-08-21 (14.4c source) | **~90%** | Play-path `ensure_frame` evicts Screen (prefer unused, cached pack), never Always/Sticky, never seq-id ranges. `mem refuse pool=cpu_pixels`. Created people walk frame 1 only. Host Ethel 117/123 + full-table ensure. |
| 2026-08-21 (14.3 source) | **~90%** | 20-crossing host ping-pong 439↔441: `file_blob` / Always / `ts_rgb` stable ≤ 4 KB after swap 4; `ff_disc_loads` does not climb. `swap_ms` + `vram_free` on each swap. Flycast delay may be ~0; hardware ODE/burn still the delay table. |
| 2026-08-21 (15.3 source) | **~90%** | Fists from START-1 `add_item`/`arm_weapon`; B runs item USE; sword ARM `init` rewrite + strength/range; fireball USE missile. Sword/bow packs Always. Bow hold-charge later. Next **16**. |
| 2026-08-21 (16.1 source) | **~90%** | Walk-into-sprite `TOUCH` (`s1-sack` → `item-pig`). `editor_type` 1 persists kill. Y inventory is **16.2**. |
| 2026-08-21 (16.2 source) | **~90%** | Y toggles `process_item` grid (seq 423). A arms `item-pig`. HUD is **16.3**. |
| 2026-08-21 (16.3 source) | **~90%** | Status bar from official BMPs (`draw_status_all`). Digit atlas 128 KB. Hold X fills `&magic_level`. L is `ACTION_MAP` / `button6.c`. V6 waiting on Flycast/hardware. |
| 2026-08-21 (V6 accepted) | **~90%** | Inventory + HUD accepted. Next picture column empty. DinkC coverage is the pole. |
| 2026-08-21 (playtest graft) | **~90%** | `editor_type` 1 is re-enter only (`scale_brain` shrink). `draw_damage` / `random_blood`. Barrel smash frames. Pig/pill/dragon walk **frame 1** on enter (`need_push` + post-decode). Blood 187–189 with combat pixels. `inside_box`. `g_spmap_seq` is int16 (~155 KB BSS). |
| 2026-08-21 (occupancy graft) | **~90%** | Enter-path opens `dir.ff` even when `EdGfx` is full. Sticky 164 not prepended on non-die screens. Slot bound 128 under `cpu_pixels`. |
| 2026-08-21 (playtest tracker) | **~90%** | Pig blood + hit numbers confirmed. Remaining Flycast issues tracked in PLAYTEST.md; host `test_playtest` locks the pig punch path. |
| 2026-08-21 (say TTL) | **~90%** | `say()` (not `say_stop`) expired via `add_text_sprite` `kill_ttl` (`max(strlen*77, 2700)`). Ethel hello / no-wizard were permanent because Dinkcast only cleared on A/B yield. Confirmed in Flycast. |
| 2026-08-22 (409 house) | **~90%** | First 409 visit skipped seq 63: slurp doubled to 1 MiB and sbrk-failed before Prev drop. `fstat` + `make_room` before malloc. |
| 2026-08-22 (barrel bg) | **~90%** | Brain 5 smash left type-1 last frame; Dink y-sorted under debris. Graft `one_time_brain` bake-to-background (`type` 0). |
| 2026-08-22 (fence painter) | **~90%** | Pig-pen seq 93 overlap shimmered: PT z-steps vs FreeDink blit. Shared world-sprite z; slot tie-break. Not a seq-93 pin. |
| 2026-08-22 (barrel hard) | **~90%** | Smash left seq-173 origin hardbox in the hitmap. Type 3 `update_play_changes` on restamp; HIT continues after finished `external`; bake copies `hard`. |
| 2026-08-22 (create hard) | **~90%** | BAR-SH leftover was seq-54 heart: `add_sprite` hard=1, create memset 0. `draw_hard_sprite` stamped it; TOUCH never restamps. Confirmed in Flycast. |
| 2026-08-22 (v0.1.0) | **~90%** | First product tag after #98. Village Flycast snapshot. Open: 409 house, smash y-sort, fence shimmer. Audio/VMU/hardware unproven. |
| 2026-08-22 (say TTL) | **~90%** | #98 called `saybox_tick` / `edraw_mark_need` without bodies. `say()` TTL + Screen mark for MAIN `create_sprite`. |
| 2026-08-22 (village Open) | **~90%** | Requester: last PLAYTEST Open pictures confirmed (409 house, smash y-sort, pig-pen fence). Village leftovers empty. |
| 2026-08-22 (HUD paper) | **~90%** | Status digits/chrome punched RGB>240 so white paper fell through PT to black. Graft LEFTALIGN / `blitNoColorKey`; host `test_status` locks opaque paper. |
| 2026-08-22 (HUD confirmed) | **~90%** | Requester: status paper + level 442 key look right in Flycast. |
| 2026-08-22 (wizard live) | **~90%** | Play-path Screen `live` until swap: 167 explode filled `cpu_pixels`, 563/567 refused. Remake live from this tick’s draw set. |
| 2026-08-22 (wizard confirmed) | **~90%** | Requester: gnome stayed visible for the `s1-wiz` meeting. refuse/evict at the cap is the policy. |
| 2026-08-22 (AlkNuts) | **~90%** | `s1-nut` `free_items()` was unimplemented (0) so pickup always said full. Graft `dc_free_items` slot count. |
| 2026-08-22 (AlkNuts confirmed) | **~90%** | Requester: “I picked up a nut!” in Flycast. |
| 2026-08-22 (fire house log) | **~90%** | Burning start-house fire/explo drew; log spam is 14.4c seq **161** ping-pong. Exit failed: `s1-h1-s` never `unfreeze`s; FreeDink warps while frozen (`get_hard_play` + `special_block`). Logged in PLAYTEST, no patch. |
| 2026-08-22 (v0.2.0) | **~90%** | Requester stamped campaign DinkC host slice. Flycast Done-when still Open. Next picture: burning-house exit. |
| 2026-08-22 (v0.3 plan) | **~90%** | Requester: title + VMU before remaining campaign issues. **17** before **12**. **14.6** still gated. |
| 2026-08-23 (v0.3.0) | **~90%** | Requester stamped START + VMU + playtest #117. Audio **12** / **14.6** still gated. |
| 2026-08-30 (v0.4.0) | **~92%** | Requester stamped AICA **12**. Title `1003.mid`, house `dance.mid`, edge pump. **14.6** gated. |
