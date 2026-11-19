# Dinkcast codebase review — 2026-11-19

A review of `src/` (about 22.5 k lines of C) against GNU FreeDink (`~/Source/gnu_freedink/src`) and the stock 1.08 data (`~/Source/freedink-data-1.08.20190120/dink`). It covers bugs, gaps against FreeDink, capacity limits measured against the stock data, RAM/VRAM/perf, and architectural changes worth making.

**How this was checked.** Line numbers are from `fix/say-npc-punch` at `e21029e` (master plus the 2026-09-07 playtest fixes); on master some `main.c`/`dinkc_vm.c` lines sit a few rows earlier. I read the source and FreeDink side by side. I also ran Python scans over the stock `map.dat`, `dink.dat`, `Story/*.c` and every `dir.ff` to measure real data against Dinkcast's limits. `make host` passes (exit 0). **Nothing here was run in Flycast or on hardware.** Each finding is marked **[verified]** (read in source and/or measured in data) or **[inferred]** (follows from the code but no repro).

This review does not reopen what [GRAFT-AUDIT.md](GRAFT-AUDIT.md) and its Challenge/Response already settled. Section 4 lists which of its items are still open. Per AGENTS.md this is analysis only: no engine change, no bite started.

---

## 1. Summary

The port is in good shape for the opening village and the early campaign. The file formats, `changedir`, the hit and talk geometry, the DinkC scoping rules and the HUD are faithful grafts. The host suite is broad. The remaining risk is concentrated in a few places:

1. **Hard capacity limits that stock screens already exceed.** The fiber pool holds 20 scripts. Map 536 needs 31 and map 192 needs 21. Dynamic sprites share slots 2..99 with active editor rows, so map 145 has 90 rows and only 9 free slots. These screens will lose scripts or effects. Neither showed up because playtests have only covered the village.
2. **Unbounded RAM growth over a long session.** The `hard.dat` tile cache is never freed (up to about 2.1 MB). Every script ever loaded stays in `file_blob` as Always (up to 1.6 MB). Each fiber allocates a fixed 64 KB token array whatever the script's size. None of this shows on a single screen; all of it shows over a full campaign.
3. **Visible FreeDink gaps that stock content uses.** Animated water and fire tiles, the screen-slide transition, `stop_entire_game` (level-up menu), `ESCAPE.c` (the in-game Load/Restart/Help menu), `set_dink_speed` (herb boots), `compare_sprite_script` (map 536 secret rock), `is_script_attached`/`run_script_by_number` (burn chains), and `fill_screen`.
4. **The residency/edraw layer is the biggest source of churn.** About 102 of 478 commits touch edraw, residency, packs, Prev, PVR or holds, and about 2.2 k lines of policy sit on top of a sprite model that FreeDink doesn't have. Three structural changes would remove most of that class: PAL8 textures, baking type-0 sprites into the background, and one live-sprite table. See §6.
5. **Untested on hardware.** GD-ROM seek cost per screen swap, music streaming fighting game reads for the drive, VMU write time and stack size are all unmeasured (bite 18).

### Top 12, in suggested order

| # | Item | Kind | § |
|---|---|---|---|
| 1 | Raise the fiber cap from 20 to ≥64; size tokens to the script, not 4096 × 16 B | Capacity + RAM | 3.1, 5.3 |
| 2 | Bound the `hard.dat` record cache (LRU, ≤256 KB per plan §1.2) | RAM leak | 5.1 |
| 3 | Make `story/` a byte-capped LRU, not Always | RAM leak | 5.2 |
| 4 | Decouple dynamic sprite slots from editor indices (map 145: 9 free slots) | Capacity | 3.2 |
| 5 | Flying sprites must respect hardness 1 (fireballs and arrows go through walls) | Bug | 2.1 |
| 6 | `stop_entire_game`; exp/level-up gated on choice/inventory/freeze | Bug/gap | 2.6, 4.2 |
| 7 | Animated water/fire tiles | Gap | 4.2 |
| 8 | Run `ESCAPE.c` (or cover its Load/Restart) instead of native Continue/Title | Gap | 4.2 |
| 9 | Real-clock timestep (slow frames slow the whole game) | Bug | 2.8 |
| 10 | PAL8 textures with the one shared Dink palette (halves VRAM and CPU pixels) | Architecture | 6.1 |
| 11 | Headless FreeDink differential traces vs a host-runnable `game_tick` | Testing | 6.6 |
| 12 | Measure GD-ROM seeks/swap on hardware before more cache-policy work | Hardware | 7 |

