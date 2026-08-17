# FreeDink alignment

**Rule:** Dinkcast **grafts GNU FreeDink**. If a behavior exists in that tree, copy it. Do not invent a simpler box, filter, or move test. Dreamcast-only exceptions: PVR lists/twiddle, ISO 8.3, Maple, no SDL blit.

Cite the FreeDink function in the PR when you touch that path. If you cannot name it, stop and look it up.

Source: GNU FreeDink `master` (`gitGNU/gnu_freedink`). Official freeware data via `DINK_DATA`.

## Current play (through 9.3 / 8.6)

| Item | FreeDink | Dinkcast | Status |
|---|---|---|---|
| `dink.dat` 20+769×3 i32+2240 | `EditorMap::load` | `world_parse_mem` | hold |
| `map.dat` 31280, tile 80 B, sprite 220 B | `load_screen_to` | `map_parse_mem` | hold |
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
| `freeze` nest | `spr[].freeze` | `Player.freeze`; A hit `++` | hold; unfreeze 11.3 |
| `&vision` starts 0 | `MAIN.c` / `draw_screen_game` | `DINK_VISION_DEFAULT` | hold |
| ini commands case-insensitive | `compare()` | `tolower` cmd | hold |

## House leftovers (not blocking 10.x)

| Item | FreeDink | When |
|---|---|---|
| Idle after diagonal | `human_brain` 1/3→2, 7/9→8 | hold (#28) |
| Full `get_box` clip | crop to playl…playx, 0…playy | first sprite that bleeds |
| `SET_FRAME_*` | `program_idata` | first seq that needs it |
| NOTANIM copy frame 1 | `load_sprites` | frame without SSI |
| Hardness 1 vs 2 | `screen_hitmap` | flying / combat |

## Official campaign (no D-Mods)

Canon table: plan **Official campaign systems**. Out of scope: D-Mod loader, editor, D-Mod-only DinkC.

| Item | FreeDink | Bite |
|---|---|---|
| Talk / hit | `run_through_tag_list_talk`, hit list | **10.1 talk**; 10.2 hit |
| DinkC + attach + yields | `dinkc*`, `game_screen_init_scripts` | 11 |
| `&vision` / `force_vision` | `draw_screen_game` | 11 |
| Engine + `MAIN.c` globals | `attach()` | 11.4 |
| `freeze` nest | `spr[].freeze` | 11.3 |
| SFX + MIDI stream | `sfx`, `bgm` | **12 after 16** (stub until then) |
| Say / choice / font | brain 8, `game_choice` | 13 / V5 |
| Screen edge + warp + `screenlock` | `did_player_cross_screen`, `special_block` | 14 |
| `play.spmap` editor_type | `fix_dead_sprites` | 14 + 17 |
| Brains 0–17 (stock names) | `update_frame` | 15.1 |
| Push / death | `human_brain`, `die` | 15 |
| Touch / inv / HUD / map bmp | `status`, `process_show_bmp` | 16 / V6 |
| VMU save | `savegame` | 17 |

## How to add a bite

1. Open the FreeDink function for that behavior.
2. Port the rule (offsets, exclusive ranges, inverted `hard`).
3. Name the function in the PR and, if new, one GOTCHAS bullet.
4. Host test that would fail if we guessed.

Do not mark a bite done because “it looks close.”
