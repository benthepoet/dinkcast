# Campaign graft audit

**When:** refreshed 2026-08-23 after **v0.3.0**. Original pass 2026-08-22 on `#100`.  
**Canon:** GNU FreeDink graft. Official 1.08 `Story/` only (381 files). Not D-Mods, not DinkEdit.  
**Canvas:** [docs/canvases/campaign-graft-audit.canvas.tsx](canvases/campaign-graft-audit.canvas.tsx). Slices: [docs/V0.2.md](V0.2.md), [docs/V0.3.md](V0.3.md).

Four parallel passes: DinkC bindings vs `k_fn[]` vs stock scripts; brains 0–17 vs `update_frame`; engine systems vs the plan table; quest scripts vs dispatch.

Spot-checks after those reports (do not take every “block leaving the village” claim at face value):

- Walking off Stonebrook is engine **edge/warp** (`14.1–14.2`), not DinkC `load_screen`.
- New game is engine `leave_title`, not `START-1.c` `set_mode`.
- Magic USE is **held X**; bow is **instant power 100**, not a log-only stub.
- `sp_target` / `sp_follow` write `BrainSpr` (`process_target` / `process_follow`). Host #107.
- `script_attach` rebinds the fiber sprite in the VM; it is not a full FreeDink `dc_script_attach`.

---

## Verdict

**Village combat, talk, inventory, HUD, START, and VMU are grafted enough to play.**  
**The full 1.08 campaign is not.** v0.2 host items 1–6 and v0.3 START/VMU **17** are on `master`. Remaining named holes (this PR): `sp_frame_delay`, inventory `count_*` / `kill_*_item`, `say_stop_xy`, `indoor[]`/`last_map`, editor_type 6/7/8 timers. Then audio **12** and **14.6**. Flycast stamps for holes/`goto`/Quackers/`screenlock` are still PLAYTEST Open except letter fade_up.

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
| START + VMU **17** | `save_game` / `load_game` / `START.c` | v0.3.0 #111 |

---

## Recommended order (still analysis)

Work that unblocks **stock scripts**, in campaign order. Plan bite ids stay.

**Landed (v0.2 / v0.3):** `goto` #102, `spawn` #104, `load_screen`/`draw_screen` #105, `screenlock` #106, target/follow #107, brain lookup #108, `free_items` #109, START/VMU #111.

**This PR (audit remainder):**

7. **`sp_frame_delay`** — `spr.frame_delay` overrides seq delay (`live_sprite_animate`).
8. Inventory: **`count_item` / `count_magic`**, **`kill_this_item` / `kill_cur_item`**.
9. **`say_stop_xy`** — `dc_say_stop_xy` yield; king / letter. `say_xy` already bound.
10. **`indoor[]` / `last_map`** — outdoor screens update `play.last_map`; map HUD dot uses it.
11. **editor_type 6/7/8 `last_time`** — `fix_dead_sprites` 5/3/1 min (`thisTickCount`).

**Still later:** audio **12**, **14.6**. Flycast pictures: holes swap, gossip/shop `goto`, Quackers follow, screenlock arena.

**Verify before treating as P0:** `kill_this_task` vs FreeDink `proc_return` (village ARM/USE already works; parent resume may still be wrong for nested procs). `while` loops (rare). In-file `void proc()` calls besides `external`.

---

## DinkC surface

| Metric | Count |
|---|---|
| FreeDink `DCBD_ADD` names (+ `sp_base_death` alias) | 186 |
| Dinkcast `k_fn[]` | 112+ (this PR adds frame_delay / count / kill / say_stop_xy) |
| Real handlers (approx.) | most of the table; stubs: `playsound`, `playmidi`, `fill_screen`, bow charge |
| Missing from table → `dinkc unimplemented` | leftover names unused or title-only (`set_mode`, `load_sound`, `math_*`, …) |
| Used in official `Story/` | ~125 of 186 |
| Missing/stub **and** used in campaign | ~39–54 distinct names |

Dinkcast-only table names (not FreeDink bindings): `stop`, `choice_start`, `choice_end`, `external`, `update_status`. FreeDink `external` lives in `process_line()`, not `DCBD_ADD`.

### VM / interpreter gaps (not just `k_fn`)

