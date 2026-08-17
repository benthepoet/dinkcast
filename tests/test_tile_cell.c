/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bmp.h"
#include "tiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal 8-bit 50×50 BMP: palette index 3 = known RGB, pixel (0,0)=3. */
static void write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void write_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

int main(void)
{
    uint8_t bmp[54 + 1024 + 52 * 50];
    uint8_t rgb[3];
    struct Bitmap bm;
    const uint8_t *pal;

    memset(bmp, 0, sizeof(bmp));
    bmp[0] = 'B';
    bmp[1] = 'M';
    write_u32(bmp + 10, 54 + 1024);
    write_u32(bmp + 14, 40);
    write_u32(bmp + 18, 50);
    write_u32(bmp + 22, 50);
    write_u16(bmp + 26, 1);
    write_u16(bmp + 28, 8);
    bmp[54 + 3 * 4 + 0] = 17; /* B */
    bmp[54 + 3 * 4 + 1] = 34; /* G */
    bmp[54 + 3 * 4 + 2] = 51; /* R */
    /* bottom-up: last row first; pixel (0,0) is last row */
    bmp[54 + 1024 + 52 * 49] = 3;

    if (tiles_cell00_rgb(bmp, sizeof(bmp), rgb) != 0) {
        fprintf(stderr, "FAIL tiles_cell00_rgb\n");
        return 1;
    }
    memset(&bm, 0, sizeof(bm));
    if (bitmap_load_mem(bmp, sizeof(bmp), &bm) != 0) {
        fprintf(stderr, "FAIL bitmap\n");
        return 1;
    }
    pal = bm.pal + (size_t)bm.pixels[0] * 3u;
    if (rgb[0] != pal[0] || rgb[1] != pal[1] || rgb[2] != pal[2]) {
        fprintf(stderr, "FAIL crop mismatch %u %u %u vs %u %u %u\n", rgb[0],
                rgb[1], rgb[2], pal[0], pal[1], pal[2]);
        bitmap_free(&bm);
        return 1;
    }
    if (rgb[0] != 51 || rgb[1] != 34 || rgb[2] != 17) {
        fprintf(stderr, "FAIL expected palette 51,34,17 got %u,%u,%u\n", rgb[0],
                rgb[1], rgb[2]);
        bitmap_free(&bm);
        return 1;
    }
    bitmap_free(&bm);
    printf("OK test_tile_cell\n");
    return 0;
}
