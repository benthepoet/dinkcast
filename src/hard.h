/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_HARD_H
#define DINKCAST_HARD_H

#include <stddef.h>
#include <stdint.h>

#define DINK_HARD_TILES 800
#define DINK_HARD_PX 51
#define DINK_HARD_REC (DINK_HARD_PX * DINK_HARD_PX + 1 + 2 + 4)
#define DINK_BTILE_MAX 5248

struct HardMap {
    uint16_t btile_default[DINK_BTILE_MAX];
};

int hard_parse_defaults(const uint8_t *p, size_t n, struct HardMap *out);
int hard_load(struct HardMap *out);
int hard_id_for_tile(const struct HardMap *h, int32_t square_full_idx0,
                     int32_t althard);

#endif
