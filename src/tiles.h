/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_TILES_H
#define DINKCAST_TILES_H

/*
 * Bite 6.1 policy A: one 512×512 RGB565 atlas of *used* 50×50 cells
 * with 1 px gutter (10×10 slots). Whole tsNN.bmp is 12×8 and will not
 * fit; pack only cells this screen needs. PAL8 512×512 = 256 KB (main
 * palette, bank 1 opaque); RGB565 fallback 512 KB.
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
    /* Exactly one is set, both static (tiles_free frees nothing). pal8:
     * 512×512 BSS indices on the main palette (official tilesheets);
     * rgb565: the kept 512 KB fallback (palette unset or an off-palette
     * sheet), allocated once. Each build reuses its buffer, so only the
     * latest atlas's CPU pixels are valid; PVR upload copies. */
    uint8_t *pal8;
    uint16_t *rgb565;
    int slot_x[DINK_SCREEN_TILES];
    int slot_y[DINK_SCREEN_TILES];
    int used;
};

void tiles_free(struct TileAtlas *a);
int tiles_build_atlas(const struct MapScreen *scr, struct TileAtlas *out);
int tiles_cell00_rgb(const uint8_t *bmp, size_t n, uint8_t rgb[3]);

void tiles_evict(struct TileAtlas *a);

size_t tiles_cache_bytes(void);
/* Drop every cached sheet (main palette changed: reload as PAL8/565). */
void tiles_cache_clear(void);
/* VRAM bytes the atlas texture takes in the current format. */
size_t tiles_atlas_tex_bytes(void);
/* Tests: switch the atlas build to RGB565 at this cell (-1 = off). */
void tiles_test_force_565_at(int cell);
int tiles_cache_sheets(void);

#ifdef _arch_dreamcast
int tiles_pvr_ensure(void);
void tiles_draw_clear_pvr(uint32_t argb);
int tiles_upload_pvr(struct TileAtlas *a);
/* An atlas texture is allocated (tiles_upload_pvr frees it before it
 * allocates the next one, so a swap needs no extra VRAM for tiles). */
int tiles_tex_held(void);
void tiles_draw_pvr(const struct TileAtlas *a);
#endif

#endif