---

## 2. Correctness bugs (diverge from FreeDink)

### 2.1 Flying sprites skip all hardness — fireballs/arrows pass through walls **[verified]**
- `src/brains.c:540` `automove`: `if (mask != NULL && !s->flying && hard_get(...))`. Any `flying` sprite skips the check.
- FreeDink `check_if_move_is_legal` (`freedink.cpp:78`) only lets flying sprites pass **hardness 2** (water). `missile_brain` (`brain_missile.cpp:39`) explodes on `hard > 0 && hard != 2`.
- Stock missiles are flying: `ITEM-FB.c:124 sp_flying(&junk, 1)`, plus `ITEM-B1/B2/B3`, `ITEM-ICE`, `ITEM-SFB`, `ITEM-AXE`. So the `DAM-*` "hit a wall" path never fires, and fireballs fly through trees, walls and sprite hardness (the `hard > 100` → victim `HIT` path).
- This needs hardness kept as 0/1/2 (§2.2) first. GRAFT-AUDIT has it as a cluster; it is still open.

### 2.2 Hardness is a boolean **[verified]**
- `src/hard.c:194` returns `g_rec[hid][off] != 0`, and `hard_stamp_tiles` writes `1`. FreeDink keeps the raw byte (0/1/2) in `screen_hitmap`.
- Effects: flying sprites can't cross water while staying blocked by walls (§2.1); Dink's push animation fires against shorelines (FreeDink skips push for `hardness == 2`, `freedink.cpp:111`).
- `hard_sample` also returns **0 (walkable)** when a record read fails (`hard.c:184`), so a GD-ROM read error silently opens walls. It should at least log, and probably treat the cell as solid.

### 2.3 `automove` overshoots when |mx| ≠ |my| **[verified]**
- `src/brains.c:512-553` steps `n = max(|mx|,|my|)` times and moves **both** axes on every step.
- FreeDink `move()` (`brain.cpp:592`) moves x only while `mx < |spr.mx|` and y only while `my < |spr.my|`, recording `lpx/lpy` per step. It also checks hardness after the move along that history, rolling back to `lpx[i-1]`.
- `changedir` always produces equal magnitudes, but DinkC `sp_mx`/`sp_my` and some missiles don't, so those paths drift diagonally.

### 2.4 Missile hitting Dink uses the wrong damage formula and order **[verified]**
- `src/brains.c:1344` calls `player_hurt(g_pl, s->strength)`, which is the `hurt_thing` roll (`hurt_roll`).
- FreeDink's `missile_brain` loop includes `spr[1]` and applies the **missile** formula (`strength==1 → strength-defense`, else `strength/2 + rand()%(strength/2)+1 - defense`) to `spr[j].damage`.
- FreeDink runs the missile's `DAMAGE` **before** the victim's `HIT`; Dinkcast runs `HIT` first (`brains.c:1380-1388`).
- Dinkcast also skips `DAMAGE` when `strength == 0`; FreeDink still runs it.

### 2.5 Dink never shows damage numbers **[verified]**
- FreeDink `human_brain` (`brain_keyboard.cpp:170-172`) calls `draw_damage(h)` before `*plife -= damage`.
- `player_apply_life` (`src/player.c:85`) doesn't, and `brains_draw_damage` has no callers. Enemies show red numbers; Dink doesn't.

### 2.6 Exp/level-up runs during dialogue, choice and freeze **[verified]**
- FreeDink `update_status_all` only advances `fexp` when `game_choice.active == 0 && show_inventory == 0 && spr[1].freeze == 0` (unless `fexp + 10 < fraise`) (`status.cpp:390`).
- `src/status.c:552` has no such gate, so `lraise` can open its choice inside another choice or a cutscene.

### 2.7 Status cadence is 100 ms instead of every frame **[verified, known]**
- `src/status.c:536`. Magic recharge, bar fill and digit scroll all run about 6× slow at 60 Hz. GRAFT-AUDIT §11 lists it; the Challenge deferred it because mana feel will change. It is a one-line class fix when the requester wants it.

