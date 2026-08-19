/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tiles.h"

#include "bmp.h"
#include "fs.h"
#include "rgb565.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

void tiles_free(struct TileAtlas *a)
{
    if (a == NULL) {
        return;
    }
    free(a->rgb565);
    a->rgb565 = NULL;
    a->used = 0;
}

int tiles_cell00_rgb(const uint8_t *bmp, size_t n, uint8_t rgb[3])
{
    struct Bitmap bm;
    int i;

    if (bmp == NULL || rgb == NULL) {
        return -1;
    }
    memset(&bm, 0, sizeof(bm));
    if (bitmap_load_mem(bmp, n, &bm) != 0) {
        return -1;
    }
    if (bm.w < DINK_TILE_PX || bm.h < DINK_TILE_PX || bm.pixels == NULL) {
        bitmap_free(&bm);
        return -1;
    }
    i = 0;
    if (bm.bpp == 8) {
        const uint8_t *pal = bm.pal + (size_t)bm.pixels[i] * 3u;
        rgb[0] = pal[0];
        rgb[1] = pal[1];
        rgb[2] = pal[2];
    } else if (bm.bpp == 24) {
        rgb[0] = bm.pixels[0];
        rgb[1] = bm.pixels[1];
        rgb[2] = bm.pixels[2];
    } else {
        bitmap_free(&bm);
        return -1;
    }
    bitmap_free(&bm);
    return 0;
}

static int slurp_rel(const char *rel, const uint8_t **out, size_t *n)
{
    if (dink_blob_get(rel, out, n) != 0 || n == NULL || *n < 54) {
        return -1;
    }
    return 0;
}

#define DINK_TS_CACHE 8

static struct {
    int sheet0;
    int w, h;
    uint16_t *pix;
} g_ts[DINK_TS_CACHE];

static int ts_sheet(int sheet0, uint16_t **pix, int *w, int *h)
{
    int i, empty = -1;
    char rel[32];
    const uint8_t *raw = NULL;
    size_t n = 0;
    struct Bitmap bm;
    uint16_t *p = NULL;
    int npx = 0;

    for (i = 0; i < DINK_TS_CACHE; i++) {
        if (g_ts[i].pix != NULL && g_ts[i].sheet0 == sheet0) {
            *pix = g_ts[i].pix;
            *w = g_ts[i].w;
            *h = g_ts[i].h;
            return 0;
        }
        if (empty < 0 && g_ts[i].pix == NULL) {
            empty = i;
        }
    }
    snprintf(rel, sizeof(rel), "tiles/ts%02d.bmp", sheet0 + 1);
    printf("tiles slurp %s\n", rel);
    if (slurp_rel(rel, &raw, &n) != 0) {
        printf("tiles slurp fail %s\n", rel);
        return -1;
    }
    printf("tiles slurp ok %s %u\n", rel, (unsigned)n);
    memset(&bm, 0, sizeof(bm));
    if (bitmap_load_mem(raw, n, &bm) != 0) {
        return -1;
    }
    if (rgb565_from_bitmap(&bm, &p, &npx) != 0) {
        bitmap_free(&bm);
        return -1;
    }
    *w = bm.w;
    *h = bm.h;
    bitmap_free(&bm);
    if (empty < 0) {
        /* Do not evict a decoded sheet: re-slurp during swap hangs /cd. */
        printf("tiles cache full skip ts%02d\n", sheet0 + 1);
        free(p);
        return -1;
    }
    g_ts[empty].sheet0 = sheet0;
    g_ts[empty].w = *w;
    g_ts[empty].h = *h;
    g_ts[empty].pix = p;
    *pix = p;
    return 0;
}

static void blit_cell(uint16_t *atlas, const uint16_t *sheet, int sw, int sh,
                      int cell, int dx, int dy)
{
    int cx, cy, y, x;

    cx = (cell % 12) * DINK_TILE_PX;
    cy = (cell / 12) * DINK_TILE_PX;
    for (y = 0; y < DINK_TILE_PX; y++) {
        if (cy + y >= sh) {
            break;
        }
        for (x = 0; x < DINK_TILE_PX; x++) {
            uint16_t px = 0;
            if (cx + x < sw) {
                px = sheet[(cy + y) * sw + (cx + x)];
            }
            atlas[(dy + y) * DINK_ATLAS_W + (dx + x)] = px;
        }
    }
}

int tiles_warm_sheet(int sheet0)
{
    uint16_t *pix = NULL;
    int w = 0, h = 0;

    if (sheet0 < 0 || sheet0 >= 41) {
        return -1;
    }
    return ts_sheet(sheet0, &pix, &w, &h);
}

