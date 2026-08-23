# Playtest pictures

Human-confirmed Flycast/hardware pictures. Host tests in `tests/test_playtest.c` lock the engine path so these cannot silently regress. A green `make host` is **not** a visual accept for anything still in **Open**.

Add a **Confirmed** row only when the requester has seen the picture and said it works. Add a host gate in the same PR.

## Confirmed

| Picture | When | Host lock |
|---|---|---|
| Blood spray (seq 187–189) and hit-point numbers when punching pigs | 2026-08-21 | `test_playtest` pig punch |
| Grain USE no longer spams / hitch | 2026-08-21 | `test_distill` s1-sack → 430/431 |
| Grain sprite + spreading (seq 430/431) on the first outdoor screen | 2026-08-21 | `test_distill` + PVR upload after `preload_seq` |
| Feeding the pigs (map 407 box) and Milder dialogue | 2026-08-21 | (Flycast; no host lock yet) |
| Pigs visible on the **first** visit north from the first outdoor screen | 2026-08-21 | `test_edraw` house → 439 → 407 seq 41 |
| Dink left/right walk with grain equipped | 2026-08-21 | `test_ini` item-pig seq 74 center |
| Barrel smash (including first outdoor screen) | 2026-08-21 | `test_edraw` seq 173 smash |
| Pigs stay gone after kill (no snap respawn) | 2026-08-21 | `test_playtest` pig 7 after seq 164 |
| Milder feed no longer spams / hitch | 2026-08-21 | `preload_seq` frame 1 (not full walk) |
| Village-entrance guard (map 408 vis 1, seq 293) with Chealse | 2026-08-21 | `test_edraw` 408 vis 1 seq 293 |
| Ethel “I’ll find Quackers” unfreezes | 2026-08-21 | `test_dinkc_vm` extra `}` |
| Old man sprite in his house | 2026-08-21 | (Flycast; no host lock yet) |
| Start-house textures after walking back | 2026-08-21 | (Flycast; no host lock yet) |
| Village-entrance guard punch (`s1-gg` `hit()`) | 2026-08-21 | `test_script` / `test_dinkc_vm` locate `hit` |
| `say()` lines expire (Ethel “Why hello, Dink.”, X “I'm no wizard!”) | 2026-08-21 | `test_saybox` TEXT_MIN |
| Smashed barrel leftover hardness gone (heart barrels / `BAR-SH`) | 2026-08-22 | `test_brains` created hard; `test_playtest` baked hard |
| Ethel outdoor house (map 409 seq 63) on first visit from 408 | 2026-08-22 | `test_edraw` 408→409 seq 63; `test_mem` make_room |
| Dink draws over smashed barrels | 2026-08-22 | `test_playtest` barrel type 0 |
| Pig-pen south fence joint does not flip-flop (map 407 seq 93 slots 10/22) | 2026-08-22 | `test_world` editor_draw_behind |
| Status-bar exp / gold / level digits keep their white paper | 2026-08-22 | `test_status` glyph + chrome opaque; level 442 white keyed |
| Wizard idle/walk stays on screen for the whole `s1-wiz` meeting (map 376) | 2026-08-22 | `test_edraw` 376 + 167 unused + 563/6 |
| Walk into a fallen AlkNut and pick it up (`s1-nut` / `item-nut`) | 2026-08-22 | `test_inv` `free_items` + s1-nut path |
| Punch map 408 editor-1 `bar-sh`: only the barrel smashes (Dink stays punch) | 2026-08-22 | `test_dinkc_vm` editor 1 `sp_seq` |
| Wizard screen pathway/trees after Load + village (map 376) | 2026-08-23 | `test_mem` ts41 over cap (Prev dir.ff drop; Prev ts01 survives). `test_edraw` 408→376 seq 32 + atlas is not the cap lock |
| Exit the burning start-house after `s1-h1-s` (`&story` 3→4) | 2026-08-23 | `test_player` get_hard_play + frozen move onto warp |
| Map 439 fire-crowd (Ethel, girl, duck, peasant, girl2) then burned house, crowd gone | 2026-08-23 | `test_dinkc_vm` `force_vision` fill_whole_hard then sprite 1000 + `draw_screen`; `test_player` tiles wipe drops stamped box; `test_edraw` 439 vis 1 unused fire then `load_frame` 221 |
| Outdoor fire ends / crowd leaves with `fade_down` / `fade_up` (`S1-H1-O`) | 2026-08-23 | `test_dinkc_vm` fade_down 1000 ms yield + 400 ms to black; fade_up 400 ms; S1-H1-O wait/force_vision in between |

## Open

| Picture | Host lock |
|---|---|
| Burned start-house: leftover pie-table hardness after the fire | later — #117 skip-without-pixels did not clear Flycast. Official vis-0 table (editor 22, seq 87/9). Do not pin 87. |
| Indoor fire scene hitch (SCIF off still slow) | later |
| START Continue / SAVEBOT slot choice: Down walks Dink | (later; `player_step` runs during `dinkc_vm_waiting_choice`) |

Do not mark new pictures confirmed until the requester says so. Name them here when they report them.

## Log notes (2026-08-23, confirmed)

408→376 after Load: `tiles slurp ok tiles/ts41.bmp` then `swap atlas ok`. `edraw load seq=32` frames 1/9/10. No `tiles slurp fail` / `atlas fail keep`. Requester stamped the path.

Burning-house exit + 439 crowd: requester “The crowd showed up this time.” Host locks already on those rows.

Pie-table hardness: still there in Flycast after #117 type 0/1 skip-without-pixels. Come back later. Official vis-0 table; do not pin seq 87.

Indoor fire hitch: still noticeable with `make emu-fast` (SCIF off). Come back later. Not serial. See log notes below.

Fade: requester “Fade works as expected.” Host lock already on that row.

### Indoor fire hitch (`s1-h1-s` map 1 vis 1)

Not SCIF. Same **14.4c** class as the wizard walk ping-pong, louder.

Vision 1 is **on top of vis 0**. Intact furniture still draws. Added looping **brain 6** (`repeat_brain`): five `fire1-` **427**, two `explo-` **70**, two `atomc-` **161**, plus small `fire2/3/4` **155/156/157**. Official slots 21, 27–33, 35, 36, 38, 41.

Enter-path `edraw_load_screen` queues **every** brain-6 frame (`need_push` 1..nfr). Seq **161** is 26 BMPs, each ~91×140 padded to **128×256** ARGB1555 = **64 KB**. All 26 = **1.70 MB** of the **2.00 MB** `cpu_pixels` cap before walls, 15 explo frames (~467 KB), or six fire1 (~192 KB). Enter refuses. Play-path then remakes Screen `live` from **this tick’s** draw frames only, so unused 161/70/427 become evictable.

`repeat_brain` + `brain_animate`: 161 delay **50 ms** (two sprites, phases 4 and 14), 70 delay **40 ms**, 427 delay **75 ms**. Each advance `ensure_frame` misses, evicts an unused Screen frame, **re-decodes the BMP from the cached pack**, `sprite_upload_pvr`. A prior Flycast log: **747** `mem refuse` / **745** Screen evict (almost all 161) / **1540** loads (**1421** of 161) on that enter. Pictures work because evict+retry succeeds; the hitch is decode+PVR every tick.

Do **not** pin seq 161 / 70 / 427. Current-only fire pixels would fit (two 161 + a handful of 427/70 ≈ 360 KB plus vis-0 house). The enter “load the whole loop” pass is what fills the cap and turns the loop into a per-frame evict. Later: 14.4c should treat a playing brain-6 seq as current-frame (or current+next), not `nfr` at enter.

## Log notes (2026-08-22, not confirmed)

Read `build/emu.log` first. These are named FreeDink paths, not a new HUD/white-quad theory.

### Wizard idle missing (`s1-magic` / `s1-wiz`, map 376 loc 277)

`s1-magic.c` `preload_seq` 561/563/567/569/167 then `create_sprite(78, 319, 0, 563, 1)` + `sp_base_walk(560)` + `sp_script("s1-wiz")`. The gnome is **brain 0**, not a forgotten editor sprite. `s1-wiz.c` walks him, flickers `sp_pseq` 561/563, then `move_stop` around the screen.

`preload_seq` decoded **frame 1** of each (same policy as Milder). Play-path then loaded explode **167** frames 2–12 and idle **563** 2–5. At 167 fr=13:

`mem refuse pool=cpu_pixels need=2187264 have=2056192 cap=2097152`

Then **563** frames 6–10, **561** fr=10, and walk **567** frames 2–7 all refuse. Draw skips `edraw_find == NULL` (`main.c`), so the gnome vanishes on those frames. There is **no** `edraw evict` on those lines: play-path `ensure` only evicts unused Screen (`edraw.c` `unused_only`). Every `load_one` stamps `live=1` and that bit is not cleared until the next swap, so explode + half-idle + Always fill the 2 MB cap and later idle/walk frames cannot enter. Do **not** pin seq 563/167 or grow a seq-id victim list. This is the named **14.4c** working-set: Screen frames that are no longer the current draw frame must be evictable (or explode 167 must not stay `live` after brain 7 finishes). Host lock: `test_edraw` remakes Screen live then `ensure` 563/6.

Same log: `s1-wiz` `main()` ran **twice** (two `create_sprite` 167 at 359,241). First attach was during `s1-magic` `sp_script`; `draw_screen` attached again. That is a second-order script-lifetime issue, not the idle holes.

### AlkNuts “I'm full!” (`s1-ntree` / `s1-nut`)

Tree `hit()` created nuts (`create_sprite` brain 0 seq **421** + `sp_script("s1-nut")`). Touch ran `s1-nut.c` `touch()`:

```
int &junk = free_items();
if (&junk < 1) { say("I'm full! …"); return; }
add_item("item-nut", 438, 19);
```

Log: `dinkc unimplemented free_items` then the full line, many times. Unimplemented commands return **0** (`eval_prim`). Inventory at that point was fists + grain (slots 1–2 of **16**). `add_item` is implemented; the script never reaches it.

FreeDink `dc_free_items` (`dinkc_bindings.cpp`) is a count of inactive `play.item[0..NB_ITEMS)` (`NB_ITEMS` 16). Grafted. Host lock: `test_inv`. `free_magic` is the 8-slot analog (not this pickup).

`scripts_used` is also unimplemented (`s1-ntree` `hit` cap at 170). It returned 0 so nuts still spawned. Adjacent, not the pickup fail.

Pickup confirmed in Flycast 2026-08-22.

### Burning start-house: log spam + no exit (`s1-h1-o` / `s1-h1-s`, map 1 vis 1)

Outdoor `&story == 3` is `S1-H1-O.c`: `&vision = 1`, freeze, “What, the house, mother nooooo!!!”, `unfreeze(1)`, `kill_this_task`. Warp in: `warp ed=13 map=1 xy=323,390` then `load_screen map 1 loc 1`. Indoor `&story == 3` is `S1-H1-S.c`: `freeze(1)`, `&vision = 1`, `say_stop`s, `&story = 4`, `move_stop` toward the door (`2, 398`, `nohard=0`), **`return` with no `unfreeze(1)`**. Outdoor `&story == 4` is the crowd (`force_vision(2)`, fade, `unfreeze`). This session never left map 1.

This log (after the fire enter): `say Mother noooooo!` … `say Ahh, too much smoke .... gotta get out ...`. No later `unfreeze 1`, `freeze orphan`, or `warp ed=`. Pad disconnect ended the session still on the fire screen.

**Spam (named, not a missing BMP).** Play-path `ensure` of **seq 161** (`graphics/effects/atomic/atomc-`, 26 frames) plus fire **157** / explo **70** at the 2 MB `cpu_pixels` cap: `mem refuse` then `edraw evict class=screen` then load, **every tick**, many looping sprites. After that enter: **747** refuse / **745** Screen evict (almost all seq 161) / **1540** loads (**1421** of 161). Pictures worked because evict+retry succeeded. Same class as the wizard walk ping-pong, much louder. Do **not** pin 161 or grow a seq-id victim list.

**Exit + crowd + fade — Flycast confirmed 2026-08-23.** `S1-H1-S.c` never thaws Dink. FreeDink `dc_freeze` stores **that script id** on `spr[1]`. Exit is `move_stop` onto the type-2 door warp while still frozen. `get_hard_play` treats `is_warp` hardness as **0** and records `warp_editor_sprite`. `special_block` has **no freeze check**. Host: `test_player`.

Outdoor `&story == 4` creates the crowd then `force_vision(2)`. That is `dc_force_vision` (sprite **1000** + `draw_screen_game`), not a `&vision` write. Enter-path remakes Screen live before `create_sprite` `load_frame` so unused fire frames evict (14.4c; no seq-id pin). Host: `test_dinkc_vm`, `test_edraw`.

`dinkc_cmd_thaw_if_idle` only clears freeze when `dinkc_vm_live() == 0`. Screen + furniture keep fibers stay live after `return`. Do **not** invent a door, `unfreeze` on `return`, or a fire seq-id victim list.
