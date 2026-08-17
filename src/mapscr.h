/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_MAPSCR_H
#define DINKCAST_MAPSCR_H

#include <stddef.h>
#include <stdint.h>

#define DINK_MAP_RECSIZE 31280
#define DINK_SCREEN_TILES 96
#define DINK_EDITOR_SPRITES 100
#define DINK_SPR_TYPE_INVISIBLE 2
#define DINK_VISION_DEFAULT 0

struct MapTile {
    int32_t square_full_idx0;
    int32_t althard;
};

/* All int32 + 16-byte script: no uint8 hole. SH-4 faults/garbles
 * misaligned int32 in an array if sizeof % 4 != 0. */
struct EditorSprite {
    int32_t x, y, seq, frame, type, size;
    int32_t active;
    int32_t brain;
    int32_t que;
    int32_t hard;
    int32_t vision;
    char script[16];
};

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(struct EditorSprite) % 4u == 0u,
               "EditorSprite must be 4-aligned for SH-4 arrays");
#endif

/* Vision 0 always; vision N only when current == N. Type 2 is hardness-only. */
int editor_sprite_on_vision(const struct EditorSprite *s, int vision);
int editor_sprite_draw(const struct EditorSprite *s, int vision);
/* screen_rank_*: que != 0 ? que : y */
int editor_sprite_rank_y(const struct EditorSprite *s);

struct MapScreen {
    struct MapTile t[97];
    struct EditorSprite sprite[101];
    char script[21];
};

/* FreeDink: sheet = idx/128, cell = idx%128; ts file is sheet+1. */
void tile_split(int32_t square_full_idx0, int *sheet0, int *cell);

int map_parse_mem(const uint8_t *p, size_t n, struct MapScreen *out);
/* 1-based map.dat record (loc[] value). */
int map_load_record(int rec, struct MapScreen *out);
int map_file_records(int64_t file_bytes, int *out_count, int *out_rem);

#endif