### 2.8 Fixed 16 ms timestep ignores real frame time **[verified]**
- `src/main.c:1347` adds `now_ms += DINKC_TICK_MS` once per loop. There is no measurement of the vblank rate or of real elapsed time.
- **PAL consoles (deferred):** the requester scoped the release to NTSC (2026-11-19). On a PAL unit at 50 Hz, all game time (`wait()`, animation delays, TTLs, respawn timers) would run 20% slow. Recorded so a later PAL bite starts here; not on the NTSC path.
- **Slow frames:** a frame that misses vblank slows the whole game. FreeDink scales by `fps_final` (`base_timing`, `dinkspeed * base_timing/4`).
- Fix: derive `now_ms` from `timer_ms_gettime64()`, clamp large gaps (swaps), and keep the 60 Hz movement step. Movement per tick is speed-based, so only the clocks need real time.

### 2.9 Respawn timers stamp on `editor_type`, not on leaving **[verified, known]**
- `src/dinkc_cmd.c:1701` stamps `g_spmap_time` when `editor_type(6/7/8)` is set. FreeDink `update_screen_time` stamps `last_time` on every edge, warp and load, so the 1/3/5 minute respawns count from when you left. The timers are also missing from the save blob.

### 2.10 Silent stubs break the "log unimplemented DinkC" rule **[verified]**
- `src/dinkc_cmd.c:1528` makes `kill_shadow` and `fill_screen` no-ops that return 1 with no log. `:1637` does the same for `sp_kill_wait`. Commands in `k_fn[]` without a handler fall through to `return 1; /* default stub */` (`:1849`).
- Unknown commands log `dinkc unimplemented`; these three don't. They should log once per name.

### 2.11 `sp_custom` is one global table that is never cleared **[verified]**
- `src/dinkc_cmd.c:94` (`g_custom[32]`) and `:1767`. Entries are keyed by sprite number and survive sprite death and screen change, so a recycled slot inherits old values. Once 32 entries exist, new keys silently no-op. FreeDink stores them per live sprite and clears them with the sprite.

### 2.12 Timer callbacks restart a fresh fiber **[verified, known]**
- `src/dinkc_vm.c:1121` makes each `set_callback_random` firing re-copy and **re-tokenize** the whole script into a new fiber with zeroed locals. FreeDink resumes the same script instance. This costs both correctness (locals, `&argN`) and per-fire CPU (re-lexing `S2-AUNTP` is about 2 k tokens).

### 2.13 No sweep that kills scripts whose sprite died **[verified, known]**
- `kill_sprite` (`src/brains.c:341`) only clears `live`. There is no `kill_scripts_with_inactive_sprites` (`dinkc.cpp:988`, run every frame), so a fiber parked in `wait()` on a dead sprite resumes against a recycled slot.

### 2.14 The DinkC op-cap overflow is an implicit yield **[verified]**
- `src/dinkc_vm.c:616`: past 2000 ops the fiber returns still `RUN` and continues next frame. For a proc started synchronously by `external()`/`spawn` mid-command, the caller then continues before the callee finishes, which FreeDink never does. Keep it as a safety net, but log the script name and line, and consider a higher cap for synchronously started procs.

### 2.15 Save path: no integrity check, non-atomic write, stack buffers **[verified]**
- `src/save.c:606` opens with `O_TRUNC` and writes in place, so a power cut or VMU pull mid-write destroys the slot. There is no CRC, only magic and version.
- There are 8 KB `uint8_t[DINK_SAVE_MAX]` stack buffers in `save_game_slot`, `save_load_slot` and `save_info_line` (`:426,576,650,661`). `save_info_line` runs 10× when the Continue list opens, and each run reads the whole file from the VMU. The KOS main stack is about 32–64 KB (GOTCHAS).
- Fix: add a CRC32, write to a temp file and rename (or two alternating files), move the buffers to static/BSS, and read only the VMS header for the slot list.

### 2.16 BLACK keyword parsed but never applied **[verified]**
- `src/ini.c:246` recognises `BLACK` but stores no flag, so these seqs still white-key: 10 (`tiles\s`), 180 (status), 192–200 (start menu). All of them happen to be drawn by native code or over black, so nothing visible breaks today. It is still a trap for any in-world or D-Mod use.

---

## 3. Capacity limits vs stock data (measured)

The Python scans below ran over stock `map.dat` and `dink.dat` (644 live screens).

### 3.1 DinkC fiber pool: 20 vs FreeDink 200 **[verified]**
`DINKC_MAX_LIVE 20` (`src/dinkc_vm.h:8`). Attached sprite scripts are **keep** fibers (`script.c:140 dinkc_vm_start_keep`), so every scripted sprite holds a slot for the whole screen. Dink's armed weapon and magic fibers and any `spawn` add to that.

