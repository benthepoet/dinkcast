# Campaign graft audit (v0.1.0)

**When:** 2026-08-22. **Tree:** `master` at `#100` (say TTL) after tag **v0.1.0**.  
**Kind:** analysis only. No code in this document.  
**Canon:** GNU FreeDink graft. Official 1.08 `Story/` only (381 files). Not D-Mods, not DinkEdit.

Four parallel passes: DinkC bindings vs `k_fn[]` vs stock scripts; brains 0–17 vs `update_frame`; engine systems vs the plan table; quest scripts vs dispatch.

Spot-checks after those reports (do not take every “block leaving the village” claim at face value):

- Walking off Stonebrook is engine **edge/warp** (`14.1–14.2`), not DinkC `load_screen`.
- New game is engine `leave_title`, not `START-1.c` `set_mode`.
- Magic USE is **held X**; bow is **instant power 100**, not a log-only stub.
- `sp_target` is in `k_fn[]` but writes `g_target[]`, not live `BrainSpr`.
- `script_attach` rebinds the fiber sprite in the VM; it is not a full FreeDink `dc_script_attach`.

---

## Verdict

**Village combat, talk, inventory, and HUD are grafted enough to play.**  
**The full 1.08 campaign is not.** Coverage is about **98 / 186** FreeDink DinkC names in `k_fn[]` (~52%). Stock scripts use **~125** of those 186. The rest of the pole is VM control flow (`goto`, `spawn`), screen-script helpers (`load_screen` / `draw_screen` / `screenlock`), enemy AI wiring (`sp_target` → brains, `get_sprite_with_this_brain`), then audio **12**, VMU **17**, and **14.6**.

Plan feasibility already called full campaign **~55–65%**. This audit agrees: the missing pieces are named FreeDink functions, not “the DC is too weak.”

---

## Already grafted (do not redo)

| Area | FreeDink | Dinkcast |
|---|---|---|
| Title / tiles / house / walk / hardness | `load_screen_to`, `get_hard`, `game_place_sprites` | Bites 0–9, 8.6 |
| Talk / hit / miss say | `run_through_tag_list_talk`, hit list, `human_brain` miss lines | 10.1–10.2 |
| DinkC lex / parse / fibers / vars / attach | `dinkc.cpp`, `game_screen_init_scripts` | 11.0–11.6 |
| Live sprite cmds | `move`, `create_sprite`, NPC `sp_*` | 11.10 |
| Say / choice / font | `say_text`, `game_choice` | 13; `say()` TTL `#100` |
| Edge + warp + `parm_seq` | `did_player_cross_screen`, `special_block` | 14.1–14.2 |
| Residency + distill | — | 14.4a–c, 14.5 |
| Brains 0–17 dispatch | `update_frame` | 15.1 (`13`/`14` log only) |
| Damage / DIE / blood / weapons / magic USE | `hurt_thing`, item ARM | 15.2–15.4 |
| Touch / inventory / HUD / L map | `process_item`, `draw_status_all`, `process_show_bmp` | 16.1–16.3 |
| `play.spmap` types 1 and 3 | `update_play_changes` | `dinkc_cmd_apply_spmap` |
| Item keep fiber 1000 | ARM on sprite 1000 | `dinkc_vm` keep |

---

## Recommended order (still analysis)

Work that unblocks **stock scripts**, in campaign order. Plan bite ids stay; this is not a license to skip gates.

1. **VM `goto`** — parsed, never jumps (`dinkc_parse.c` vs `run_fiber`). ~88 uses / 22 files (`S2-OUT.c`, `ESCAPE.c`, `S1-LG.c`, `S8-DA.c`, …).
2. **`spawn`** — not in `k_fn[]`. ~7–21 uses (`ITEM-BOM.c`, `EN-DRAG.c`, `S4-DUCK.c`, `s7-boss.c`). FreeDink `dc_spawn`.
3. **`load_screen` + `draw_screen` + fades** — `load_screen` silent no-op (`dinkc_cmd.c` with `fade_*` / `fill_screen`); `draw_screen` missing. ~18 files (holes, caves, island warps). **Not** the same as walking a map edge.
4. **`screenlock`** — missing. ~21–33 files. FreeDink `dc_screenlock` + `get_hard` clamp. Boss/cult/castle arenas.
5. **Wire `sp_target` / `sp_attack_wait` / `sp_distance` / `sp_follow` onto `BrainSpr`** and graft `process_target` / `process_follow`. Pill/dragon ATTACK. `sp_follow` used by Quackers (`S1-DUCK.c`).
6. **`get_sprite_with_this_brain`** (and rand variant) — missing. ~25 files. `EN-DRAG.c`, castle, cult.
7. **`sp_frame_delay`** — missing. ~21–37 files. Boss/enemy timing.
8. Inventory DinkC: `count_item`, `free_items`, `kill_this_item` / `kill_cur_item` — boot/bomb/elixir.
9. **`say_stop_xy` / `say_xy`** — missing. King / letter / save UI text placement.
10. **`indoor[]` / `last_map`** — parsed, unused. Map HUD dot after indoor warps.
11. **editor_type 6/7/8 `last_time`** + VMU `save_game` / `load_game` / `game_exist` — bite **17**.
12. Audio **12** (`playsound` ~339, `playmidi` ~44, `load_sound` in `START.c` only).
13. **14.6** per-frame `dir.ff` — requester full-campaign go; not the first script hole.

