/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sprite.h"

#include "bmp.h"
#include "ff.h"
#include "fs.h"

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
    sprite_evict_pvr(f);
    free(f->argb1555);
    memset(f, 0, sizeof(*f));
}

int sprite_pixel_opaque(uint16_t p)
{
    return (p & SPRITE_ARGB1555_OPAQUE) != 0;
}

static uint16_t pack1555(uint8_t r, uint8_t g, uint8_t b, int nocolorkey)
{
    if (!nocolorkey && r > 240 && g > 240 && b > 240) {
        return 0; /* A=0 punch-through */
    }
    return (uint16_t)(SPRITE_ARGB1555_OPAQUE | ((r >> 3) << 10) | ((g >> 3) << 5) |
                      (b >> 3));
}

static int load_seq_frame(struct SeqInfo *seq, int seqn, int frame,
                          struct SpriteFrame *out, int nocolorkey)
{
    char dir[160], base[32], name[24];
    const char *sl;
    const uint8_t *bmp = NULL;
    size_t bn = 0;
    int bmp_owned = 0;
    struct Bitmap bm;
    uint16_t *pad;
    int x, y, tw, th;
    struct FfFile *ff = NULL;

    if (seq == NULL || out == NULL || frame < 1 || seq->prefix[0] == '\0') {
        return -1;
    }
    {
        int dseq = seqn, dfr = frame;

        if (ini_resolve_frame(seqn, frame, &dseq, &dfr) != 0) {
            return -1;
        }
        if (dseq == seqn && dfr >= 1) {
            frame = dfr;
        }
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
    /* Loose BMPs (seq 456/457 arrows) have no dir.ff. Try those first so
     * we do not fopen a missing pack once per frame. */
    if (!ff_is_cached(dir)) {
        char rel[160];
        int fi, nf = 0;

        snprintf(rel, sizeof(rel), frame < 10 ? "%s0%d.bmp" : "%s%d.bmp",
                 seq->prefix, frame);
        if (dink_blob_get(rel, &bmp, &bn) == 0 && bmp != NULL && bn > 0) {
            if (seq->nframes < 1) {
                for (fi = 1; fi < DINK_MAX_FRAMES; fi++) {
                    const uint8_t *p;
                    size_t ln;
                    char r2[160];

                    snprintf(r2, sizeof(r2),
                             fi < 10 ? "%s0%d.bmp" : "%s%d.bmp", seq->prefix,
                             fi);
                    if (dink_blob_get(r2, &p, &ln) != 0) {
                        break;
                    }
                    nf = fi;
                }
                seq->nframes = ini_seq_len(seqn, nf);
                printf("seq %d nframes %d\n", seqn, seq->nframes);
            }
            goto decode;
        }
    }
    if (ff_cached(dir, &ff) != 0 || ff == NULL) {
        return -1;
    }
    if (seq->nframes < 1) {
        int nf = 0, fi;

        for (fi = 1; fi < DINK_MAX_FRAMES; fi++) {
            const uint8_t *p;
            size_t ln;
            char fn[24];

            snprintf(fn, sizeof(fn), fi < 10 ? "%s0%d.bmp" : "%s%d.bmp", base,
                     fi);
            if (ff_find(ff, fn, &p, &ln) != 0) {
                break;
            }
            nf = fi;
        }
        seq->nframes = ini_seq_len(seqn, nf);
        printf("seq %d nframes %d\n", seqn, seq->nframes);
    }
    if (ff_read_bmp(ff, name, &bmp, &bn, &bmp_owned) != 0) {
        return -1;
    }
decode:
    memset(&bm, 0, sizeof(bm));
    if (bitmap_load_mem(bmp, bn, &bm) != 0) {
        if (bmp_owned) {
            free((void *)bmp);
        }
        return -1;
    }
    tw = next_pow2(bm.w);
    th = next_pow2(bm.h);
    pad = (uint16_t *)calloc((size_t)tw * (size_t)th, 2);
    if (pad == NULL) {
        bitmap_free(&bm);
        if (bmp_owned) {
            free((void *)bmp);
        }
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
            pad[y * tw + x] = pack1555(r, g, b, nocolorkey);
        }
    }
    out->w = bm.w;
    out->h = bm.h;
    out->tw = tw;
    out->th = th;
    ini_frame_geom(seq, seqn, frame, bm.w, bm.h, &out->cx, &out->cy, &out->hl,
                   &out->ht, &out->hr, &out->hb);
    /* figure_out animation: pin omitted centers from frame 1 so later
     * frames (S1-HOLE crawl 452) keep the hole registration. */
    if (seq->reuse_off && frame == 1) {
        if (seq->cx < 1) {
            seq->cx = out->cx;
        }
        if (seq->cy < 1) {
            seq->cy = out->cy;
        }
    }
    out->argb1555 = pad;
    out->tex = NULL;
    bitmap_free(&bm);
    if (bmp_owned) {
        free((void *)bmp);
    }
    return 0;
}

int sprite_load_seq_frame(struct SeqInfo *seq, int seqn, int frame,
                          struct SpriteFrame *out)
{
    return load_seq_frame(seq, seqn, frame, out, 0);
}