| Map | Scripted type-1 sprites (+ screen script) | Contents |
|---|---|---|
| **536** | **31** (vision 1) | 27× `s4-duck`, `s4-rock`, `s4-stat`, 2× `chN-ap`, screen `s4-ducks` |
| **192** | **21** | 17× `en-slim`, 4× `thorn` |
| **352** | **20** | 15× `lgold`, 5 potions/heart (+ weapon fiber → over) |
| 633 | 15 | 14× `en-slimb`, `en-bong` |

Past slot 20 the log says `dinkc vm full` and that sprite has no script, so no hit, die or talk. On map 192 that means unkillable or inert slimes; on map 536, a broken duck puzzle and secret.

**Fix:** raise the cap to 64–100. The real cost is the per-fiber footprint (§5.3), not the count. Moving the choice buffers (`choice_line[20][80]`, `choice_ret`) out of `struct Fiber` (only one choice is ever active) and right-sizing tokens makes 100 fibers cheaper than today's 20.

### 3.2 Dynamic sprite slots share editor indices **[verified]**
`brains_create` (`src/brains.c:2031-2038`) uses slots 2..99 and **skips any active editor row, including type-0 background sprites and rows on another vision**. FreeDink allocates live sprites from a 300-slot pool and frees type-0 sprites right after baking them (`lsm_remove_sprite`, `game_engine.cpp`).

| Map | Active editor rows | Slots left for blood, damage numbers, missiles, `create_sprite` |
|---|---|---|
| **145** | 90 | **9** |
| 671 | 70 | 29 |
| 536 | 66 | 33 (with 27 ducks fighting) |

On a full table `brains_create` returns 0 with no log, so effects and scripted creates vanish. GRAFT-AUDIT's Response already said the silent fail, not the number, is the rule change. §6.3 is the structural fix; the stopgap is to log every refusal and stop reserving type-0 and other-vision rows.

### 3.3 Save blob 8 KB **[inferred]**
- Fixed part: about 60 B header, 80 B info, 600 B inventory, plus about 24 B per global (`MAIN.c` alone has about 63 globals), so roughly 2.7 KB.
- That leaves about 5.4 KB at 7 B per `spmap` entry, about **780 entries**. Every killed monster, smashed barrel and picked item with `editor_type` adds one. A late-campaign save may fail `save_pack` (`o > DINK_SAVE_MAX`).
- The failure path needs a visible message, and the cap needs checking against a full-campaign `spmap` count. The `ns > 2000` read guard (`save.c:308`) is larger than the 8 KB write cap allows anyway.
- VMU: about 17–18 blocks per slot, so 10 slots use about 180 of 200 blocks. Saves to a VMU that is already partly full will fail. The fallback should say so.

---

## 4. FreeDink gaps

### 4.1 Missing DinkC bindings used by stock `Story/*.c` **[verified]**
This is a diff of FreeDink `DCBD_ADD` (186 bindings) against Dinkcast `k_fn[]` plus the VM keywords. It lists only names that appear in stock scripts (file count in brackets).

| Binding | Stock use | Impact |
|---|---|---|
| `compare_sprite_script` [3] | `S4-ROCK`, `S4-SEC1`, `S5-SEC1` | Secrets never trigger (S4-ROCK is on map 536) |
| `is_script_attached` [2], `run_script_by_number` [2] | `DAM-FIRE`, `DAM-SFB` | Tree/object burn chain; already in PLAYTEST Open |
| `set_dink_speed` [2] | `MAIN.c`, `ITEM-BT` | Herb boots do nothing; walk is fixed at `DINK_SPEED 3` |
| `stop_entire_game` [1] | `LRAISE.c` | Enemies keep moving and hitting during the level-up stat choice |
| `dink_can_walk_off_screen` [1] | `S2-CULG.c` | Cult cutscene walk-off triggers a screen change |
| `free_magic` [1] | `S6-VEND.c` | Vendor slot check returns 0 → "full" |
| `scripts_used` [1] | `s1-ntree` | Cap check returns 0 (harmless today) |
| `sp_sound` [2] | — | Already in PLAYTEST Open |
| `get_burn` [2] | `S7-HELL`, `S7-MIL` | Returns 0; FreeDink returns 1 |
| `set_button` [1] | `ESCAPE.c` | Only matters if ESCAPE.c runs (§4.2) |
| `sp_reverse` [5], `sp_noclip` [5], `set_mode` [2], `get_version` [1], `load_sound` [1], `reset_timer` [1] | START*.c | Native start menu covers these; reverse/noclip matter if START.c is ever run |

