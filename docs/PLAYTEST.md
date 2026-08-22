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

## Open

| Picture | Host lock |
|---|---|
| Wizard idle/walk stays on screen for the whole `s1-wiz` meeting (map 376) | `test_edraw` 376 + 167 unused + 563/6 |
| Walk into a fallen AlkNut and pick it up (`s1-nut` / `item-nut`) | (none yet — log named `dinkc unimplemented free_items`) |

Village leftovers above were confirmed 2026-08-22.

Do not mark new pictures confirmed until the requester says so. Name them here when they report them.

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

FreeDink `dc_free_items` (`dinkc_bindings.cpp`) is a count of inactive `play.item[0..NB_ITEMS)` (`NB_ITEMS` 16). Graft that. `free_magic` is the 8-slot analog (not this pickup).

`scripts_used` is also unimplemented (`s1-ntree` `hit` cap at 170). It returned 0 so nuts still spawned. Adjacent, not the pickup fail.
