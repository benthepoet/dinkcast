# Visual fidelity — 2026-11-21

Placement and drawing compared with GNU FreeDink (`~/Source/gnu_freedink/src`) after the damage-number miss (PR #172). Read side by side. Nothing here was run in Flycast or on hardware. Analysis only: no engine change.

Damage numbers and Dink's own floater are on master (#172, #175). This note is the rest of that class.

## Talk text sits too low

FreeDink `say_text` (`text.cpp`) stores the vertical offset once, from the frame showing when the line starts:

```c
spr[tsprite].defense = (k[gfx_id].box.bottom - k[gfx_id].yoffset) + 100;
```

`box.bottom` is the bitmap height. `text_brain` (`brain_text.cpp`) then sets the box to `owner.y - defense` every frame. That is 100 px above the top of the picture, not 100 px above the feet. The offset stays at the frame from `say` time if the sprite changes picture later.

`saybox_place` (`src/saybox.c`) always uses `owner.y - DINK_SAY_YOFF` (100). The error is `bitmap_h - yoffset`. On Dink that is about 15–30 px, so the line sits on his head. On a tall sprite it overlaps the body.

Horizontal placement matches: `x - 75` (`strength` in `say_text`), clamp to at least 1, then if `x + 150 > 620` shift left (`text_draw`). Wrap width 150, narrator owner 1000, and `` ` `` colour codes match `text_draw`.

## Sprites draw through the border

FreeDink `get_box` (`live_screen.cpp`) clips each sprite to `playl = 20`, `playx = 620`, `playy = 400` and shrinks the source rectangle so the picture stops at that edge.

`sprite_draw_pvr_clip` (`src/sprite.c`) only skips a sprite that is entirely outside that rectangle. A sprite that straddles x = 20, x = 620, or y = 400 is drawn whole, over the side bars and the status bar. Tiles are placed at `DINK_PLAY_LEFT` (20) in `tiles_draw_pvr`, so the map is in the right place and the sprites are not.

`sp_noclip` is also missing. FreeDink `dc_sp_noclip` widens that clip to 0..640 by 0..480. `sprite_draw_pvr_noclip` exists and the START menu uses it. The play loop always calls `sprite_draw_pvr_alt_size`, which uses the playfield reject.

## BLACK is not a colour key

FreeDink `load_sprite_pak` (`gfx_sprites.cpp`): `BLACK` makes palette index 255 transparent and darkens white. Other frames make index 0 (white) transparent and brighten black.

`parse_load_tail` (`src/ini.c`) sees `BLACK` and only sets `reuse_off`. The loader still keys near-white (`pack1555` in `src/sprite.c`, or palette index 0 for a main-palette PAL8 frame). Stock `BLACK` sequences draw with the hole in the wrong place. The status HUD avoids this by calling `sprite_load_seq_frame_nocolorkey` on purpose.

## Frame-1 centers are inherited backwards

FreeDink copies frame 1's center onto later frames only when the load line says `NOTANIM` and that frame did not give its own offset (`gfx_sprites.cpp`, `oo > 1 && notanim`).

`parse_load_tail` does the opposite. `NOTANIM` returns with `reuse_off` left at 0. A normal animation line, and `BLACK`, set `reuse_off`. On frame 1, `load_seq_frame` (`src/sprite.c`) writes the computed center back into the sequence when `seq->cx` or `seq->cy` was omitted. Later frames then take that center in `ini_frame_geom`. Variable-size frames (explosions and similar) register to frame 1 instead of their own bitmap. An explicit offset on the frame still wins.

## Smaller gaps

- Screen changes cut. FreeDink `transition` (`live_screen.cpp`) slides the 600×400 playfield for `move_screen` 2, 4, 6, and 8. Nothing in `src/` does that.
- One say box. FreeDink can have several brain-8 text sprites at once.
- Dink is always drawn at size 100 (`src/main.c` play loop). Other sprites use `brains_slot_size`.

## Already aligned

Status digits, life bars, gold, and level use the same coordinates as `status.cpp` (`draw_num` / `draw_bar`: exp 404,459; strength 81,415; defense 81,437; magic 81,459; gold 298,457; level 528 or 523,456; bars at 284,412). Inventory cells match `inventory.cpp` `draw_item` (weapons 260,83 step 83×75, magic 45,83 step 83×75). Damage numbers use the bitmap box and the 50 px centered rect (#172), and Dink's own floater uses his drawn frame (#175).