Plus three silent stubs: `fill_screen` (`S1-LTR`, `S2-BAR`, `S2-CAVE2`, `S2-MDOOR`, `START`), `kill_shadow` (`DAM-*`), and `sp_kill_wait` (about 10 item scripts).

### 4.2 Missing engine behaviour (not DinkC) **[verified]**
| Gap | FreeDink | Notes |
|---|---|---|
| **Animated tiles** | `process_animated_tiles` (`gfx_tiles.cpp:77`): water swaps tilesheet 8↔9 at random <2 s, fire cycles sheets 19–23 | Water and lava screens render frozen. The atlas needs the alternate-sheet cells (+128·k) for those tiles. No code in `tiles.c`. |
| **Screen-slide transition** | `grab_trick` + `transition()` (`live_screen.cpp:440-592`), about 19 frames at 32 px/frame | Edge crossing is an instant cut. On PVR: render-to-texture both 600×400 frames and slide two quads. It also hides part of the load latency. |
| **`ESCAPE.c` menu** | `brain_keyboard.cpp:451` `load_script("escape", 1000)` | Native Continue/Title instead. No in-game Load (you must quit to title), Restart, Help or button view. |
| **`stop_entire_game`** | `update_frame.cpp:268` | See §4.1. Also used by D-Mods. |
| `update_screen_time` | Stamped on every leave | §2.9 |
| Dink damage floaters | `human_brain` `draw_damage` | §2.5 |
| `push_active()` | `dc_push_active` | Missing binding (D-Mod use) |
| Talk fallback | Rescan past first in-box sprite without TALK; random quip | GRAFT-AUDIT §4 residual; silence instead of a quip |
| Font | LiberationSans TTF (FreeDink) / Arial (1.08) | Embedded VGA 8×8 is an accepted V5 shape, but visibly different. Option: rasterize a TTF found on the build host into an atlas at disc-build time (never commit it). |

### 4.3 Status of GRAFT-AUDIT items (re-checked today)
- **Fixed since the audit:** map.dat +100/+104/+108 bases; same-file proc calls; `stopmidi`; `sp_dir`/`sp_speed` `changedir`; missile `DAMAGE` and `brain_parm` skip; `free_items`; `fade_*`; `force_vision`; `screenlock`; `sp_disabled`/`sp_nodraw`.
- **Still open:** hardness 2; flying vs walls; `set_dink_speed`; `push_active`; `sp_reverse`/`sp_picfreeze`; dead-sprite script sweep; callback fresh fiber; `compare_sprite_script`; `fill_screen`/`kill_shadow`/`sp_kill_wait`; `free_magic`; `is_script_attached`/`run_script_by_number`; `sp()` identity; single saybox; `sp_custom` global; status cadence; `update_screen_time`; inventory/map overlay still runs `dinkc_vm_tick`/`saybox_tick` (`main.c:1303-1306`); `make_global_function` (D-Mod only); bow charge.

---

## 5. Memory (main RAM)

Plan §1.2 caps the engine at ≤10 MB. These pools are not counted by `mem_log`, or they grow without bound.

### 5.1 `hard.dat` record cache never frees **[verified]**
`static uint8_t *g_rec[800]` (`src/hard.c:12`) mallocs 2 608 B per unique hardness tile (`hard_ensure_rec`) and never frees it. Over a full campaign it can reach about **2.08 MB** plus malloc overhead. That is outside `file_blob`/`cpu_pixels`/`ts_rgb`, so `mem_log` never shows it. Plan §1.2 says `hard_cache` ≤ 256 KB.

**Fix:** LRU by bytes (about 100 records), or keep only the 96 records the current screen references and drop the rest on swap. Prefer the same **class** policy as packs (Screen/Prev) over a new special.

### 5.2 Every loaded script stays forever (`story/` is Always) **[verified]**
`residency_is_always` includes `"story/"` (`src/residency.c:46`). Stock `Story/` is **1.6 MB**, so over a playthrough Always grows by up to that much inside the 4.5 MB `file_blob` cap. That squeezes Screen/Prev and produces more `mem refuse` late in the campaign than in the village, where everything was tuned.

**Fix:** a Script class with a byte LRU (say 256 KB). Fibers copy the source anyway, so dropping the blob is safe.

