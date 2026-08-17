/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_MAPSCR_H
#define DINKCAST_MAPSCR_H

#include <stddef.h>
#include <stdint.h>

#define DINK_MAP_RECSIZE 31280
#define DINK_SCREEN_TILES 96
#define DINK_EDITOR_SPRITES 100

struct MapTile {
    int32_t square_full_idx0;
    int32_t althard;
};

struct EditorSprite {
    int32_t x, y, seq, frame, type, size;
    uint8_t active;
    int32_t brain;
    char script[14];
};

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
