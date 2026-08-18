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

static int slurp_rel(const char *rel, uint8_t **out, size_t *n)
{
    FILE *fp;
    long sz;

    fp = dink_fopen(rel, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 54 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    *out = (uint8_t *)malloc((size_t)sz);
    if (*out == NULL) {
        fclose(fp);
        return -1;
    }
    if (fread(*out, 1, (size_t)sz, fp) != (size_t)sz) {
        free(*out);
        *out = NULL;
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *n = (size_t)sz;
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

static void free_sheets(uint16_t **sheets)
{
    int i;

    for (i = 0; i < 41; i++) {
        free(sheets[i]);
        sheets[i] = NULL;
    }
}

int tiles_build_atlas(const struct MapScreen *scr, struct TileAtlas *out)
{
    uint16_t *sheets[41];
    int sw[41], sh[41];
    int i, slot;

    if (scr == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    memset(sheets, 0, sizeof(sheets));
    out->rgb565 = (uint16_t *)calloc((size_t)DINK_ATLAS_W * DINK_ATLAS_H, 2);
    if (out->rgb565 == NULL) {
        return -1;
    }

    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        int sheet0, cell;
        char rel[32];
        uint8_t *raw = NULL;
        size_t n = 0;
        struct Bitmap bm;
        uint16_t *pix = NULL;
        int npx = 0;
        int ax, ay;

        tile_split(scr->t[i].square_full_idx0, &sheet0, &cell);
        if (sheet0 < 0 || sheet0 >= 41 || cell < 0 || cell >= 128) {
            free_sheets(sheets);
            tiles_free(out);
            return -1;
        }
        if (sheets[sheet0] == NULL) {
            snprintf(rel, sizeof(rel), "tiles/ts%02d.bmp", sheet0 + 1);
            if (slurp_rel(rel, &raw, &n) != 0) {
                free_sheets(sheets);
                tiles_free(out);
                return -1;
            }
            memset(&bm, 0, sizeof(bm));
            if (bitmap_load_mem(raw, n, &bm) != 0) {
                free(raw);
                free_sheets(sheets);
                tiles_free(out);
                return -1;
            }
            free(raw);
            if (rgb565_from_bitmap(&bm, &pix, &npx) != 0) {
                bitmap_free(&bm);
                free_sheets(sheets);
                tiles_free(out);
                return -1;
            }
            sw[sheet0] = bm.w;
            sh[sheet0] = bm.h;
            bitmap_free(&bm);
            sheets[sheet0] = pix;
        }
        slot = i;
        ax = (slot % DINK_ATLAS_COLS) * DINK_ATLAS_STRIDE;
        ay = (slot / DINK_ATLAS_COLS) * DINK_ATLAS_STRIDE;
        blit_cell(out->rgb565, sheets[sheet0], sw[sheet0], sh[sheet0], cell, ax,
                  ay);
        out->slot_x[i] = ax;
        out->slot_y[i] = ay;
    }
    free_sheets(sheets);
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
    pvr_init(&params);
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