### 5.3 Fixed 64 KB token array per fiber **[verified]**
`fill_toks` (`src/dinkc_vm.c:497`) mallocs `4096 × sizeof(DinkcTok)` (16 B) = **64 KB per fiber**, regardless of script size. The largest stock script is about 2.1 k tokens (`S2-AUNTP.c`); the median is far smaller. Twenty live fibers use about 1.3 MB, uncounted. Each fiber also holds `locals[256]` and the choice buffers (about 2.8 KB) plus a full source copy.

**Fix (in order of value):**
1. Count tokens and then allocate exactly. This alone recovers most of the 1.3 MB.
2. Share one immutable tokenized program per script name (refcounted). Fibers keep only `ip`, locals and wait state. This also removes the re-lex on every talk/hit/callback (§5.4) and makes the 100-fiber cap in §3.1 cheap.

### 5.4 Triple copies and a full parse per script start **[verified]**
- A script start goes blob → `dinkc_load` malloc copy → fiber `src` copy → token array.
- `src_has_proc` (`src/script.c`) runs a full `dinkc_parse` just to check whether a proc exists, on every talk/hit/touch.
- Both disappear with a shared program cache (§5.3) that records proc entry points at load.

### 5.5 Smaller items
- `memcpy(g_scr.sprite, g_spr_ok, 101 × sizeof)` runs three times per frame (`main.c:1157,1488` plus `brains_apply` twice). That is wasted bandwidth and a symptom of §6.3.
- `g_spmap_type/seq/frame` is about 310 KB of BSS for 769×101 entries. That's fine, but it's a fixed cost worth listing in `mem_log`.
- PVR `vertex_buf_size = 512 KB` (`tiles.c:297`), which is double-buffered, so about 1 MB of VRAM. A frame is roughly 100 sprites plus text glyphs; 128–192 KB would free about 700 KB of VRAM.

---

## 6. Architectural improvements

These are larger changes. Each would retire a class of bugs rather than one screen's symptom. All of them need their own plan bite and a requester go.

### 6.1 PAL8 textures with the shared Dink palette **[verified basis]**
Measured: **all 3 447 BMPs inside stock `dir.ff` packs are 8-bit, uncompressed, and share one identical palette** (md5 `70bbcf3e…`). Index 0 is white, and no other entry is near-white. Of 181 loose BMPs, only one is 24-bit.

- The PVR supports twiddled 8bpp paletted textures with on-chip palette RAM (4 banks × 256).
- Uploading index data plus one 256-entry palette (index 0 = alpha 0):
  - **halves** VRAM and `cpu_pixels` versus ARGB1555; decode becomes a memcpy plus twiddle, with no per-pixel RGB conversion;
  - makes the colorkey exactly FreeDink's rule (index 0, or index 255 for BLACK; `gfx_sprites.cpp:236-262`), which fixes §2.16 for free;
  - allows a FreeDink-style palette fade (`CyclePalette`/`up_cycle`) by rewriting palette RAM, if the TR overlay ever looks wrong.
- Tilesheets are 8-bit as well, so the tile atlas could use a second bank. That cuts `ts_rgb` (1.25 MB cap) in half.
- This is the single change that most relieves the `cpu_pixels` and VRAM pressure that §6.2–6.4 keep fighting.

### 6.2 Bake type-0 editor sprites into the background
FreeDink draws type-0 sprites into the background once per screen and frees them (`game_place_sprites`, `lsm_remove_sprite`). Dinkcast keeps every type-0 frame resident as a texture and draws it as a quad every frame. This is why house walls, innwalls and details compete with animated sprites for `cpu_pixels` and Screen PVR.

- **Proposal:** at `draw_screen`, render tiles plus type-0 sprites in rank order into one 640×400 (1024×512) render-to-texture with `pvr_render_to_texture`, then free those sprites' textures. Per-frame work becomes one background quad plus live sprites.
- This is faithful to FreeDink, uses the PVR (not a CPU blit renderer), and greatly shrinks the working set the residency policy must manage.
- Hardness is unaffected: `fill_back_sprites` uses ini hardboxes, not pixels.
- Re-bake on `draw_screen`, `draw_background` and `draw_hard_map`, where FreeDink re-blits too.

### 6.3 One live-sprite table, as in FreeDink
Today the same objects are spread over three structures:
- `g_scr.sprite[1..100]` (editor rows, used as the **render list**), plus the `g_spr_ok` snapshot that is copied back every frame;
- `g_b[1..100]` `BrainSpr` (indexed by **editor** slot), mirrored back into editor rows by `brains_apply`;
- a separate `Player` for Dink.

