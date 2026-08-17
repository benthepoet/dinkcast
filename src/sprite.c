/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sprite.h"

#include "bmp.h"
#include "ff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

static int next_pow2(int v)
{
    int p = 8;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

void sprite_frame_free(struct SpriteFrame *f)
{
    if (f == NULL) {
        return;
    }
#ifdef _arch_dreamcast
    sprite_evict_pvr(f);
#endif
    free(f->argb1555);
    memset(f, 0, sizeof(*f));
}

static uint16_t pack1555(uint8_t r, uint8_t g, uint8_t b)
{
    if (r > 240 && g > 240 && b > 240) {
        return 0;
    }
    return (uint16_t)(0x8000u | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
}

int sprite_load_seq_frame(const struct SeqInfo *seq, int frame,
                          struct SpriteFrame *out)
{
    char dir[160], base[32], name[24];
    const char *sl;
    struct FfFile ff;
    const uint8_t *bmp = NULL;
    size_t bn = 0;
    struct Bitmap bm;
    uint16_t *pad;
    int x, y, tw, th;

    if (seq == NULL || out == NULL || frame < 1 || seq->prefix[0] == '\0') {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    sl = strrchr(seq->prefix, '/');
    if (sl == NULL) {
        return -1;
    }
    snprintf(dir, sizeof(dir), "%.*s/dir.ff", (int)(sl - seq->prefix), seq->prefix);
    snprintf(base, sizeof(base), "%s", sl + 1);
    snprintf(name, sizeof(name), frame < 10 ? "%s0%d.bmp" : "%s%d.bmp", base,
             frame);
    memset(&ff, 0, sizeof(ff));
    if (ff_load_rel(dir, &ff) != 0) {
        return -1;
    }
    if (ff_find(&ff, name, &bmp, &bn) != 0) {
        ff_free(&ff);
        return -1;
    }
    memset(&bm, 0, sizeof(bm));
    if (bitmap_load_mem(bmp, bn, &bm) != 0) {
        ff_free(&ff);
        return -1;
    }
    ff_free(&ff);
    tw = next_pow2(bm.w);
    th = next_pow2(bm.h);
    pad = (uint16_t *)calloc((size_t)tw * (size_t)th, 2);
    if (pad == NULL) {
        bitmap_free(&bm);
        return -1;
    }
    for (y = 0; y < bm.h; y++) {
        for (x = 0; x < bm.w; x++) {
            uint8_t r, g, b;
            int i = y * bm.w + x;
            if (bm.bpp == 8) {
                const uint8_t *pal = bm.pal + (size_t)bm.pixels[i] * 3u;
                r = pal[0];
                g = pal[1];
                b = pal[2];
            } else {
                r = bm.pixels[i * 3];
                g = bm.pixels[i * 3 + 1];
                b = bm.pixels[i * 3 + 2];
            }
            pad[y * tw + x] = pack1555(r, g, b);
        }
    }
    out->w = bm.w;
    out->h = bm.h;
    out->tw = tw;
    out->th = th;
    out->cx = seq->cx;
    out->cy = seq->cy;
    out->argb1555 = pad;
    bitmap_free(&bm);
    return 0;
}

#ifdef _arch_dreamcast
static pvr_ptr_t g_spr_tex;

int sprite_upload_pvr(struct SpriteFrame *f)
{
    if (f == NULL || f->argb1555 == NULL) {
        return -1;
    }
    sprite_evict_pvr(f);
    g_spr_tex = pvr_mem_malloc((size_t)f->tw * (size_t)f->th * 2u);
    if (g_spr_tex == NULL) {
        return -1;
    }
    pvr_txr_load_ex(f->argb1555, g_spr_tex, f->tw, f->th, PVR_TXRLOAD_16BPP);
    return 0;
}

void sprite_evict_pvr(struct SpriteFrame *f)
{
    (void)f;
    if (g_spr_tex != NULL) {
        pvr_mem_free(g_spr_tex);
        g_spr_tex = NULL;
    }
}

void sprite_draw_pvr(const struct SpriteFrame *f, float x, float y, float z)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    float u, v, x0, y0, x1, y1;

    if (f == NULL || g_spr_tex == NULL) {
        return;
    }
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555, f->tw, f->th,
                     g_spr_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    u = (float)f->w / (float)f->tw;
    v = (float)f->h / (float)f->th;
    x0 = x - (float)f->cx;
    y0 = y - (float)f->cy;
    x1 = x0 + (float)f->w;
    y1 = y0 + (float)f->h;
    vert.argb = 0xffffffff;
    vert.oargb = 0;
    vert.z = z;
    vert.flags = PVR_CMD_VERTEX;
    vert.x = x0;
    vert.y = y0;
    vert.u = 0;
    vert.v = 0;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x1;
    vert.y = y0;
    vert.u = u;
    vert.v = 0;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x0;
    vert.y = y1;
    vert.u = 0;
    vert.v = v;
    pvr_prim(&vert, sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = x1;
    vert.y = y1;
    vert.u = u;
    vert.v = v;
    pvr_prim(&vert, sizeof(vert));
}
#endif