| Gap | FreeDink | Dinkcast | Campaign |
|---|---|---|---|
| `goto` / labels | `locate_goto` in `process_line` | Host #102 | Flycast gossip/shop still a picture |
| `spawn` | `dc_spawn` | Host #104 | Sprite 1000; parent continues |
| `screenlock` | `dc_screenlock` | Host #106 | 0/1 set; `get_hard` clamp; edge will not wrap |
| `kill_this_task` | Resume `proc_return` | `fiber_kill` | Mostly OK; nested parent resume unverified |
| `external` | `process_line` | `bind_external` + `WAIT_EXT` | Loot (`MAKE.c` / `EMAKE.c`) — village smash depends on this |
| Same-file `void foo()` | `locate` + `run_script` | Fiber starts at one proc | Possible silent skip |
| `make_global_int` | Binding | VM opcode | `MAIN.c` OK |
| Say `&var` substitution | `decipher_string` | Unverified in saybox | Dialogue may show `&life` literally |
| `script_attach` | `dc_script_attach` | Return arg + VM `f->sprite =` | Partial |

### Commands used in campaign and missing or stubbed

**Landed since the v0.1 pass:** `save_game` / `load_game` / `game_exist`, `say_xy`, `free_items`, (this PR) `say_stop_xy`, `sp_frame_delay`, `count_item` / `count_magic`, `kill_this_item` / `kill_cur_item`.

**Still missing (used in 1.08, not this PR):**  
`set_mode`, `reset_timer`, `set_dink_speed`, `load_sound`, `get_version`, `sp_noclip`, `sp_reverse`, `sp_sound`, `dink_can_walk_off_screen`, `stopmidi`, `compare_sprite_script`, `run_script_by_number`, …

**In table, not FreeDink-complete:**  
`playsound`, `playmidi`, `fill_screen`, `activate_bow` (no charge loop), `kill_shadow`, `sp_kill_wait`, `sp_attack_hit_sound*`, `initfont`, `wait_for_button` (yield, no pad).

**0 stock hits (defer):**  
`copy_bmp_to_screen`, `wait_for_button`, `callback_kill`, `math_*`, `get_date_*`, `sp_clip_*`, `sp_gold`, `make_global_function`, `set_keep_mouse`, most editor/truecolor helpers.

---

## Brains 0–17

Dispatch: FreeDink `update_frame.cpp`; Dinkcast `brain_switch()` + player in `player_step` / `main.c`.

| ID | Name | Status | Notes |
|---|---|---|---|
| 0 | none | Aligned | `process_follow` grafted |
| 1 | human | Mixed | Walk/push/talk/USE/held magic/inventory OK. Missing: diagonal slide, `sp_base_idle` on idle seq, player hurt floater/sfx, freeze+talk hurry text. Bow charge later. Brains 13/14 N/A |
| 2 | bounce | Aligned | Bounds use playfield constants vs `getpic` |
| 3 | duck | DIE aligned | Follow grafted; no idle SFX; no dead-duck blood drip |
| 4 | pig | Aligned | SFX nit |
| 5 | one_time bake | Aligned | `bg_baked` → type 0 (smash under Dink confirmed 2026-08-22) |
| 6 | repeat | Aligned | editor `sp_index` only; create_sprite food stays `pseq`/`pframe` |
| 7 | one_time stay | Aligned | `hidden=1` so snapshot does not respawn |
| 8 | text | Mixed | Say is `saybox_*`. Hit numbers OK. Kill exp is `&exp` only (no +N floater) |
| 9 | pill | Mixed | `process_target` + ATTACK locate; brain lookup this PR |
| 10 | dragon | Mixed | Follow + ATTACK wait locate; no pill-style walk-in |
| 11 | missile | Gap | Hit + `last_hit` OK. No hard>100 HIT script, sfx, blood, `strength==1` roll |
| 12 | scale | Aligned | Pickup shrink |
| 13 | mouse | Deferred | Pad port; log unimplemented |
| 14 | button | Deferred | Title leftover |
| 15 | shadow | Aligned | |
| 16 | people | Aligned | Follow grafted |
| 17 | missile expire | Gap | Same as 11 + no seq repeat |