Consequences visible in history and code:
- editor slot 1 collides with Dink's sprite 1 ("editor slot 1 `sp_*` is not Dink", "`sp_seq(1)` is Dink on the editor-1 hole");
- `sp()`/`sp_editor_num` are identity stubs;
- created-sprite capacity depends on editor density (§3.2);
- talk only sees created NPCs after last frame's mirror;
- stats are duplicated between `Player` and DinkC vars (`pl.defense = &defense` each frame, the `hurt(1)` desync in GRAFT-AUDIT).

**Proposal:** `spr[1..MAX]` with `spr[1]` = Dink, editor rows placed by `add_sprite_dumb` with `sp_index = j` (`game_engine.cpp:466`), and the editor table read-only after `update_play_changes`. Make Dink's `life`/`strength`/`defense` pointers into the var table (`plife`, `pstrength`, … in FreeDink). The metadata cap can be 300, as the plan's §1.1 already says ("300 metadata slots, textures only for live seqs").

This is the riskiest refactor. Do it after §6.6 exists so traces can prove there is no behaviour change.

### 6.4 Simplify residency now that 14.6 (per-frame reads) has landed
14.6 means packs no longer have to sit in RAM to decode one frame (TOC plus `SEEK_SET`). Much of the Always/Screen/Prev **pack** machinery (14.4b), plus the "hold / live-touch / current+next / reap" pixel machinery (14.4c) layered on it, existed to keep packs and pixels resident.

With §6.1 and §6.2 a simpler policy could fit:
- **Enter path:** for this screen's live set (editor type-1 and type-2 seqs, base walk/idle/attack/hit/die of live brains, `preload_seq`, Dink's set), read frames by TOC, upload to VRAM, and free the CPU copy at once.
- **Play path:** VRAM only, never disc. A miss logs and skips (unchanged rule).
- **Eviction:** a byte LRU over VRAM keyed by (seq, frame), with the frames drawn this tick pinned. Across screens, the LRU gives Prev-style backtracking without a separate class.

About 21% of commits (102/478) and about 2.2 k lines touch this area today, and PROGRESS shows most fixes named after a specific map. That is the symptom AGENTS.md warns about. Write this up as a plan amendment (the plan names 14.4b/14.4c as policy) rather than changing code piecemeal.

Related: a **deferred texture-free list** (free N frames later) would let `pvr_wait_ready()` move from the top of the frame (`main.c` "Finish the last scene before evicting") to just before `pvr_scene_begin`. Game logic would then overlap the GPU render of the previous frame. The top-of-frame wait exists only to make eviction safe.

### 6.5 Per-screen disc bundles (bite 18.2 direction)
Today a screen swap does a path resolve (stat, then `readdir` scan on a case miss, `fs.c:238`), an open, a TOC read, and scattered frame reads, for each pack, tilesheet and script. Flycast hides that; a real GD-ROM seek is about 100–300 ms, and CD-R is worse.

- **Proposal:** extend the existing distill step (14.5) at disc-build time. Emit one contiguous bundle per screen (or region) holding that screen's tiles and frames, already **twiddled PAL8**, plus a small index. A swap is then one seek, one sequential read and a DMA to VRAM, with no CPU decode.
- Legally this is the same as the current distill and ADPCM steps: derived from `DINK_DATA` at build time, never committed.
- Short of that, a build-time manifest (lowercase rel → exact ISO name) would remove the per-open directory scans.

### 6.6 Host-runnable game loop plus FreeDink differential traces
`main()` holds the entire game loop inside a DC-only `#ifdef` (`main.c`, about 700 lines in one block). The host build is a stub (`main.c:697`), so host tests re-create slices of the loop. That's why ordering bugs (talk before `brains_apply`, VM during inventory) are found by reading rather than testing.

- **Step 1:** pull out `game_tick(Game *, Input)` and `game_render(Game *, Renderer *)`. Give the renderer a PVR backend on DC and a recording backend on the host (a draw list of (seq, frame, x, y, z)). Most of the ~25 `dinkc_cmd_bind_*` function-pointer binds and module-static globals fold into a `struct Game` context.
- **Step 2:** FreeDink ships `FakeIOGfxDisplay.cpp`, so a headless FreeDink build is practical. Add a per-frame dump of `spr[]` (x, y, seq, frame, brain, hp) and the var table. Feed both engines the same scripted input (for example, new game → walk to the pig pen → punch a pig) and diff the traces.
- This turns "faithful to FreeDink" from code reading into a test. It is also the safety net that §6.3 needs.

