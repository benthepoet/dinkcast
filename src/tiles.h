/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_TILES_H
#define DINKCAST_TILES_H

/*
 * Bite 6.1 policy A: one 512×512 RGB565 atlas of *used* 50×50 cells
 * with 1 px gutter (10×10 slots). Whole tsNN.bmp is 12×8 and will not
 * fit; pack only cells this screen needs. Budget 512×512×2 = 512 KB.
 */
#define DINK_TILE_PX 50
#define DINK_ATLAS_W 512
#define DINK_ATLAS_H 512
#define DINK_ATLAS_GUTTER 1
#define DINK_ATLAS_STRIDE (DINK_TILE_PX + DINK_ATLAS_GUTTER)
#define DINK_ATLAS_COLS 10
#define DINK_PLAY_LEFT 20
#define DINK_PLAY_TOP 0
#define DINK_PLAY_W 600
#define DINK_PLAY_H 400

#include "mapscr.h"

#include <stdint.h>

struct TileAtlas {
    uint16_t *rgb565; /* 512×512 BSS; do not free. PVR upload copies. */
    int slot_x[DINK_SCREEN_TILES];
    int slot_y[DINK_SCREEN_TILES];
    int used;
};

void tiles_free(struct TileAtlas *a);
int tiles_build_atlas(const struct MapScreen *scr, struct TileAtlas *out);
/* Decode ts(sheet0+1).bmp into the sheet cache. Call before swap I/O. */
int tiles_warm_sheet(int sheet0);
int tiles_cell00_rgb(const uint8_t *bmp, size_t n, uint8_t rgb[3]);

void tiles_evict(struct TileAtlas *a);

#ifdef _arch_dreamcast
int tiles_pvr_ensure(void);
void tiles_draw_clear_pvr(uint32_t argb);
int tiles_upload_pvr(struct TileAtlas *a);
void tiles_draw_pvr(const struct TileAtlas *a);
#endif

#endif
