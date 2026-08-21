# FreeDink alignment

**Rule:** Dinkcast **grafts GNU FreeDink**. If a behavior exists in that tree, copy it. Do not invent a simpler box, filter, or move test. Dreamcast-only exceptions: PVR lists/twiddle, ISO 8.3, Maple, no SDL blit.

Cite the FreeDink function in the PR when you touch that path. If you cannot name it, stop and look it up.

Source: GNU FreeDink `master` (`gitGNU/gnu_freedink`). Official freeware data via `DINK_DATA`.

## Current play (through 9.3 / 8.6)

| Item | FreeDink | Dinkcast | Status |
|---|---|---|---|
| `dink.dat` 20+769×3 i32+2240 | `EditorMap::load` | `world_parse_mem` | hold |
| `map.dat` 31280, tile 80 B, sprite 220 B | `load_screen_to` | `map_parse_mem` | hold; screen `script` **30240** |
| `vision` +188, `hard` +120 (`0`=solid), `que` +116 | `screen_rank_*` | sort by `que?:y`; **draw at map x,y** | hold |
| Type 0 draw, 1 live, 2 hard-only | `game_place_sprites` | `editor_sprite_draw` | hold |
| `SET_SPRITE_INFO` last-wins | `program_idata` | `ini_store_frame` | hold (400 unique; official 207) |
| Default cx/cy/hardbox | `load_sprites` | `ini_frame_geom` | hold |
| `dir.ff` u32+13 name | `FastFileInit` | `ff_parse_mem` | hold |
| `hard.dat` 800×51×51 + `btile_default` | `load_hard` / `realhard` | `hard_sample` / `hard_id_for_tile` | hold |
| Tile `idx/128`, `%128`, 12×8, playl=20 | `gfx_tiles` / `fill_whole_hard` | `tile_split` / `tiles_draw` | hold |
| Stamp exclusive `[l,r)×[t,b)`, `xx-20` | `add_hardness` | `hard_stamp_box` | hold |
| Move 1 px, `get_hard(x-20,y)` | `move` / `check_if_move_is_legal` | `player_step` / `hard_get` | hold |
| Diag `speed-speed/3` | `changedir` | `player_step` steps | hold |
| Start map 1, 334,161, dir 4 | `MAIN.c` / `starting_dink_*` / `START-1.c` | `start_map.h` | hold |
| Idle `10+dir`, walk `70+dir` | `base_idle` / `base_walk` | `DINK_BASE_*` | hold |
| Talk A | `run_through_tag_list_talk` | `talk_probe` | hold (10.1) |
| Hit B | `item-fst` + `run_through_tag_list` | `player_attack` + `hit_tag_list` | hold (15.2); `hit_probe` geometry only |
| Talk/hit/main hooks | `locate` + `run_script` | `script_on_*` log stubs | hold (10.3); run in 11 |
| `story/name.c` | `load_script` | `dinkc_load` 32 KB | hold (11.0) |
| DinkC tokens | line + `get_word` | `dinkc_lex_*` | hold (11.1); `//` only |
| DinkC shape | `process_line` / `void` / `if` | `dinkc_parse` | hold (11.2); no run |
| DinkC fibers | `run_script` + `wait` callback | `dinkc_vm_*` | hold (11.3) |
| Attach on enter | `draw_screen_game` + `game_place_sprites` + `game_screen_init_scripts` | `script_attach_screen` | hold (11.6); type 1, `strlen>1`, rank |
| Vars 1.08 | `lookup_var_local_global_108` | `dinkc_var_*` | hold (11.4); MAIN.c + engine specials |
| Wave 1 cmds | `dinkc_bindings` | `dinkc_cmd` | hold (11.5); say=serial; freeze spr[1]; live sprite leftover → **11.10** |
| Choice lines | `dinkc_get_choices` | `choice_start` + `choice_ret[]` | hold (11.7); A = first visible; `&result` = official # |
| Wave 2 cmds | `add_item` / `hurt` / `sp_hitpoints` | `dinkc_cmd` | hold (11.7); no inv UI; `playsound` stub |
| Wave 3 cmds | `playmidi` / `draw_status` / `compare_weapon` | `dinkc_cmd` | hold (11.8); midi/status stub; no fade/screen |
| Cmd table | `dinkc_bindings` hash | `k_fn[]` + `DINKC_DUMP_FNS` | hold (11.9) |
| `SET_FRAME_SPECIAL` | `seq[].special[]` | `ini_frame_special` | hold |
| `freeze` nest | `spr[].freeze` | `Player.freeze`; A hit `++` | hold; unfreeze 11.3 |
| `&vision` starts 0 | `draw_screen_game` `*pvision=0` then screen MAIN | `script_enter_vision` | hold (11.6); then place with `&vision` |
| ini commands case-insensitive | `compare()` | `tolower` cmd | hold |