### 6.7 Cheaper name lookups
- DinkC commands dispatch through a chain of about 150 `is_cmd` `strcmp` calls (`dinkc_cmd.c`). FreeDink uses a hash table (`dinkc_bindings_lookup`). Resolve the command index once at tokenize time and store it in the token.
- Variable reads scan up to 250 slots with `strcmp` each time (`dinkc_var.c`). That includes the engine's own per-frame reads: `status_update` alone does dozens of `dinkc_var_get("&…")` calls per tick. Intern names to slots at tokenize time, and give the engine cached slot indices (FreeDink's `plife`, `pexp`, … pointers).
- Neither is urgent at 60 FPS in the village. Both scale with script density, and bite 18.1 should measure them.

---

## 7. Hardware risks (unmeasured; bite 18)

| Risk | Why it matters | What to measure |
|---|---|---|
| GD-ROM seeks per swap | Flycast hostfs is instant; real seek time × (packs + tiles + scripts + hard recs + map rec) per swap could be seconds | Log `disc_opens`, seek count, bytes and `swap_ms` per swap on ODE and burned CD-R |
| Music refills compete with game reads for the drive | `audio_music_poll` reads WAV from disc on the main thread; each refill moves the head away from game data | Underruns per swap. Alternative: load a whole track as ADPCM into AICA RAM on music change (about 11 KB/s at 22 kHz mono, so 1 MB ≈ 90 s; with a 512 KB SFX cap, about 1.4 MB free). No streaming, hardware loop. |
| VMU write time | 16+ blocks written synchronously stalls the frame; music may stutter | Time `save_game_slot`; show a "Saving…" state |
| Main-thread stack | Several 3 KB locals in the VM (`title_from_src raw[3000]`), 8 KB save buffers, deep recursion in `eval_*` | Use KOS stack-guard builds or a larger main stack via `KOS_INIT_FLAGS`/thread config |
| Controller | D-pad only; no analog stick; no hot-plug message; no A+B+X+Y+Start soft reset | Map the analog stick to 8-way; add the reset combo |

---

## 8. Smaller cleanups

- Fix the misleading `DINK_BASE_TIMING` comment (`brains.c:38`). At 60 Hz, FreeDink `fps_final` is 16, so `base_timing` is 5, not 4. The result is the same because only `base_timing/4` (= 1) is ever used.
- `brains_draw_damage` (public) has no callers (§2.5).
- `title_show_splash`'s comment says it doesn't show a splash; the name does.
- `struct DrawSpr draw[101]` is bubble-sorted every frame (`main.c:1540`). That's fine at ≤101; an insertion sort over an already nearly-sorted list would be cheaper.
- GOTCHAS is about 63 KB. A one-screen "top 15 rules" index at the head would help agents more than a longer list.

---

## 9. Suggested sequencing (requester-gated, one concern per PR)

1. **Capacity and leaks, with no behaviour change:** right-size tokens (§5.3); fiber cap ≥64 with the choice buffers moved out (§3.1); bound the `hard.dat` cache (§5.1); Script LRU class (§5.2); log `brains_create` refusals (§3.2); log the silent stubs (§2.10). Host locks: map 536 and 192 attach counts; a 200-crossing campaign walk with flat `mem_log`.
2. **Test harness:** `game_tick`/`game_render` split plus a recording renderer (§6.6 step 1), then headless FreeDink traces (step 2).
3. **Visible gaps:** animated tiles, `stop_entire_game` plus the exp gate (§2.6), `ESCAPE.c`, `set_dink_speed`, `compare_sprite_script`, `is_script_attached`/`run_script_by_number`, Dink damage floaters, the screen-slide transition.
4. **Hardness 0/1/2, flying and missile walls** (§2.1–2.4) as one class PR, locked by traces.
5. **Timing:** real-clock `now_ms` for slow frames (§2.8; PAL deferred) and status cadence (§2.7), with the requester checking mana feel.
6. **Architecture:** PAL8 (§6.1), then type-0 bake (§6.2), then the residency simplification as a plan amendment (§6.4), then the unified sprite table (§6.3) behind traces.
7. **Bite 18 on hardware**, including disc bundles if the seek numbers call for them (§6.5, §7).
