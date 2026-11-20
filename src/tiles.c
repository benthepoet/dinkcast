/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tiles.h"

#include "bmp.h"
#include "fs.h"
#include "mem.h"
#include "rgb565.h"
#include "twiddle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

/* Swap must not sbrk a big atlas: heap is full of packs; calloc fails and
 * the previous screen's tiles stay on PVR (house floor in the yard). PAL8
 * (every official tilesheet) uses this 256 KB BSS; the RGB565 fallback is
 * allocated once on first need and kept (below). */
static uint8_t g_atlas_pal8[DINK_ATLAS_W * DINK_ATLAS_H];
/* RGB565 fallback (main palette unset, or an off-palette sheet): allocated
 * once on first need and kept for the session, never per swap (GOTCHAS:
 * the heap cannot sbrk 512 KB after packs). Counted in mem peak. */
static uint16_t *g_atlas_565;
static int g_force_565_at = -1; /* tests: switch to 565 at this cell */

static uint16_t *atlas_565_buf(void)
{
    if (g_atlas_565 == NULL) {
        g_atlas_565 = (uint16_t *)malloc((size_t)DINK_ATLAS_W *
                                         DINK_ATLAS_H * 2u);
        if (g_atlas_565 == NULL) {
            printf("tiles atlas rgb565 fallback alloc fail\n");
            return NULL;
        }
        mem_set_atlas_extra((size_t)DINK_ATLAS_W * DINK_ATLAS_H * 2u);
        printf("tiles atlas rgb565 fallback allocated (kept)\n");
    }
    memset(g_atlas_565, 0, (size_t)DINK_ATLAS_W * DINK_ATLAS_H * 2u);
    return g_atlas_565;
}

void tiles_test_force_565_at(int cell)
{
    g_force_565_at = cell;
}

size_t tiles_atlas_tex_bytes(void)
{
    return (size_t)DINK_ATLAS_W * DINK_ATLAS_H *
           (bmp_main_palette() != NULL ? 1u : 2u);
}

/* Buffers are static (BSS or the kept fallback): nothing to free. */
void tiles_free(struct TileAtlas *a)
{
    if (a == NULL) {
        return;
    }
    a->rgb565 = NULL;
    a->pal8 = NULL;
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
    uint16_t *pix;  /* RGB565, or NULL */
    uint8_t *pix8;  /* PAL8 on the main palette, or NULL */
    int tick;
} g_ts[DINK_TS_CACHE];

static int ts_used(int i)
{
    return g_ts[i].pix != NULL || g_ts[i].pix8 != NULL;
}
static int g_ts_tick;

static int ts_evict_lru(void)
{
    int i, victim = -1;

    for (i = 0; i < DINK_TS_CACHE; i++) {
        if (ts_used(i) &&
            (victim < 0 || g_ts[i].tick < g_ts[victim].tick)) {
            victim = i;
        }
    }
    if (victim < 0) {
        return -1;
    }
    printf("tiles evict ts%02d\n", g_ts[victim].sheet0 + 1);
    free(g_ts[victim].pix);
    g_ts[victim].pix = NULL;
    free(g_ts[victim].pix8);
    g_ts[victim].pix8 = NULL;
    g_ts[victim].w = 0;
    g_ts[victim].h = 0;
    g_ts[victim].sheet0 = -1;
    return 0;
}

/* One tilesheet, cached: PAL8 indices when it is on the main palette (the
 * official 41), else RGB565. Bytes are what is held (1 or 2 per pixel). */
