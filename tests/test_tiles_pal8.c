/* SPDX-License-Identifier: GPL-3.0-or-later */
/* PAL8 tile atlas: with the main palette set, the start screen's atlas is
 * 8-bit indices that map through the palette to exactly the RGB565 atlas
 * built without it, and the sheet cache holds half the bytes. Off the
 * main palette the atlas stays RGB565. */
#include "bmp.h"
#include "fs.h"
#include "mapscr.h"
#include "start_map.h"
#include "tiles.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c)                                                               \
    do {                                                                       \
        if (!(c)) {                                                            \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);       \
            g_fail = 1;                                                        \
        }                                                                      \
    } while (0)

static int load_esplash_pal(uint8_t rgb[768])
{
    uint8_t hdr[54 + 1024];
    FILE *fp = dink_fopen("Tiles/Esplash.bmp", "rb");
    int i;

    if (fp == NULL || fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        if (fp != NULL) {
            fclose(fp);
        }
        return -1;
    }
    fclose(fp);
    for (i = 0; i < 256; i++) {
        rgb[i * 3 + 0] = hdr[54 + i * 4 + 2];
        rgb[i * 3 + 1] = hdr[54 + i * 4 + 1];
        rgb[i * 3 + 2] = hdr[54 + i * 4 + 0];
    }
    return 0;
}

static int in_cell(const struct TileAtlas *a, size_t i)
{
    int x = (int)(i % DINK_ATLAS_W), y = (int)(i / DINK_ATLAS_W), s;

    for (s = 0; s < a->used; s++) {
        if (x >= a->slot_x[s] && x < a->slot_x[s] + DINK_TILE_PX &&
            y >= a->slot_y[s] && y < a->slot_y[s] + DINK_TILE_PX) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    static uint8_t pal[768], other[768];
    static uint16_t ref[DINK_ATLAS_W * DINK_ATLAS_H];
    struct World w;
    struct MapScreen scr;
    struct TileAtlas a;
    size_t i, n = (size_t)DINK_ATLAS_W * DINK_ATLAS_H, mism = 0;
    size_t bytes16, bytes8;

    if (dink_fs_init() != 0 || world_load(&w) != 0 ||
        map_load_record((int)w.loc[DINK_START_PLAYER_MAP], &scr) != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    CHECK(load_esplash_pal(pal) == 0);

    /* Reference: RGB565 atlas, no main palette. */
    bmp_set_main_palette(NULL);
    memset(&a, 0, sizeof(a));
    CHECK(tiles_build_atlas(&scr, &a) == 0);
    CHECK(a.rgb565 != NULL && a.pal8 == NULL);
    if (a.rgb565 != NULL) {
        memcpy(ref, a.rgb565, sizeof(ref));
    }
    bytes16 = tiles_cache_bytes();
    tiles_free(&a);

    /* PAL8: drop the 16-bit sheets so the cache refills as PAL8. */
    bmp_set_main_palette(pal);
    tiles_cache_clear();
    CHECK(tiles_cache_sheets() == 0 && tiles_cache_bytes() == 0);
    memset(&a, 0, sizeof(a));
    CHECK(tiles_build_atlas(&scr, &a) == 0);
    CHECK(a.pal8 != NULL && a.rgb565 == NULL);
    /* Compare the 96 tile cells; the unused atlas area (never sampled)
     * is index 0 in PAL8 and 0 in RGB565. */
    if (a.pal8 != NULL) {
        for (i = 0; i < n; i++) {
            const uint8_t *c = pal + a.pal8[i] * 3u;

            if (!in_cell(&a, i)) {
                continue;
            }
            uint16_t v = (uint16_t)(((c[0] >> 3) << 11) |
                                    ((c[1] >> 2) << 5) | (c[2] >> 3));

            if (v != ref[i]) {
                mism++;
            }
        }
    }
    CHECK(mism == 0);
    bytes8 = tiles_cache_bytes();
    CHECK(bytes8 * 2u == bytes16);
    printf("atlas pal8 mismatches=%u sheet bytes 16-bit=%u pal8=%u\n",
           (unsigned)mism, (unsigned)bytes16, (unsigned)bytes8);
    tiles_free(&a);

    /* Mid-build switch (an off-palette sheet at cell 40): the 40 PAL8
     * cells already built convert to RGB565, the rest blit as RGB565, and
     * the whole atlas matches the reference on every cell. */
    tiles_test_force_565_at(40);
    memset(&a, 0, sizeof(a));
    CHECK(tiles_build_atlas(&scr, &a) == 0);
    tiles_test_force_565_at(-1);
    CHECK(a.rgb565 != NULL && a.pal8 == NULL);
    if (a.rgb565 != NULL) {
        mism = 0;
        for (i = 0; i < n; i++) {
            if (in_cell(&a, i) && a.rgb565[i] != ref[i]) {
                mism++;
            }
        }
        CHECK(mism == 0);
    }
    tiles_free(&a);

    /* Mixed: PAL8 sheets cached, then an off-palette main palette makes
     * the next sheet 16-bit only if reloaded; clear and rebuild instead
     * to check the all-565 path matches the reference. */
    tiles_cache_clear();

    /* Off the main palette: RGB565, same picture. */
    memcpy(other, pal, sizeof(other));
    other[3] ^= 0xff;
    bmp_set_main_palette(other);
    tiles_cache_clear();
    memset(&a, 0, sizeof(a));
    CHECK(tiles_build_atlas(&scr, &a) == 0);
    CHECK(a.rgb565 != NULL && a.pal8 == NULL);
    if (a.rgb565 != NULL) {
        CHECK(memcmp(a.rgb565, ref, sizeof(ref)) == 0);
    }
    tiles_free(&a);
    bmp_set_main_palette(NULL);
    if (g_fail) {
        return 1;
    }
    printf("OK test_tiles_pal8\n");
    return 0;
}
