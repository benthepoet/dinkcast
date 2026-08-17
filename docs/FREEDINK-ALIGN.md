# FreeDink alignment

**Rule:** Dinkcast **grafts GNU FreeDink**. If a behavior exists in that tree, copy it. Do not invent a simpler box, filter, or move test. Dreamcast-only exceptions: PVR lists/twiddle, ISO 8.3, Maple, no SDL blit.

Cite the FreeDink function in the PR when you touch that path. If you cannot name it, stop and look it up.

Source: GNU FreeDink `master` (`gitGNU/gnu_freedink`). Official freeware data via `DINK_DATA`.

## Current play (through 9.3 / 8.6)

| Item | FreeDink | Dinkcast | Status |
|---|---|---|---|
| `dink.dat` 20+769×3 i32+2240 | `EditorMap::load` | `world_parse_mem` | hold |
| `map.dat` 31280, tile 80 B, sprite 220 B | `load_screen_to` | `map_parse_mem` | hold |
| `vision` +188, `hard` +120 (`0`=solid), `que` +116 | `editor_screen` / `screen_rank_*` | parsed; rank `que?:y` | hold |
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
| `&vision` starts 0 | `MAIN.c` / `draw_screen_game` | `DINK_VISION_DEFAULT` | hold |
| ini commands case-insensitive | `compare()` | `tolower` cmd | hold |

## Deferred (not in current loop — graft when that bite lands)

| Item | FreeDink | Bite |
|---|---|---|
| `SET_FRAME_FRAME` / `_DELAY` / `_SPECIAL` | `program_idata` | 8.x if a used seq needs it |
| `alt` trim | `get_box` | when a screen uses clip |
| `size` ≠ 100 | `get_box` scale | when a sprite is scaled |
| `BLACK` / `LEFTALIGN` | `load_sprites` flags | HUD / fade |
| Loose `prefix01.bmp` (no `dir.ff`) | `load_sprites` fallback | if a seq has no pack |
| NPC/fire brains, scripts | `brain_*` / DinkC | 10–13 |
| Warp, screenlock, `is_warp` | `special_block` | 14 |
| `dinkspeed` FPS remap | `game_compute_speed` | keep 3 px @ 60 Hz (plan) |
| Hardness value 1 vs 2 (low/high) | `screen_hitmap` | combat / flying |
| Pig / say box | scripts | 10 / 13 / V5 |

## How to add a bite

1. Open the FreeDink function for that behavior.
2. Port the rule (offsets, exclusive ranges, inverted `hard`).
3. Name the function in the PR and, if new, one GOTCHAS bullet.
4. Host test that would fail if we guessed.

Do not mark a bite done because “it looks close.”