static int ts_sheet(int sheet0, uint16_t **pix, uint8_t **pix8, int *w,
                    int *h)
{
    int i, empty = -1;
    char rel[32];
    const uint8_t *raw = NULL;
    size_t n = 0, need;
    struct Bitmap bm;
    uint16_t *p = NULL;
    uint8_t *p8 = NULL;
    int npx = 0;

    *pix = NULL;
    *pix8 = NULL;
    g_ts_tick++;
    for (i = 0; i < DINK_TS_CACHE; i++) {
        if (ts_used(i) && g_ts[i].sheet0 == sheet0) {
            g_ts[i].tick = g_ts_tick;
            *pix = g_ts[i].pix;
            *pix8 = g_ts[i].pix8;
            *w = g_ts[i].w;
            *h = g_ts[i].h;
            return 0;
        }
        if (empty < 0 && !ts_used(i)) {
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
    if (bmp_pal_is_main(&bm)) {
        p8 = (uint8_t *)malloc((size_t)bm.w * (size_t)bm.h);
        if (p8 == NULL) {
            bitmap_free(&bm);
            return -1;
        }
        memcpy(p8, bm.pixels, (size_t)bm.w * (size_t)bm.h);
        need = (size_t)bm.w * (size_t)bm.h;
    } else {
        if (rgb565_from_bitmap(&bm, &p, &npx) != 0) {
            bitmap_free(&bm);
            return -1;
        }
        need = (size_t)bm.w * (size_t)bm.h * 2u;
    }
    *w = bm.w;
    *h = bm.h;
    bitmap_free(&bm);
    /* Plan §1.2: ts_rgb ≤ 1.25 MB, LRU by bytes not slot count. */
    while (tiles_cache_bytes() + need > (size_t)DINK_MEM_TS_RGB) {
        if (ts_evict_lru() != 0) {
            break;
        }
        empty = -1;
    }
    if (tiles_cache_bytes() + need > (size_t)DINK_MEM_TS_RGB) {
        printf("mem refuse pool=ts_rgb need=%u have=%u cap=%u\n",
               (unsigned)need, (unsigned)tiles_cache_bytes(),
               (unsigned)DINK_MEM_TS_RGB);
        free(p);
        free(p8);
        return -1;
    }
    if (empty < 0) {
        for (i = 0; i < DINK_TS_CACHE; i++) {
            if (!ts_used(i)) {
                empty = i;
                break;
            }
        }
    }
    if (empty < 0) {
        if (ts_evict_lru() != 0) {
            free(p);
            free(p8);
            return -1;
        }
        for (i = 0; i < DINK_TS_CACHE; i++) {
            if (!ts_used(i)) {
                empty = i;
                break;
            }
        }
        if (empty < 0) {
            free(p);
            free(p8);
            return -1;
        }
    }
    g_ts[empty].sheet0 = sheet0;
    g_ts[empty].w = *w;
    g_ts[empty].h = *h;
    g_ts[empty].pix = p;
    g_ts[empty].pix8 = p8;
    g_ts[empty].tick = g_ts_tick;
    *pix = p;
    *pix8 = p8;
    return 0;
}

void tiles_cache_clear(void)
{
    int i;

    for (i = 0; i < DINK_TS_CACHE; i++) {
        free(g_ts[i].pix);
        free(g_ts[i].pix8);
        memset(&g_ts[i], 0, sizeof(g_ts[i]));
        g_ts[i].sheet0 = -1;
    }
}

size_t tiles_cache_bytes(void)
{
    size_t t = 0;
    int i;

    for (i = 0; i < DINK_TS_CACHE; i++) {
        if (g_ts[i].w > 0 && g_ts[i].h > 0) {
            size_t px = (size_t)g_ts[i].w * (size_t)g_ts[i].h;

            if (g_ts[i].pix8 != NULL) {
                t += px;
            } else if (g_ts[i].pix != NULL) {
                t += px * 2u;
            }
        }
    }
    return t;
}

int tiles_cache_sheets(void)
{
    int i, n = 0;

    for (i = 0; i < DINK_TS_CACHE; i++) {
        if (ts_used(i)) {
            n++;
        }
    }
    return n;
}

static uint16_t to565(const uint8_t *rgb)
{
    return (uint16_t)(((rgb[0] >> 3) << 11) | ((rgb[1] >> 2) << 5) |
                      (rgb[2] >> 3));
}

/* Copy one 50×50 cell. Out of range on the sheet stays 0: black in RGB565,
 * index 0 (white) in PAL8; no official screen uses an off-sheet cell. A
 * PAL8 sheet into an RGB565 atlas converts via the main palette. */
static void blit_cell(struct TileAtlas *a, const uint16_t *sheet,
                      const uint8_t *sheet8, int sw, int sh, int cell, int dx,
                      int dy)
{
    const uint8_t *pal = bmp_main_palette();
    int cx, cy, y, x;

    cx = (cell % 12) * DINK_TILE_PX;
    cy = (cell / 12) * DINK_TILE_PX;
    for (y = 0; y < DINK_TILE_PX; y++) {
        if (cy + y >= sh) {
            break;
        }
        for (x = 0; x < DINK_TILE_PX; x++) {
            size_t di = (size_t)(dy + y) * DINK_ATLAS_W + (size_t)(dx + x);
            size_t si = (size_t)(cy + y) * (size_t)sw + (size_t)(cx + x);

            if (cx + x >= sw) {
                continue;
            }
            if (a->pal8 != NULL) {
                a->pal8[di] = sheet8[si];
            } else if (sheet8 != NULL) {
                a->rgb565[di] = pal != NULL ? to565(pal + sheet8[si] * 3u) : 0;
            } else {
                a->rgb565[di] = sheet[si];
            }
        }
    }
}

/* An off-palette sheet on a PAL8 screen: convert the cells built so far
 * (slots 0..done-1); everything else stays 0 like a fresh RGB565 atlas. */
static int atlas_to_565(struct TileAtlas *a, int done)
{
    const uint8_t *pal = bmp_main_palette();
    int s, x, y;

    a->rgb565 = atlas_565_buf();
    if (a->rgb565 == NULL || pal == NULL) {
        return -1;
    }
    for (s = 0; s < done; s++) {
        for (y = 0; y < DINK_TILE_PX; y++) {
            for (x = 0; x < DINK_TILE_PX; x++) {
                size_t i = (size_t)(a->slot_y[s] + y) * DINK_ATLAS_W +
                           (size_t)(a->slot_x[s] + x);

                a->rgb565[i] = to565(pal + a->pal8[i] * 3u);
            }
        }
    }
    a->pal8 = NULL;
    return 0;
}

int tiles_build_atlas(const struct MapScreen *scr, struct TileAtlas *out)
{
    int i;

    if (scr == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    /* PAL8 while every sheet is on the main palette (official data). One
     * pass: a later ts_sheet may evict an earlier sheet (LRU by bytes). */
    if (bmp_main_palette() != NULL) {
        memset(g_atlas_pal8, 0, sizeof(g_atlas_pal8));
        out->pal8 = g_atlas_pal8;
    } else {
        out->rgb565 = atlas_565_buf();
        if (out->rgb565 == NULL) {
            return -1;
        }
    }
    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        int sheet0, cell, sw, sh, ax, ay;
        uint16_t *pix = NULL;
        uint8_t *pix8 = NULL;

        tile_split(scr->t[i].square_full_idx0, &sheet0, &cell);
        if (sheet0 < 0 || sheet0 >= 41 || cell < 0 || cell >= 128) {
            tiles_free(out);
            return -1;
        }
        if (ts_sheet(sheet0, &pix, &pix8, &sw, &sh) != 0 ||
            (pix == NULL && pix8 == NULL)) {
            tiles_free(out);
            return -1;
        }
        if (out->pal8 != NULL && (pix8 == NULL || i == g_force_565_at) &&
            atlas_to_565(out, i) != 0) {
            tiles_free(out);
            return -1;
        }
        ax = (i % DINK_ATLAS_COLS) * DINK_ATLAS_STRIDE;
        ay = (i / DINK_ATLAS_COLS) * DINK_ATLAS_STRIDE;
        blit_cell(out, pix, pix8, sw, sh, cell, ax, ay);
        out->slot_x[i] = ax;
        out->slot_y[i] = ay;
        dink_cd_yield();
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

int tiles_tex_held(void)
{
    return g_tile_tex != NULL;
}

static int g_tile_pal8;

int tiles_upload_pvr(struct TileAtlas *a)
{
    if (a == NULL || (a->rgb565 == NULL && a->pal8 == NULL)) {
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
    g_tile_pal8 = a->pal8 != NULL;
    g_tile_tex = pvr_mem_malloc((size_t)DINK_ATLAS_W * DINK_ATLAS_H *
                                (g_tile_pal8 ? 1u : 2u));
    if (g_tile_tex == NULL) {
        return -1;
    }
    /* Fast twiddle (twiddle.c, same layout as pvr_txr_load_ex) straight
     * into VRAM: the atlas is bigger than the sprite scratch. Paletted must
     * be twiddled; bank 1 is the opaque main palette. */
    if (g_tile_pal8) {
        twiddle_8bpp(a->pal8, (uint16_t *)g_tile_tex, DINK_ATLAS_W,
                     DINK_ATLAS_H);
    } else {
        twiddle_16bpp(a->rgb565, (uint16_t *)g_tile_tex, DINK_ATLAS_W,
                      DINK_ATLAS_H);
    }
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
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                     g_tile_pal8
                         ? (int)(PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(1))
                         : (int)PVR_TXRFMT_RGB565,
                     DINK_ATLAS_W, DINK_ATLAS_H, g_tile_tex, PVR_FILTER_NONE);
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
