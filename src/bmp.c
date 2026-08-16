/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bmp.h"

#include "le.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bitmap_free(struct Bitmap *bm)
{
    if (bm == NULL) {
        return;
    }
    free(bm->pixels);
    bm->pixels = NULL;
    bm->w = bm->h = bm->stride = bm->bpp = 0;
}

static int load_body(const uint8_t *data, size_t n, struct Bitmap *out)
{
    uint16_t magic, planes, bpp;
    int32_t w, h;
    uint32_t off_bits, compression, dib;
    int abs_h, topdown, row_src, i, y, x;
    const uint8_t *src;
    uint8_t *dst;

    memset(out, 0, sizeof(*out));
    if (n < 54 || le_u16(data, n, 0, &magic) != 0 || magic != 0x4D42) {
        return -1;
    }
    if (le_u32(data, n, 10, &off_bits) != 0 ||
        le_u32(data, n, 14, &dib) != 0 ||
        le_i32(data, n, 18, &w) != 0 ||
        le_i32(data, n, 22, &h) != 0 ||
        le_u16(data, n, 26, &planes) != 0 ||
        le_u16(data, n, 28, &bpp) != 0 ||
        le_u32(data, n, 30, &compression) != 0) {
        return -1;
    }
    if (dib < 40 || planes != 1 || compression != 0 || w <= 0) {
        return -1;
    }
    if (bpp != 8 && bpp != 24) {
        return -1;
    }
    topdown = (h < 0);
    abs_h = topdown ? -h : h;
    if (abs_h <= 0 || w > 4096 || abs_h > 4096) {
        return -1;
    }
    if ((uint64_t)w * (uint64_t)abs_h * 2ull > (uint64_t)DINK_BMP_MAX_RGB565) {
        return -1;
    }
    out->w = w;
    out->h = abs_h;
    out->bpp = (int)bpp;
    if (bpp == 8) {
        out->stride = w;
        if (off_bits < 14 + 40 + 256u * 4u) {
            return -1;
        }
        for (i = 0; i < 256; i++) {
            size_t po = 14 + 40 + (size_t)i * 4u;
            if (po + 3 > n) {
                return -1;
            }
            out->pal[i * 3 + 0] = data[po + 2];
            out->pal[i * 3 + 1] = data[po + 1];
            out->pal[i * 3 + 2] = data[po + 0];
        }
    } else {
        out->stride = w * 3;
    }
    row_src = (bpp == 8) ? ((w + 3) & ~3) : ((w * 3 + 3) & ~3);
    if (off_bits > n) {
        return -1;
    }
    out->pixels = (uint8_t *)calloc((size_t)out->stride * (size_t)abs_h, 1);
    if (out->pixels == NULL) {
        return -1;
    }
    for (y = 0; y < abs_h; y++) {
        int src_y = topdown ? y : (abs_h - 1 - y);
        size_t so = (size_t)off_bits + (size_t)src_y * (size_t)row_src;
        if (so + (size_t)row_src > n) {
            bitmap_free(out);
            return -1;
        }
        src = data + so;
        dst = out->pixels + (size_t)y * (size_t)out->stride;
        if (bpp == 8) {
            memcpy(dst, src, (size_t)w);
        } else {
            for (x = 0; x < w; x++) {
                dst[x * 3 + 0] = src[x * 3 + 2];
                dst[x * 3 + 1] = src[x * 3 + 1];
                dst[x * 3 + 2] = src[x * 3 + 0];
            }
        }
    }
    return 0;
}

int bitmap_load_mem(const uint8_t *data, size_t n, struct Bitmap *out)
{
    if (data == NULL || out == NULL) {
        return -1;
    }
    return load_body(data, n, out);
}

int bitmap_load_file(const char *path, struct Bitmap *out)
{
    FILE *fp;
    uint8_t *buf = NULL;
    long sz;
    int rc = -1;

    if (path == NULL || out == NULL) {
        return -1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 54 || sz > 8 * 1024 * 1024) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    buf = (uint8_t *)malloc((size_t)sz);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }
    if (fread_exact(fp, buf, (size_t)sz) == 0) {
        rc = bitmap_load_mem(buf, (size_t)sz, out);
    }
    free(buf);
    fclose(fp);
    return rc;
}
