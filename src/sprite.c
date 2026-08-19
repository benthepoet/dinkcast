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

int sprite_pixel_opaque(uint16_t p)
{
    return (p & SPRITE_ARGB1555_OPAQUE) != 0;
}

static uint16_t pack1555(uint8_t r, uint8_t g, uint8_t b)
{
    if (r > 240 && g > 240 && b > 240) {
        return 0; /* A=0 punch-through */
    }
    return (uint16_t)(SPRITE_ARGB1555_OPAQUE | ((r >> 3) << 10) | ((g >> 3) << 5) |
                      (b >> 3));
}

int sprite_load_seq_frame(const struct SeqInfo *seq, int seqn, int frame,
                          struct SpriteFrame *out)
{
    char dir[160], base[32], name[24];
    const char *sl;
    const uint8_t *bmp = NULL;
    size_t bn = 0;
    struct Bitmap bm;
    uint16_t *pad;
    int x, y, tw, th;
    struct FfFile *ff = NULL;

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
    if (ff_cached(dir, &ff) != 0 || ff == NULL) {
        return -1;
    }
    if (ff_find(ff, name, &bmp, &bn) != 0) {
        return -1;
    }
    memset(&bm, 0, sizeof(bm));
    if (bitmap_load_mem(bmp, bn, &bm) != 0) {
        return -1;
    }
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
    ini_frame_geom(seq, seqn, frame, bm.w, bm.h, &out->cx, &out->cy, &out->hl,
                   &out->ht, &out->hr, &out->hb);
    out->argb1555 = pad;
    out->tex = NULL;
    bitmap_free(&bm);
    return 0;
}

int sprite_alt_src(int fw, int fh, int al, int at, int ar, int ab, int *sl,
                   int *st, int *sr, int *sb)
{
    if (sl == NULL || st == NULL || sr == NULL || sb == NULL || fw < 1 ||
        fh < 1) {
        return 0;
    }
    *sl = 0;
    *st = 0;
    *sr = fw;
    *sb = fh;
    /* get_box: alt applies if right||left||top (not bottom alone). */
    if (ar == 0 && al == 0 && at == 0) {
        return 0;
    }
    if (al < 0) {
        al = 0;
    }
    if (al > fw) {
        al = fw;
    }
    if (at < 0) {
        at = 0;
    }
    if (at > fh) {
        at = fh;
    }
    if (ar < 0) {
        ar = 0;
    }
    if (ar > fw) {
        ar = fw;
    }
    if (ab < 0) {
        ab = 0;
    }
    if (ab > fh) {
        ab = fh;
    }
    *sl = al;
    *st = at;
    *sr = ar;
    *sb = ab;
    return 1;
}

#ifdef _arch_dreamcast
int sprite_upload_pvr(struct SpriteFrame *f)
{
    pvr_ptr_t tex;

    if (f == NULL || f->argb1555 == NULL) {
        return -1;
    }
    sprite_evict_pvr(f);
    tex = pvr_mem_malloc((size_t)f->tw * (size_t)f->th * 2u);
    if (tex == NULL) {
        return -1;
    }
    pvr_txr_load_ex(f->argb1555, tex, f->tw, f->th, PVR_TXRLOAD_16BPP);
    f->tex = tex;
    return 0;
}

void sprite_evict_pvr(struct SpriteFrame *f)
{
    if (f != NULL && f->tex != NULL) {
        pvr_mem_free((pvr_ptr_t)f->tex);
        f->tex = NULL;
    }
}

void sprite_draw_pvr(const struct SpriteFrame *f, float x, float y, float z)
{
    sprite_draw_pvr_alt(f, x, y, z, 0, 0, 0, 0);
}

void sprite_draw_pvr_alt(const struct SpriteFrame *f, float x, float y,
                         float z, int al, int at, int ar, int ab)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    float u0, v0, u1, v1, x0, y0, x1, y1;
    int sl = 0, st = 0, sr = 0, sb = 0;

    if (f == NULL || f->tex == NULL) {
        return;
    }
    (void)sprite_alt_src(f->w, f->h, al, at, ar, ab, &sl, &st, &sr, &sb);
    if (sr <= sl || sb <= st) {
        return;
    }
    /* 1-bit alpha: punch-through list. TR still writes A=0 as black. */
    pvr_poly_cxt_txr(&cxt, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB1555, f->tw, f->th,
                     (pvr_ptr_t)f->tex, PVR_FILTER_NONE);
    cxt.gen.alpha = PVR_ALPHA_ENABLE;
    cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    u0 = (float)sl / (float)f->tw;
    v0 = (float)st / (float)f->th;
    u1 = (float)sr / (float)f->tw;
    v1 = (float)sb / (float)f->th;
    x0 = x - (float)f->cx + (float)sl;
    y0 = y - (float)f->cy + (float)st;
    x1 = x - (float)f->cx + (float)sr;
    y1 = y - (float)f->cy + (float)sb;
    /* get_box: skip if fully outside playl..playx, 0..playy */
    if (x1 <= 20.0f || y1 <= 0.0f || x0 >= 620.0f || y0 >= 400.0f) {
        return;
    }
    vert.argb = 0xffffffff;
    vert.oargb = 0;
    vert.z = z;
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
#endif