int tiles_build_atlas(const struct MapScreen *scr, struct TileAtlas *out)
{
    int i, slot;

    if (scr == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->rgb565 = (uint16_t *)calloc((size_t)DINK_ATLAS_W * DINK_ATLAS_H, 2);
    if (out->rgb565 == NULL) {
        return -1;
    }

    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        int sheet0, cell, sw, sh, ax, ay;
        uint16_t *pix = NULL;

        tile_split(scr->t[i].square_full_idx0, &sheet0, &cell);
        if (sheet0 < 0 || sheet0 >= 41 || cell < 0 || cell >= 128) {
            tiles_free(out);
            return -1;
        }
        if (ts_sheet(sheet0, &pix, &sw, &sh) != 0 || pix == NULL) {
            tiles_free(out);
            return -1;
        }
        slot = i;
        ax = (slot % DINK_ATLAS_COLS) * DINK_ATLAS_STRIDE;
        ay = (slot / DINK_ATLAS_COLS) * DINK_ATLAS_STRIDE;
        blit_cell(out->rgb565, pix, sw, sh, cell, ax, ay);
        out->slot_x[i] = ax;
        out->slot_y[i] = ay;
    }
    out->used = DINK_SCREEN_TILES;
    return 0;
}

#ifdef _arch_dreamcast
static pvr_ptr_t g_tile_tex;
static int g_pvr_ready;

int tiles_pvr_ensure(void)
{
    pvr_init_params_t params;

    if (g_pvr_ready) {
        return 0;
    }
    memset(&params, 0, sizeof(params));
    params.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    params.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    params.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_16;
    params.vertex_buf_size = 512 * 1024;
    if (pvr_init(&params) != 0) {
        return -1;
    }
    g_pvr_ready = 1;
    return 0;
}

void tiles_draw_clear_pvr(uint32_t argb)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    if (tiles_pvr_ensure() != 0) {
        return;
    }
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    vert.flags = PVR_CMD_VERTEX;
    vert.argb = argb;
    vert.oargb = 0;
    vert.z = 1.0f;
    vert.u = vert.v = 0.0f;
    vert.x = 0.0f;
    vert.y = 0.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = 640.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = 0.0f;
    vert.y = 480.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = 640.0f;
    pvr_prim(&vert, sizeof(vert));
    pvr_list_finish();
    pvr_scene_finish();
    /* Finish returns before the flip; wait or the splash stays up. */
    pvr_wait_ready();
}

int tiles_upload_pvr(struct TileAtlas *a)
{
    if (a == NULL || a->rgb565 == NULL) {
        return -1;
    }
    if (g_tile_tex != NULL) {
        pvr_mem_free(g_tile_tex);
        g_tile_tex = NULL;
    }
    /* Enable PT bins so 1555 sprites can punch through (defaults often 0).
     * pvr_init once — a second call wipes live saybox/font VRAM. */
    if (tiles_pvr_ensure() != 0) {
        return -1;
    }
    g_tile_tex = pvr_mem_malloc((size_t)DINK_ATLAS_W * DINK_ATLAS_H * 2u);
    if (g_tile_tex == NULL) {
        return -1;
    }
    pvr_txr_load_ex(a->rgb565, g_tile_tex, DINK_ATLAS_W, DINK_ATLAS_H,
                    PVR_TXRLOAD_16BPP);
    return 0;
}

void tiles_evict(struct TileAtlas *a)
{
    if (g_tile_tex != NULL) {
        pvr_mem_free(g_tile_tex);
        g_tile_tex = NULL;
    }
    tiles_free(a);
}

void tiles_draw_pvr(const struct TileAtlas *a)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    int i;

    if (a == NULL || g_tile_tex == NULL) {
        return;
    }
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565, DINK_ATLAS_W,
                     DINK_ATLAS_H, g_tile_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        float x0 = (float)(DINK_PLAY_LEFT + (i % 12) * DINK_TILE_PX);
        float y0 = (float)(DINK_PLAY_TOP + (i / 12) * DINK_TILE_PX);
        float x1 = x0 + (float)DINK_TILE_PX;
        float y1 = y0 + (float)DINK_TILE_PX;
        float u0 = (float)a->slot_x[i] / (float)DINK_ATLAS_W;
        float v0 = (float)a->slot_y[i] / (float)DINK_ATLAS_H;
        float u1 = (float)(a->slot_x[i] + DINK_TILE_PX) / (float)DINK_ATLAS_W;
        float v1 = (float)(a->slot_y[i] + DINK_TILE_PX) / (float)DINK_ATLAS_H;

        vert.argb = 0xffffffff;
        vert.oargb = 0;
        vert.z = 1.0f;
        vert.flags = PVR_CMD_VERTEX;
        vert.x = x0;
        vert.y = y0;
        vert.u = u0;
        vert.v = v0;
        pvr_prim(&vert, sizeof(vert));
        vert.x = x1;
        vert.y = y0;
        vert.u = u1;
        vert.v = v0;
        pvr_prim(&vert, sizeof(vert));
        vert.x = x0;
        vert.y = y1;
        vert.u = u0;
        vert.v = v1;
        pvr_prim(&vert, sizeof(vert));
        vert.flags = PVR_CMD_VERTEX_EOL;
        vert.x = x1;
        vert.y = y1;
        vert.u = u1;
        vert.v = v1;
        pvr_prim(&vert, sizeof(vert));
    }
}
#else
void tiles_evict(struct TileAtlas *a)
{
    tiles_free(a);
}
#endif
