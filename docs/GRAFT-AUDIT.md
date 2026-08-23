# GRAFT-AUDIT.md — FreeDink → Dinkcast faithfulness audit

Static, line-level comparison of GNU FreeDink (`/home/benh/Source/gnu_freedink/src`,
1.08-compatible refactor) against `src/` here. Twelve subsystem passes, every finding
cited on both sides. No emulators were run; this is source analysis only. Findings
already tracked in PROGRESS.md / CAMPAIGN-AUDIT.md are marked **[known]**.

Classification: **missing** (no graft), **diverged** (grafted, logic differs),
**simplified** (deliberate DC adaptation that changes rules), **unverified**.

Severity: **P0** breaks the stock campaign, **P1** breaks D-Mods/edge behavior or
visibly wrong stock behavior, **P2** cosmetic/rare.

## Summary

| # | Subsystem | P0 | P1 | P2 | Headline |
|---|---|---|---|---|---|
| 1 | Movement, collision, hardness | 0 | 5 | 5 | hardness==2 flattened; flying passes walls |
| 2 | Animation & timing | 0 | 4 | 5 | sp_reverse/sp_picfreeze/NPC nocontrol missing |
| 3 | Brains 0–17 | 0 | 6 | 13 | missile subsystem least faithful |
| 4 | Player brain, talk, hit, hurt, bow | 0 | 5 | 5 | bow charge missing [known]; created NPCs untalkable |
| 5 | DinkC interpreter | 0 | 3 | 10 | no user-defined proc calls |
| 6 | DinkC command table | 0 | 15 | ~14 | fill_screen/kill_shadow/sp_kill_wait stubs; say/hurt side effects |
| 7 | Sprite loading, dink.ini, centers | 0 | 5 | 8 | BLACK keyword absent; frame-1 center inheritance |
| 8 | Screen change, warp, vision | 1 | 2 | 6 | editor base_attack/base_idle/base_hit dropped |
| 9 | Sprite lifecycle & manager | 0 | 6 | 4 | cap 100 vs 300; no eviction; sp() identity stub |
| 10 | Text, say, choice | 0 | 3 | 9 | single global saybox vs brain-8 text sprites |
| 11 | Inventory, stats, HUD, save | 0 | 5 | 11 | magic regen ~8× slow; load doesn't re-run main |
| 12 | Main loop ordering | 0 | 4 | 9 | scripts run before brains / during inventory |

Totals: **1 P0, ~58 P1, ~99 P2** (P1/P2 counts overlap slightly where two passes
found the same root cause — dedup noted inline).

## P0

**P0-1. Editor sprite `base_idle`/`base_attack`/`base_hit` never parsed; all
editor-placed monsters get -1.**
FreeDink: `editor_screen.cpp:153-155` parses map.dat sprite offsets +100/+104/+108;
`game_engine.cpp:525-527 game_place_sprites` copies them to the live sprite.
Dinkcast: `src/mapscr.c:97-133` parses +92/+96/+112/+116/+120/+124/+140/+188 but
never +100/+104/+108; `src/brains.c:1631-1632 brains_enter` hardcodes
`base_idle = -1; base_attack = -1` (no base_hit at all).
Consequence: stock map.dat monsters (boncas, pillbugs, slimes…) carry editor-set
attack/idle bases; with -1 they lose attack/idle animations — combat across the
campaign. **Verified by hand during consolidation.**

## P1 findings by subsystem

### 1. Movement, collision, hardness
- **hardness==2 (water/low) flattened to 1.** FreeDink keeps raw 0/1/2 values
  (`live_screen.cpp:142,186`); `freedink.cpp:95-101` lets `flying` pass hardness 2
  and `:111` exempts it from push. Dinkcast `src/hard.c:168 hard_sample` returns a
  boolean; `hard.c:208` stamps literal 1. → Push anim fires against shorelines;
  flying semantics unimplementable. (simplified)
- **Flying sprites bypass ALL hardness.** `freedink.cpp:95` vs `src/brains.c:520`
  (`!s->flying &&` skips the check). Flying enemies/fireballs pass solid walls.
  (diverged)
- **Player has no `flying`/`speed`/`timing` fields.** `sp_flying(1,…)`,
  `sp_speed(1,…)`, `sp_timing(1,…)` are no-ops (`player.h`, `dinkc_cmd.c:794-848`
  fall-through). Walk speed hardcoded `DINK_SPEED 3` (`player.c:253`). (missing)
- **`push_active()` DinkC command missing** (`dinkc_bindings.cpp:1179 dc_push_active`;
  no `k_fn[]` entry). (missing)