**Verify before treating as P0:** `kill_this_task` vs FreeDink `proc_return` (village ARM/USE already works; parent resume may still be wrong for nested procs). `while` loops (rare). In-file `void proc()` calls besides `external`.

---

## DinkC surface

| Metric | Count |
|---|---|
| FreeDink `DCBD_ADD` names (+ `sp_base_death` alias) | 186 |
| Dinkcast `k_fn[]` | 98 |
| Real handlers (approx.) | ~74 |
| In table but stub / silent success | ~16 (`playsound`, `playmidi`, `load_screen`, fades, `fill_screen`, `activate_bow` instant-100, …) |
| Missing from table → `dinkc unimplemented` | ~92 |
| Used in official `Story/` | ~125 of 186 |
| Missing/stub **and** used in campaign | ~39–54 distinct names |

Dinkcast-only table names (not FreeDink bindings): `stop`, `choice_start`, `choice_end`, `external`, `update_status`. FreeDink `external` lives in `process_line()`, not `DCBD_ADD`.

### VM / interpreter gaps (not just `k_fn`)

| Gap | FreeDink | Dinkcast | Campaign |
|---|---|---|---|
| `goto` / labels | `locate_goto` in `process_line` | Parse only | Gossip, shops, escape, late-game |
| `spawn` | `dc_spawn` | Absent | Bombs, dragons, end sequences |
| `kill_this_task` | Resume `proc_return` | `fiber_kill` | Mostly OK; nested parent resume unverified |
| `external` | `process_line` | `bind_external` + `WAIT_EXT` | Loot (`MAKE.c` / `EMAKE.c`) — village smash depends on this |
| Same-file `void foo()` | `locate` + `run_script` | Fiber starts at one proc | Possible silent skip |
| `make_global_int` | Binding | VM opcode | `MAIN.c` OK |
| Say `&var` substitution | `decipher_string` | Unverified in saybox | Dialogue may show `&life` literally |
| `script_attach` | `dc_script_attach` | Return arg + VM `f->sprite =` | Partial |

### Commands used in campaign and missing or stubbed

**Missing (not in `k_fn[]`), used in 1.08:**  
`spawn`, `draw_screen`, `screenlock`, `save_game`, `load_game`, `game_exist`, `set_mode`, `reset_timer`, `set_dink_speed`, `say_xy`, `say_stop_xy`, `load_sound`, `get_version`, `sp_noclip`, `sp_reverse`, `sp_follow`, `sp_sound`, `sp_frame_delay`, `sp_nodraw`, `get_sprite_with_this_brain`, `get_rand_sprite_with_this_brain`, `dink_can_walk_off_screen`, `count_magic`, `count_item`, `free_items`, `kill_this_item`, `draw_hard_map`, `stopmidi`, `compare_sprite_script`, `run_script_by_number`, …

**In table, not FreeDink-complete:**  
`playsound`, `playmidi`, `load_screen`, `fade_up`/`fade_down`, `fill_screen`, `activate_bow` (no charge loop), `sp_target` (side array), `sp_attack_wait` (same), `get_next_sprite_with_this_brain` (no-op), `kill_shadow`, `sp_kill_wait`, `sp_attack_hit_sound*`, `initfont`, `wait_for_button` (yield, no pad).

**0 stock hits (defer):**  
`copy_bmp_to_screen`, `wait_for_button`, `callback_kill`, `math_*`, `get_date_*`, `sp_clip_*`, `sp_gold`, `make_global_function`, `set_keep_mouse`, most editor/truecolor helpers.

---

## Brains 0–17

Dispatch: FreeDink `update_frame.cpp`; Dinkcast `brain_switch()` + player in `player_step` / `main.c`.

