/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_HARD_H
#define DINKCAST_HARD_H

#include <stddef.h>
#include <stdint.h>

#define DINK_HARD_TILES 800
#define DINK_HARD_PX 51
#define DINK_HARD_REC (DINK_HARD_PX * DINK_HARD_PX + 1 + 2 + 4)
#define DINK_BTILE_MAX 5248

#include "mapscr.h"
#include "tiles.h"

struct HardMap {
    uint16_t btile_default[DINK_BTILE_MAX];
    int ready;
};

struct HardMask {
    uint8_t *pix; /* 600×400, 0 = walkable */
};

void hard_free(struct HardMap *h);
int hard_parse_defaults(const uint8_t *p, size_t n, struct HardMap *out);
int hard_load(struct HardMap *out);
int hard_id_for_tile(const struct HardMap *h, int32_t square_full_idx0,
                     int32_t althard);
int hard_sample(const struct HardMap *h, int hid, int lx, int ly);
void hard_mask_free(struct HardMask *m);
int hard_stamp_tiles(const struct HardMap *h, const struct MapScreen *scr,
                     struct HardMask *out);
/* FreeDink get_hard(x-playl, y). Sprite origin, not a box. */
int hard_get(const struct HardMask *m, int sx, int sy);
/* dc_screenlock / get_hard clamp. 1.08: 0 or 1 sets; else read. */
int hard_screenlock_get(void);
void hard_screenlock_set(int on);
int hard_box_blocked(const struct HardMask *m, int x, int y, int hl, int ht,
                     int hr, int hb);
void hard_stamp_box(struct HardMask *m, int x, int y, int hl, int ht, int hr,
                    int hb, int hid);

#endif