## House leftovers (not blocking 10.x)

| Item | FreeDink | When |
|---|---|---|
| Idle after diagonal | `human_brain` 1/3→2, 7/9→8 | hold (#28) |
| Full `get_box` clip | crop to playl…playx, 0…playy | first sprite that bleeds |
| `spr[].alt` trim | `get_box` if left\|\|top\|\|right | hold (pig fence stubs) |
| `SET_FRAME_FRAME` / `SET_FRAME_DELAY` | `program_idata` | hold (idle ping-pong 4→5=3, 6=2) |
| NOTANIM copy frame 1 | `load_sprites` | frame without SSI |
| Hardness 1 vs 2 | `screen_hitmap` | flying / combat |

## Official campaign (no D-Mods)

Canon table: plan **Official campaign systems**. Out of scope: D-Mod loader, editor, D-Mod-only DinkC.

| Item | FreeDink | Bite |
|---|---|---|
| Talk / hit | `run_through_tag_list_talk`, hit list | **10.1 talk**; **10.2 hit** |
| DinkC + attach + yields | `dinkc*`, `game_screen_init_scripts` | 11 |
| `&vision` / `force_vision` | `draw_screen_game` | 11 |
| Engine + `MAIN.c` globals | `attach()` | 11.4 |
| `freeze` nest | `spr[].freeze` | 11.3 |
| SFX + MIDI stream | `sfx`, `bgm` | **12 after 16** (stub until then) |
| Say / choice / font | brain 8, `game_choice` | 13 / V5 |
| Font atlas | TTF LiberationSans | embedded IBM VGA 8x8 | hold (13.1); no stock BMP in official data |
| Say box | `say_text` / `text_brain` / `text_draw` | `saybox_*` | hold (13.2); x-75 y-100 wrap 150; `print_text_wrap` hcenter; follow owner; `font_colors` 1–15 |
| Choice menu | `game_choice` / `game_choice_renderer` | seq 30 frames 2–4 + hcenter 184–463 + arrows 456/457 | hold (13.3); D-pad + A; `&result` official # |
| Screen edge + warp + `screenlock` | `did_player_cross_screen`, `special_block` | hold (14.1–14.2); no fade / screenlock; `parm_seq` wait; **14.3** 20-crossing `mem_log` / `swap_ms` (this PR) |
| `play.spmap` editor_type | `fix_dead_sprites` | 14 + 17 |
| Brains 0–17 (stock names) | `update_frame` | hold (15.1); all ids; then **11.10**; damage/DIE **15.2** |
| Live sprite DinkC | `move` / `create_sprite` / `sp_kill` / NPC `sp_x` | hold (11.10); skip active editor; keep MAIN creates |
| Push / death | `hurt_thing`, `add_kill_sprite`, `human_brain` push, `dinfo` DIE | hold (15.2) |
| Weapons / magic | `add_item`, `dc_arm_weapon`, `human_brain` USE | hold (15.3–15.4); sprite 1000 keep; `init` seq rewrite; bow charge instant 100 |
| Touch / pickup | `run_through_touch_damage_list` | hold (16.1); `-1` → `TOUCH`; `editor_type` 1 |
| Talk/magic miss say | `human_brain` `say_text` | hold (10.1); 6 talk + 6 magic lines; no `dnotalk` file |
| Inv / HUD / map bmp | `status`, `process_show_bmp` | 16.2 / 16.3 / V6 |
| VMU save | `savegame` | 17 |

## How to add a bite

1. Open the FreeDink function for that behavior.
2. Port the rule (offsets, exclusive ranges, inverted `hard`).
3. Name the function in the PR and, if new, one GOTCHAS bullet.
4. Host test that would fail if we guessed.

Do not mark a bite done because “it looks close.”