| ID | Name | Status | Notes |
|---|---|---|---|
| 0 | none | Gap | No `process_follow` |
| 1 | human | Mixed | Walk/push/talk/USE/held magic/inventory OK. Missing: diagonal slide, `sp_base_idle` on idle seq, player hurt floater/sfx, freeze+talk hurry text. Bow charge later. Brains 13/14 N/A |
| 2 | bounce | Aligned | Bounds use playfield constants vs `getpic` |
| 3 | duck | DIE aligned | No follow; no idle SFX; no dead-duck blood drip |
| 4 | pig | Aligned | SFX nit |
| 5 | one_time bake | Aligned | `bg_baked` → type 0 (smash under Dink confirmed 2026-08-22) |
| 6 | repeat | Gap | `seq_orig` may follow live `pseq` after `brains_apply` |
| 7 | one_time stay | Aligned | `hidden=1` so snapshot does not respawn |
| 8 | text | Mixed | Say is `saybox_*`. Hit numbers OK. Kill exp is `&exp` only (no +N floater) |
| 9 | pill | **Gap** | No `process_target`; `sp_target` not on `BrainSpr` |
| 10 | dragon | **Gap** | No target / ATTACK wait |
| 11 | missile | Gap | Hit + `last_hit` OK. No hard>100 HIT script, sfx, blood, `strength==1` roll |
| 12 | scale | Aligned | Pickup shrink |
| 13 | mouse | Deferred | Pad port; log unimplemented |
| 14 | button | Deferred | Title leftover |
| 15 | shadow | Aligned | |
| 16 | people | Aligned | Follow still missing |
| 17 | missile expire | Gap | Same as 11 + no seq repeat |

`brains_create` now defaults `hard=1` like `add_sprite_dumb` (`#98`). Slot search 2..99 skipping active editor is a Dinkcast layout choice (slot 1 is not Dink).

---

## Engine systems vs plan table

| Plan row | Status | Campaign note |
|---|---|---|
| screenlock | Missing | Arenas |
| indoor / last_map | Parsed, unused | Map marker |
| fade | Stub | Cosmetic + some `load_screen` sequences |
| force_vision | OK | |
| editor_type 1 / 3 | OK | Types 6/7/8: no `last_time` |
| play.spmap save | RAM only | Bite 17 |
| warp parm_seq | OK | |
| dinfo life 0 | OK | `load_game` in that menu missing |
| push | OK | |
| show_bmp / button6 | Partial | Dot uses `&player_map` |
| inventory DinkC extras | Missing | `count_item` / `free_items` |
| HUD / magic meter | OK | |
| playsound / playmidi | Stub | Bite 12 |
| VMU | Missing | Bite 17 |
| 14.6 dir.ff | Pending | RAM, not first script hole |
| hardness 2 flying | Partial | Dink passes `h==2`; no screenlock |
| get_box full clip | Partial | Skip fully off-screen only |

---

## Quest scripts (illustrative)

| Arc | Scripts | First holes |
|---|---|---|
| Ethel / Quackers | `S1-H2-O.c`, `S1-DUCK.c`, `FINDDUCK.c`, `S1-LG.c` | `sp_follow`; `goto` in gossip |
| Holes / letter | `S1-HOLE.c`, `S1-HOLE3.c`, `S1-LTR.c` | `load_screen` / `draw_screen` |
| Milder / bar | `S2-MAN2.c`, `S2-OUT.c` | `goto` loops |
| Castle | `SC-*.c`, `KING.c` | `screenlock`, brain lookup, `say_stop_xy` |
| Dragons / end | `EN-DRAG.c`, `S5-END.c`, `s7-boss.c` | `spawn`, `screenlock`, targeting |
| Islands | `S6-WARP.c`, `S8-DA.c`, `S6-VEND.c` | `load_screen`, `goto`, `count_magic` |
| Save / death | `ESCAPE.c`, `SAVEBOT.c`, `DINFO.c`, `START-2.c` | `save_game` / `load_game` / `game_exist` |

Engine boot files still in data (`MAIN.c`, `START*.c`, `BUTTON6.c`) — this port does not run the PC title brain the same way. Treat `set_mode` / `load_sound` / `sp_noclip` as title leftovers unless a script is actually attached.

---

## PLAYTEST vs this audit

Village Open is **empty** (requester 2026-08-22): 409 house, smash y-sort, and pig-pen fence are confirmed. Those were occupancy/paint, not DinkC holes.

Campaign-hard failures start at **`goto`**, **`spawn`**, **`load_screen`/`draw_screen`**, **`screenlock`**, and **pill/dragon targeting**.

---

## Out of scope

D-Mod loader, DinkEdit, keyboard/mouse brains, `wait_for_button` (0 story hits), truecolor/PC paths, CD-DA `playmidi` tracks (plan: MIDI id table in bite 12).

---

## Sources

- `/home/benh/Source/dinkcast/src/dinkc_cmd.c` (`k_fn[]`, stubs at `load_screen` / `fade_*`)
- `/home/benh/Source/dinkcast/src/dinkc_vm.c` (`run_fiber`, no `goto`)
- `/home/benh/Source/dinkcast/src/brains.c`
- `/home/benh/Source/gnu_freedink/src/dinkc_bindings.cpp` (`dinkc_bindings_init`)
- `/home/benh/Source/gnu_freedink/src/update_frame.cpp`
- `/home/benh/Source/freedink-data-1.08.20190120/dink/Story/`
- Plan § Official campaign systems; `docs/FREEDINK-ALIGN.md` (stale in places; this file wins until that table is updated)