- **Live-sprite cap 100, no 300 + `kill_highest_nonlive_sprite` eviction** — see §9.

### 2. Animation & timing
- **`sp_reverse` missing for gameplay sprites** — `live_sprite.cpp:105-144` reverse
  branch; `brains.c:327 brain_animate` is forward-only; no `reverse` field. (missing)
- **`sp_picfreeze` missing** — `live_sprite.cpp:146`; no field/command. (missing)
- **`nocontrol` never cleared on one-shot seq end for NPCs** —
  `live_sprite.cpp:137,186` vs `brains.c:359-367`; `DINKC_SP_NOCONTROL` (24) falls
  through `brains_change_prop` returning -1 silently (also violates "log
  unimplemented DinkC"). (missing)
- **`set_dink_speed` binding absent** (`dinkc_bindings.cpp:569`) — used by stock
  `MAIN.c` and `item-bt.c` (herb boots give no speed boost). (missing) [known]
- Fixed 16 ms timestep vs `base_timing = fps_final/3` is a deliberate 60 Hz
  adaptation; identical math at 60 Hz, dilates game time under load. (simplified, P2)

### 3. Brains (missile cluster is the least faithful area in `src/brains.c`)
- **Flying missiles fly through walls** (same root cause as §1 flying finding):
  `brain_missile.cpp:39` explodes on `hard > 0 && hard != 2`; `brains.c:1258` only
  dies off-screen. (diverged)
- **Missile hit path missing notouch/blood/scripts**: FreeDink sets victim
  `notouch=1`, `notouch_timer=+100`, `target=1`, `&enemy_sprite`/`&missile_target`,
  runs missile `DAMAGE` and victim `HIT` procs, `random_blood(x, y-40)`, and does
  NOT remove the missile (`brain_missile.cpp:113-185`). Dinkcast `brains.c:1276-1311`
  applies damage then `live = 0`. Scripted fireballs/DoT silently dead. (diverged)
- **Missile damage formula**: `strength==1 → strength - defense`; else
  `strength/2 + rand()%(strength/2) + 1 - defense` (`brain_missile.cpp:151-159`) vs
  coin-flip with `half` clamped ≥1 (`brains.c:1294-1302`). (diverged)
- **Missile vs sprite-hardness (>100) script path missing**
  (`brain_missile.cpp:42-85`). (missing)
- **Headless duck (base_walk==110) bleed block missing** (`brain_duck.cpp:124-129`
  vs `brains.c:935-951`) — visible stock behavior. (missing)
- **Dink diagonal slide-around-obstacle probes missing**
  (`brain_keyboard.cpp:567-772`, up to 5 alternate 1–2 px offsets) — Dink snags on
  hard corners FreeDink slides past. (missing)
- Brains 13/14 log-only (`brains.c:1452`): `button_brain` BUTTONON/OFF hardbox logic
  and `run_through_mouse_list` genuinely absent. (missing) [known]

### 4. Player / talk / hit / hurt / bow
- **Bow hold-charge (`process_bow`) entirely missing** [known, sized]:
  `brain_keyboard.cpp:87-149` + `dc_activate_bow` + `update_frame.cpp:235`
  (`bow.hitme` re-arm every 100 ms). Full behavior: script yields, `pseq=100+dir`
  draw anim, `bow.time += 7`/10 ms capped 500, `pframe = time/100+1`, free re-aim,
  release → `bow.last_power`, resume `bow.script`. Dinkcast: `dinkc_cmd.c:1550` sets
  `g_bow_power = 100` synchronously. Arrows always weak; no charge/aim. (missing)
- **Talk probe covers editor rows only** — `run_through_tag_list_talk` iterates all
  live sprites 1..300; `talk.c:49` loops editor `sprite[1..99]`. `create_sprite`d
  NPCs can never be talked to. (diverged)
- **`dnotalk` / `dnomagic` fallback hooks missing**; also no TALK-proc rescan past
  the first in-box sprite (`brain_keyboard.cpp:264-291`, `freedink.cpp:286-304` vs
  `talk.c:49-80`). D-Mod hooks never fire; stock gives silence instead of flavor
  quips in edge cases. (missing/diverged)
- **Unarmed attack punches with no weapon script** — FreeDink requires
  `weapon_script != 0 && base_hit > 0` (`brain_keyboard.cpp:294-302`);
  `main.c:1176-1180` falls back to `player_attack()`. (diverged)
- **`sp_speed(1,…)` dropped** (see §1 player-fields finding). (diverged)

### 5. DinkC interpreter
- **No user-defined proc calls / `make_global_function`** — `dinkc.cpp:2537-2660`
  (same-script proc locate + proc_return + yield; global `play.func[]` fallback) vs
  `dinkc_vm.c:631-633` routing every unknown `ident(` to the command table →
  "dinkc unimplemented". Largest semantic hole; custom `void foo();` calls silently
  skip. (missing)
- **No `kill_scripts_with_inactive_sprites` sweep** (`dinkc.cpp:988-1001`, called
  every frame) — a fiber parked in `wait()` whose sprite dies resumes and operates
  on a dead/recycled slot. (missing)
- **`set_callback_random` resume runs a fresh fiber**, losing owner locals/`&argN`
  (`dinkc.cpp:1058-1062` resumes same script id; `dinkc_vm.c:1055-1061` starts a new
  fiber with zeroed args). (diverged)

### 6. DinkC command table (full 186-binding diff; details in pass notes)
Stock-campaign-used and missing/stubbed:
- `compare_sprite_script` missing — used by `S4-ROCK.c`, `S4-SEC1.c`, `S5-SEC1.c`
  (secrets never trigger).
- `fill_screen` stubbed (`dinkc_cmd.c:1437`) — `S1-LTR.c` letter scene (main quest),
  `S2-BAR.c`, `S2-CAVE2.c`, `S2-MDOOR.c`, `START.c` show live pixels instead of
  black. [known: "instant" per PROGRESS; full impact is visual blackout]
- `kill_shadow` stubbed (same line) — `DAM-A1/FIRE/ICE/SFB.c`; shadow blobs linger
  after explosions.
- `sp_kill_wait` stubbed (`dinkc_cmd.c:1546`) — 10 item scripts; NPCs stuck in
  talk-wait after weapon use.
- `set_dink_speed` missing (§2). `free_magic` missing — `S6-VEND.c` vendor slot
  check. `run_script_by_number`/`is_script_attached` missing — `DAM-FIRE.c`/
  `DAM-SFB.c` burn chains dead. `stopmidi` missing — `DINFO.c` death screen.
- **`sp()`/`sp_editor_num` are identity stubs** — FreeDink `dc_sp` searches live
  sprites by `sp_index`, returns 0 when absent; `dinkc_cmd.c:1561` echoes the
  argument. Dead-sprite quest checks never fire. (also §9)
- **`say` family side effects dropped**: `say_stop_npc` missing already-talking
  guard; `say*` never kill owner's/Dink's prior text, never set `&last_talk`,
  return 1/0 instead of the text sprite id (`dinkc_bindings.cpp:740,758,782` vs
  `dinkc_cmd.c:888-902`). (diverged)
- **Scripted `hurt(&t,N)` doesn't run victim HIT proc / set `&enemy_sprite`/
  `&missle_source`** (`dc_hurt:1533` vs `dinkc_cmd.c:1157`); and `hurt(1,…)` reads
  the `&defense` *var* while `sp_defense(1,x)` writes `g_pl->defense` — the two
  desync (`dinkc_cmd.c:1162-1175`). (diverged)
- **`sp_dir`/`sp_speed` don't graft `changedir`** — no mx/my recompute, no speed
  rescale, no seq fallbacks; Dink path stores dir only, no seq change
  (`dc_sp_dir:255`/`dc_sp_speed:437` vs `brains.c:1872-1892`, `dinkc_cmd.c:800-848`).
  Cutscene `sp_dir(1,…)` turns Dink without changing his art. (diverged)
- **`sp_seq` resets frame (Dink path to 1, NPC path to 0!) and dropped the range
  check** (`dc_sp_seq:409` vs `dinkc_cmd.c:839-841`, `brains.c:1878`). (diverged)
- **`sp_custom` is one global 32-entry table**, not per-sprite; silently no-ops when
  full, never checks sprite active (`dc_sp_custom:1635` vs `dinkc_cmd.c:1671`).
  (simplified)
- `busy()` missing (`dc_busy:1388`) — dialogue pacing loops keyed on "still talking"
  exit early. (missing)
- Audio stubs (`playsound`, `playmidi`, `sp_sound`, `load_sound`, hit-sound cmds) —
  [known, gated bite 12].

### 7. Sprite loading / dink.ini / center math
- **`BLACK` transparency keyword not parsed** — `dinkini.cpp:173-176` +
  `gfx_sprites.cpp:253-262` (colorkey index 255, white kept) vs `src/ini.c:380-405`
  (integer-only) + `sprite.c:41-48` always white-key. Stock dink.ini has 11 BLACK
  seqs (10, 180, 192-197); in-world BLACK sprites get inverted transparency. HUD
  survives only via the status.c special case. **Verified by hand.** (missing)
- **Animated seqs: frames 2..N must inherit frame 1's center** —
  `gfx_sprites.cpp:286-292,299-305` (`oo > 1 && notanim` inherits) vs
  `ini.c:253-275 ini_frame_geom` recomputing per frame from that frame's w/h.
  Variable-size frames (explosions, fireballs) jitter and their hardboxes wobble.
  (diverged)
- **`init()` honors only LOAD_SEQUENCE; SET_* lines in init() silently dropped** —
  `dinkini.cpp:364-412` vs `ini.c:418-441 ini_apply_line` returning 0. (missing)
- **`check_base`/on-demand load replaced by hardcoded per-brain subsets; play-path
  refuses uncached packs** — `dinkini.cpp:423-468` (any seq loadable any time) vs
  `edraw.c:422-452 walk_seqs_for_brain` + `edraw.c:804-814` skip-forever when the
  dir.ff isn't cached. Script-driven mid-screen `sp_seq`/`sp_base_walk`/brain changes
  to evicted packs produce invisible sprites. (diverged; RAM-driven but rule-changing)
- **In-game `sp_noclip` not honored** — noclip draw variant exists but only
  startmenu uses it (`main.c:1451-1454` always clipped). (missing, game path)

### 8. Screen change / warp / vision
- P0-1 above (editor base_* parse).
- **`update_screen_time` semantics**: FreeDink stamps `spmap[].last_time` when
  LEAVING a screen; respawn types 6/7/8 count from departure. Dinkcast stamps only
  when `editor_type(6/7/8)` is set (`dinkc_cmd.c:1606`), never on leave/warp/load;
  not in the save blob. Monsters return too early after a long linger. (diverged)
- **`dink_can_walk_off_screen` / walk_off_screen missing** (`freedink.cpp:137`,
  `dinkc_bindings.cpp:1174`) — ending-sequence walk-off cutscenes trigger unwanted
  screen changes. Stock campaign uses this. (missing)

### 9. Sprite lifecycle & manager
- **Cap 100 vs 300** (`brains.c:84` vs `MAX_SPRITES_AT_ONCE 300`), and **no
  `kill_highest_nonlive_sprite` eviction / `last_sprite_created` high-water mark** —
  `create_sprite` hard-fails at 100 instead of evicting. (diverged+missing)
- **`live`-flag semantics inverted**: FreeDink `live=1` protects say_stop_xy text
  from screen-change kill-all, and all created sprites die on screen change;
  dinkcast `brains_enter` preserves `created` sprites across screens
  (`brains.c:1597-1599`). Stale created sprites leak across screens. (diverged)
- **`sp()`/`find_sprite` identity stub with no active check** (see §6). (diverged)
- **`kill_text_owned_by` / `does_sprite_have_text` missing** (see §10). (missing)
- **Single global saybox vs brain-8 text sprites** (see §10). (simplified)
- Unverified: brain kill paths don't obviously kill the sprite's attached script
  fiber (`kill_sprite` `brains.c:321` doesn't touch the VM) — zombie-script-on-
  recycled-slot risk; ties into §5 M3.

### 10. Text / say / choice
- **Text is one global saybox, not per-sprite brain-8 sprites** —
  `text.cpp:add_text_sprite` (many concurrent, `text_owner`, `kill_ttl`,
  `say_stop_callback`) vs `saybox.c:21-24` single box; a second `say()` clobbers the
  first. Crowd scenes drop lines. Root cause of §6 say-side-effect findings.
  (simplified)
- **Choice paging/height from fixed-cell metrics** — FreeDink
  `process_text_for_wrapping` measures real glyph widths and breaks overlong words;
  `saybox.c:157-186`/`choice.c:15-47` use `chars × advance('A')` and don't insert a
  break for overlong words (draw past the 150 px box). Choice page splits land at
  different points than FreeDink. (simplified)
- `stop_entire_game` missing (used by `LRAISE.c`). (missing)

### 11. Inventory / stats / HUD / save
- **Magic recharge ~8× too slow**: FreeDink adds `*pmagic` to `*pmagic_level` every
  frame (~84/s) (`status.cpp:460-464`); `status.c:534,623-629` throttles to 100 ms
  (10/s). Combat pacing shifts campaign-wide. (diverged)
- **`kill_cur_magic`/`kill_this_magic` not implemented** (not even in `k_fn[]`).
  (missing)
- **`ARMMOVIE` never run on arm** (`inventory.cpp:356-359,423-426` vs
  `script.c:254`, `dinkc_cmd.c:1395-1436`). (missing)
- **Save load never re-runs `main.c main()` + discards `base_walk`** —
  `savegame.cpp:286-293,141` vs `save.c:338 (void)bw;` + `dinkc_cmd.c:1717-1739`.
  D-Mods setting base graphics/init in main() break; Dink can reload with wrong
  walk anims. (missing/diverged)
- **No custom map/palette/tileset state in save** (v1.08 saves map.dat/dink.dat
  paths, palette, 41 tile slots; `savegame.cpp:226-276`). Mid-game `load_tile`/
  palette swaps lost across load. (missing)

### 12. Main loop ordering
- **Scripts run BEFORE brains and draw; FreeDink runs callbacks/kill-sweep AFTER
  the sprite pass** (`update_frame.cpp:409-410` vs `main.c:1207-1216`). Resumed
  scripts see pre-brain state; end-of-frame callback timing differs. (diverged)
- **VM/saybox/scripts keep running while inventory or map overlay is open** —
  FreeDink `update_frame.cpp:244-247` early-returns the whole frame for
  `show_inventory`; dinkcast gates only `brains_tick` (`main.c:1213`). **Verified by
  hand.** `wait()`s expire and say text auto-advances behind the open inventory.
  (diverged)
- **No `bow.hitme` 100 ms re-arm** (`update_frame.cpp:235`) — part of the bow gap.
  (missing)
- **TTL expiry drops `say_stop_callback` and there is no per-frame
  kill-scripts-with-inactive-sprites sweep** (see §5 M3). (missing)
- `process_animated_tiles` missing — animated fire/water tiles render frozen.
  (missing, P2)

## Verified faithful (highlights)

The audit also confirmed large faithful areas — the accepted V-gates are consistent
with this:

- map.dat/dink.dat/hard.dat/dir.ff binary layouts, `realhard` indirection, vision
  filter, warp trigger rule, `update_play_changes` type table, `fix_dead_sprites`
  timers (incl. type-6 clock wrap).
- `changedir` full table incl. seq fallbacks and diagonal `speed − speed/3`;
  `add_kill_sprite` corpse path; `autoreverse(_diag)`; `get_distance_and_dir_
  nosmooth`; `process_follow`/`process_target`; people/dragon/scale/shadow/pill/
  pig/repeat brain cores; `random_blood`.
- Hurt roll, hit-box inflation geometry (incl. inverted-reach convention and the
  dir-8 no-op quirk), special-frame melee gating, punch formula, talk geometry
  (50/35, brain-8 exclusion), push state machine, touch-damage list, `get_hard_play`
  quirk set.
- DinkC: 1.08 local-then-global scope, choice `&result` numbering, goto/labels,
  if/else skipping, `external`, `spawn`, wait/say_stop resume structure. (`while`
  confirmed absent in FreeDink too — non-issue.)
- `inside_box` reproduced bug-compatibly (crossed right/top args); TTL math; font
  color escapes + palette; choice geometry/paging math; inventory geometry/
  navigation; `next_raise`; draw_bar; stat-scroll bookkeeping; HUD digit seqs and
  positions; kill_ttl boundary; sp_timing throttle.

## Recommended fix order (future bites — requester's go required)

1. **P0-1** — parse +100/+104/+108 in `mapscr.c`, carry through `brains_enter`.
   Small, self-contained.
2. **Hardness==2 + flying class** (§1 ×2, §3 missile-wall) — one fix in `hard.c`/
   `brains.c` restores water/push/flying/missile rules together.
3. **Missile hit contract** (§3) — notouch/blood/HIT/DAMAGE/`&missile_target`;
   revives the stock fireball/arrow script chain.
4. **DinkC proc calls + dead-sprite script sweep + callback-fiber locals** (§5) —
   one interpreter bite.
5. **Command stubs the campaign hits** (§6): `fill_screen`, `kill_shadow`,
   `sp_kill_wait`, `compare_sprite_script`, `set_dink_speed`, `sp()` real lookup,
   `hurt` HIT-proc path, `sp_dir`/`sp_seq` changedir graft.
6. **Magic regen / status throttle** (§11 #1, #8, #9) — one-line class fix in
   `status.c` cadence.
7. **Bow charge** (§4 F1 + §12 F7) — needs input + yield design; [known] gap.
8. **Center inheritance + BLACK keyword** (§7 F1/F2) — one ini/sprite bite.
9. Then the P2 tail, grouped by module.

Nothing here is started — per AGENTS.md, engine bites wait for the requester.
