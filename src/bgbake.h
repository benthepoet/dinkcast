/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_BGBAKE_H
#define DINKCAST_BGBAKE_H

#include "mapscr.h"

/* FreeDink game_place_sprites: editor rows of type 0 (background) and 2
 * (hardness only) are drawn into the background, their hardness stamped,
 * then removed (lsm_remove_sprite), so they never hold a live sprite slot.
 * Dinkcast moves them into this list instead (drawn under everything every
 * frame, stamped again on draw_hard_map) and frees their row. */
struct BgRow {
    int ed; /* editor index j (hardness id 100 + j, draw tie-break) */
    struct EditorSprite es;
};

/* rows[1..100]: move this vision's active non-warp type 0/2 rows into out
 * (up to cap) and clear their active flag. Warp rows stay (warp lookups read
 * the table by editor index). Returns the number moved. */
int bg_bake(struct EditorSprite *rows, int vision, struct BgRow *out, int cap);

/* FreeDink fill_back_sprites: an editor row whose hardness is stamped from
 * the editor table itself (active, on this vision, not type 1, hard == 0).
 * Used on draw_hard_map with the spmap-updated editor rows; live sprites
 * go through fill_hard_sprites instead. */
int bg_stamps_back(const struct EditorSprite *es, int vision);

#endif