/* FreeDink LEFTALIGN / blitNoColorKey: keep white paper (HUD digits, chrome). */
int sprite_load_seq_frame_nocolorkey(struct SeqInfo *seq, int seqn, int frame,
                                     struct SpriteFrame *out)
{
    return load_seq_frame(seq, seqn, frame, out, 1);
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

void sprite_evict_pvr(struct SpriteFrame *f)
{
#ifdef _arch_dreamcast
    if (f != NULL && f->tex != NULL) {
        pvr_mem_free((pvr_ptr_t)f->tex);
        f->tex = NULL;
    }
#else
    if (f != NULL) {
        f->tex = NULL;
    }
#endif
}

void sprite_drop_cpu(struct SpriteFrame *f)
{
    if (f == NULL) {
        return;
    }
    free(f->argb1555);
    f->argb1555 = NULL;
}

int sprite_upload_needed(const struct SpriteFrame *f)
{
    if (f == NULL) {
        return -1;
    }
    /* Screen CPU drop after PVR: tex lives, argb1555 is NULL. */
    if (f->tex != NULL) {
        return 0;
    }
    if (f->argb1555 == NULL) {
        return -1;
    }
    return 1;
}

#ifdef _arch_dreamcast
int sprite_upload_pvr(struct SpriteFrame *f)
{
    pvr_ptr_t tex;
    int need;

    need = sprite_upload_needed(f);
    if (need <= 0) {
        return need;
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

static void sprite_draw_pvr_clip(const struct SpriteFrame *f, float x, float y,
                                 float z, int al, int at, int ar, int ab,
                                 int size, int noclip)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    float u0, v0, u1, v1, x0, y0, x1, y1, ratio, xcompat, ycompat;
    int sl = 0, st = 0, sr = 0, sb = 0;

    if (f == NULL || f->tex == NULL) {
        return;
    }
    if (size < 1) {
        size = 100;
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
    ratio = (float)size / 100.0f;
    xcompat = (float)f->w * (ratio - 1.0f) / 2.0f;
    ycompat = (float)f->h * (ratio - 1.0f) / 2.0f;
    x0 = x - (float)f->cx - xcompat + (float)sl * ratio;
    y0 = y - (float)f->cy - ycompat + (float)st * ratio;
    x1 = x - (float)f->cx - xcompat + (float)sr * ratio;
    y1 = y - (float)f->cy - ycompat + (float)sb * ratio;
    /* get_box: skip if fully outside. noclip uses 0..640, 0..480. */
    if (noclip) {
        if (x1 <= 0.0f || y1 <= 0.0f || x0 >= 640.0f || y0 >= 480.0f) {
            return;
        }
    } else if (x1 <= 20.0f || y1 <= 0.0f || x0 >= 620.0f || y0 >= 400.0f) {
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

void sprite_draw_pvr(const struct SpriteFrame *f, float x, float y, float z)
{
    sprite_draw_pvr_clip(f, x, y, z, 0, 0, 0, 0, 100, 0);
}

void sprite_draw_pvr_noclip(const struct SpriteFrame *f, float x, float y,
                            float z)
{
    sprite_draw_pvr_clip(f, x, y, z, 0, 0, 0, 0, 100, 1);
}

void sprite_draw_pvr_alt(const struct SpriteFrame *f, float x, float y,
                         float z, int al, int at, int ar, int ab)
{
    sprite_draw_pvr_clip(f, x, y, z, al, at, ar, ab, 100, 0);
}

void sprite_draw_pvr_alt_size(const struct SpriteFrame *f, float x, float y,
                              float z, int al, int at, int ar, int ab,
                              int size)
{
    sprite_draw_pvr_clip(f, x, y, z, al, at, ar, ab, size, 0);
}

void sprite_blit_pvr(const struct SpriteFrame *f, float dx, float dy, float z)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    float u1, v1, x1, y1;

    if (f == NULL || f->tex == NULL || f->w < 1 || f->h < 1) {
        return;
    }
    pvr_poly_cxt_txr(&cxt, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB1555, f->tw, f->th,
                     (pvr_ptr_t)f->tex, PVR_FILTER_NONE);
    cxt.gen.alpha = PVR_ALPHA_ENABLE;
    cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    u1 = (float)f->w / (float)f->tw;
    v1 = (float)f->h / (float)f->th;
    x1 = dx + (float)f->w;
    y1 = dy + (float)f->h;
    vert.argb = 0xffffffff;
    vert.oargb = 0;
    vert.z = z;
    vert.flags = PVR_CMD_VERTEX;
    vert.x = dx;
    vert.y = dy;
    vert.u = 0.0f;
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x1;
    vert.y = dy;
    vert.u = u1;
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = dx;
    vert.y = y1;
    vert.u = 0.0f;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = x1;
    vert.y = y1;
    vert.u = u1;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));
}

void sprite_blit_pvr_src(const struct SpriteFrame *f, float dx, float dy,
                         float z, int sl, int st, int sr, int sb)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    float u0, v0, u1, v1, x1, y1;

    if (f == NULL || f->tex == NULL || sr <= sl || sb <= st) {
        return;
    }
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
    x1 = dx + (float)(sr - sl);
    y1 = dy + (float)(sb - st);
    vert.argb = 0xffffffff;
    vert.oargb = 0;
    vert.z = z;
    vert.flags = PVR_CMD_VERTEX;
    vert.x = dx;
    vert.y = dy;
    vert.u = u0;
    vert.v = v0;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x1;
    vert.y = dy;
    vert.u = u1;
    vert.v = v0;
    pvr_prim(&vert, sizeof(vert));
    vert.x = dx;
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
