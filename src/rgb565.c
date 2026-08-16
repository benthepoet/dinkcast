/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rgb565.h"

#include <stdlib.h>

int rgb565_from_bitmap(const struct Bitmap *bm, uint16_t **out, int *npx)
{
    int n, i;
    uint16_t *p;

    if (bm == NULL || bm->pixels == NULL || out == NULL || npx == NULL) {
        return -1;
    }
    if (bm->w <= 0 || bm->h <= 0) {
        return -1;
    }
    n = bm->w * bm->h;
    if ((size_t)n * 2u > (size_t)DINK_BMP_MAX_RGB565) {
        return -1;
    }
    p = (uint16_t *)malloc((size_t)n * sizeof(uint16_t));
    if (p == NULL) {
        return -1;
    }
    if (bm->bpp == 8) {
        for (i = 0; i < n; i++) {
            const uint8_t *pal = bm->pal + (size_t)bm->pixels[i] * 3u;
            p[i] = DINK_RGB565(pal[0], pal[1], pal[2]);
        }
    } else if (bm->bpp == 24) {
        for (i = 0; i < n; i++) {
            const uint8_t *c = bm->pixels + (size_t)i * 3u;
            p[i] = DINK_RGB565(c[0], c[1], c[2]);
        }
    } else {
        free(p);
        return -1;
    }
    *out = p;
    *npx = n;
    return 0;
}