`brains_create` now defaults `hard=1` like `add_sprite_dumb` (`#98`). Slot search 2..99 skipping active editor is a Dinkcast layout choice (slot 1 is not Dink).

---

## Engine systems vs plan table

| Plan row | Status | Campaign note |
|---|---|---|
| screenlock | Host #106 | Arenas; no lock-bar gfx |
| indoor / last_map | This PR | Outdoor screens set `last_map`; map HUD seq 165 |
| fade | Graft | Truecolor `CyclePalette` / `up_cycle` (400 ms); `fade_down` yields 1000 ms |
| force_vision | Graft | `dc_force_vision` → sprite 1000 + `fill_whole_hard` + `draw_screen_game` |
| editor_type 1 / 3 | OK | Types 6/7/8 `last_time` this PR (`fix_dead_sprites`) |
| play.spmap save | VMU | Bite 17 v0.3.0 |
| warp parm_seq | OK | |
| dinfo life 0 | OK | `load_game` on START Continue |
| push | OK | |
| show_bmp / button6 | This PR | Dot uses `last_map` |
| inventory DinkC extras | This PR | `count_item` / `kill_*`; `free_items` #109 |
| HUD / magic meter | OK | |
| playsound / playmidi | Stub | Bite 12 |
| VMU | v0.3.0 | Flycast stamp; hardware/ODE pending |
| 14.6 dir.ff | Pending | RAM, not first script hole |
| hardness 2 flying | Partial | Dink passes `h==2`; screenlock #106 |
| get_box full clip | Partial | Skip fully off-screen only |

---

## Quest scripts (illustrative)

| Arc | Scripts | First holes |
|---|---|---|
| Ethel / Quackers | `S1-H2-O.c`, `S1-DUCK.c`, `FINDDUCK.c`, `S1-LG.c` | Host `sp_follow`; Flycast `goto` gossip still a picture |
| Holes / letter | `S1-HOLE.c`, `S1-HOLE3.c`, `S1-LTR.c` | Host load/draw; letter fade_up confirmed; holes still a picture |
| Milder / bar | `S2-MAN2.c`, `S2-OUT.c` | Host `goto`; Flycast shop loop still a picture |
| Castle | `SC-*.c`, `KING.c` | Host screenlock/brain lookup; `say_stop_xy` this PR |
| Dragons / end | `EN-DRAG.c`, `S5-END.c`, `s7-boss.c` | Host spawn/target; `sp_frame_delay` this PR |
| Islands | `S6-WARP.c`, `S8-DA.c`, `S6-VEND.c` | `count_magic` this PR |
| Save / death | `ESCAPE.c`, `SAVEBOT.c`, `DINFO.c`, `START-2.c` | VMU v0.3.0 |

Engine boot files still in data (`MAIN.c`, `START*.c`, `BUTTON6.c`) — this port does not run the PC title brain the same way. Treat `set_mode` / `load_sound` / `sp_noclip` as title leftovers unless a script is actually attached.

---

## PLAYTEST vs this audit

Village Open is **empty** (requester 2026-08-22): 409 house, smash y-sort, and pig-pen fence are confirmed. Those were occupancy/paint, not DinkC holes.

Campaign-hard leftovers after this remainder: **audio 12** and **14.6**. Flycast still Open: holes, gossip/shop `goto`, Quackers follow, screenlock arena.

---

## Out of scope

D-Mod loader, DinkEdit, keyboard/mouse brains, `wait_for_button` (0 story hits), truecolor/PC paths, CD-DA `playmidi` tracks (plan: MIDI id table in bite 12).

---

## Sources

- `/home/benh/Source/dinkcast/src/dinkc_cmd.c` (`k_fn[]`, `fill_screen` stub)
- `/home/benh/Source/dinkcast/src/dinkc_vm.c` (`run_fiber`, `locate_goto` #102)
- `/home/benh/Source/dinkcast/src/brains.c`
- `/home/benh/Source/gnu_freedink/src/dinkc_bindings.cpp` (`dinkc_bindings_init`)
- `/home/benh/Source/gnu_freedink/src/update_frame.cpp`
- `/home/benh/Source/freedink-data-1.08.20190120/dink/Story/`
- Plan § Official campaign systems; `docs/FREEDINK-ALIGN.md` (stale in places; this file wins until that table is updated)
