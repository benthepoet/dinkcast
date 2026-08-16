/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_BMP_H
#define DINKCAST_BMP_H

#include <stddef.h>
#include <stdint.h>

#define DINK_BMP_MAX_RGB565 (1500000)

enum BitmapFmt { DINK_BMP_PAL8 = 8, DINK_BMP_BGR24 = 24 };

struct Bitmap {
    int w;
    int h;
    int stride;
    int bpp;
    uint8_t *pixels;
    uint8_t pal[256 * 3];
};

void bitmap_free(struct Bitmap *bm);
int bitmap_load_file(const char *path, struct Bitmap *out);
int bitmap_load_mem(const uint8_t *data, size_t n, struct Bitmap *out);

#endif
