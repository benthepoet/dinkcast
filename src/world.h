/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_WORLD_H
#define DINKCAST_WORLD_H

#include <stddef.h>
#include <stdint.h>

#define DINK_WORLD_SLOTS 769
#define DINK_DAT_IDENT 20
#define DINK_DAT_TRAILER 2240
#define DINK_DAT_SIZE \
    (DINK_DAT_IDENT + DINK_WORLD_SLOTS * 12 + DINK_DAT_TRAILER)

struct World {
    int32_t loc[DINK_WORLD_SLOTS];
    int32_t music[DINK_WORLD_SLOTS];
    int32_t indoor[DINK_WORLD_SLOTS];
};

int world_parse_mem(const uint8_t *p, size_t n, struct World *out);
int world_load(struct World *out);
int world_loc_nonzero_count(const struct World *w);
int32_t world_max_loc(const struct World *w);

#endif
