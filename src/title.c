/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "title.h"

#include "bmp.h"
#include "fs.h"
#include "rgb565.h"
#include "title_path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

void title_free(struct TitleStill *t)
{
    if (t == NULL) {
        return;
    }
    free(t->rgb565);
    t->rgb565 = NULL;
    t->w = t->h = t->npx = 0;
}

static int slurp(FILE *fp, uint8_t **out, size_t *n)
{
    long sz;

    if (fseek(fp, 0, SEEK_END) != 0) {
        return -1;
    }
    sz = ftell(fp);
    if (sz < 54 || sz > 8 * 1024 * 1024) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return -1;
    }
    *out = (uint8_t *)malloc((size_t)sz);
    if (*out == NULL) {
        return -1;
    }
    if (fread(*out, 1, (size_t)sz, fp) != (size_t)sz) {
        free(*out);
        *out = NULL;
        return -1;
    }
    *n = (size_t)sz;
    return 0;
}

int title_load(struct TitleStill *out)
{
    FILE *fp;
    uint8_t *raw = NULL;
    size_t n = 0;
    struct Bitmap bm;
    uint16_t *pix = NULL;
    int npx = 0;

    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    memset(&bm, 0, sizeof(bm));
    fp = dink_fopen(DINK_TITLE_REL, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (slurp(fp, &raw, &n) != 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (bitmap_load_mem(raw, n, &bm) != 0) {
        free(raw);
        return -1;
    }
    free(raw);
    if (rgb565_from_bitmap(&bm, &pix, &npx) != 0) {
        bitmap_free(&bm);
        return -1;
    }
    out->w = bm.w;
    out->h = bm.h;
    bitmap_free(&bm);
    out->rgb565 = pix;
    out->npx = npx;
    return 0;
}

#ifdef _arch_dreamcast
static int next_pow2(int v)
{
    int p = 8;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

int title_present_pvr(const struct TitleStill *t)
{
    int tw, th, y;
    pvr_ptr_t tex;
    uint16_t *pad;
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    float u, v;

    if (t == NULL || t->rgb565 == NULL || t->w <= 0 || t->h <= 0) {
        return -1;
    }
    /* 640×480 RGB565 = 614400 B — comment required by plan. */
    tw = next_pow2(t->w);
    th = next_pow2(t->h);
    pad = (uint16_t *)calloc((size_t)tw * (size_t)th, 2);
    if (pad == NULL) {
        return -1;
    }
    for (y = 0; y < t->h; y++) {
        memcpy(pad + (size_t)y * (size_t)tw,
               t->rgb565 + (size_t)y * (size_t)t->w,
               (size_t)t->w * 2u);
    }
    pvr_init_defaults();
    tex = pvr_mem_malloc((size_t)tw * (size_t)th * 2u);
    if (tex == NULL) {
        free(pad);
        return -1;
    }
    pvr_txr_load_ex(pad, tex, tw, th, PVR_TXRLOAD_16BPP);
    free(pad);

    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
                     tw, th, tex, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);
    u = (float)t->w / (float)tw;
    v = (float)t->h / (float)th;

    for (;;) {
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        pvr_prim(&hdr, sizeof(hdr));
        vert.argb = 0xffffffff;
        vert.oargb = 0;
        vert.z = 1.0f;
        vert.flags = PVR_CMD_VERTEX;
        vert.x = 0.0f;
        vert.y = 0.0f;
        vert.u = 0.0f;
        vert.v = 0.0f;
        pvr_prim(&vert, sizeof(vert));
        vert.x = 640.0f;
        vert.y = 0.0f;
        vert.u = u;
        vert.v = 0.0f;
        pvr_prim(&vert, sizeof(vert));
        vert.x = 0.0f;
        vert.y = 480.0f;
        vert.u = 0.0f;
        vert.v = v;
        pvr_prim(&vert, sizeof(vert));
        vert.flags = PVR_CMD_VERTEX_EOL;
        vert.x = 640.0f;
        vert.y = 480.0f;
        vert.u = u;
        vert.v = v;
        pvr_prim(&vert, sizeof(vert));
        pvr_list_finish();
        pvr_scene_finish();
    }
}
#endif
