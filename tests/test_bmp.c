/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmp.h"
#include "rgb565.h"

static void wr_u16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}

static void wr_u32(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

/* 2x2 24-bit BGR, bottom-up, no extra pad (row=6, pad to 8). */
static int make_bgr24(uint8_t **out, size_t *n)
{
    uint8_t *b = (uint8_t *)calloc(1, 128);
    int row = (2 * 3 + 3) & ~3;

    if (b == NULL) {
        return -1;
    }
    b[0] = 'B';
    b[1] = 'M';
    wr_u32(b + 10, 54);
    wr_u32(b + 14, 40);
    wr_u32(b + 18, 2);
    wr_u32(b + 22, 2);
    wr_u16(b + 26, 1);
    wr_u16(b + 28, 24);
    /* bottom row: red, green; top row: blue, white */
    b[54 + 0] = 0;
    b[54 + 1] = 0;
    b[54 + 2] = 255;
    b[54 + 3] = 0;
    b[54 + 4] = 255;
    b[54 + 5] = 0;
    b[54 + row + 0] = 255;
    b[54 + row + 1] = 0;
    b[54 + row + 2] = 0;
    b[54 + row + 3] = 255;
    b[54 + row + 4] = 255;
    b[54 + row + 5] = 255;
    *out = b;
    *n = 54 + (size_t)row * 2;
    return 0;
}

static int make_pal8(uint8_t **out, size_t *n)
{
    uint8_t *b = (uint8_t *)calloc(1, 14 + 40 + 1024 + 16);
    int i, row = 4;
    size_t pix;

    if (b == NULL) {
        return -1;
    }
    b[0] = 'B';
    b[1] = 'M';
    wr_u32(b + 10, 14 + 40 + 1024);
    wr_u32(b + 14, 40);
    wr_u32(b + 18, 2);
    wr_u32(b + 22, 2);
    wr_u16(b + 26, 1);
    wr_u16(b + 28, 8);
    for (i = 0; i < 256; i++) {
        size_t o = 54 + (size_t)i * 4;
        b[o + 0] = (uint8_t)i;
        b[o + 1] = 0;
        b[o + 2] = 0;
        b[o + 3] = 0;
    }
    pix = 14 + 40 + 1024;
    b[pix + 0] = 1;
    b[pix + 1] = 2; /* bottom */
    b[pix + row + 0] = 3;
    b[pix + row + 1] = 4; /* top */
    *out = b;
    *n = pix + (size_t)row * 2;
    return 0;
}

int main(void)
{
    uint8_t *raw = NULL;
    size_t n = 0;
    struct Bitmap bm;
    uint16_t *pix = NULL;
    int npx = 0;

    if (make_bgr24(&raw, &n) != 0 || bitmap_load_mem(raw, n, &bm) != 0) {
        fprintf(stderr, "FAIL 24\n");
        return 1;
    }
    if (bm.w != 2 || bm.h != 2 || bm.bpp != 24) {
        fprintf(stderr, "FAIL 24 hdr\n");
        return 1;
    }
    /* top-left should be blue (top row stored last in file) */
    if (bm.pixels[0] != 0 || bm.pixels[1] != 0 || bm.pixels[2] != 255) {
        fprintf(stderr, "FAIL 24 pixel %u %u %u\n", bm.pixels[0], bm.pixels[1],
                bm.pixels[2]);
        return 1;
    }
    if (rgb565_from_bitmap(&bm, &pix, &npx) != 0 || npx != 4) {
        fprintf(stderr, "FAIL 24 rgb\n");
        return 1;
    }
    if (pix[0] != DINK_RGB565(0, 0, 255)) {
        fprintf(stderr, "FAIL 24 565 %04x\n", pix[0]);
        return 1;
    }
    free(pix);
    bitmap_free(&bm);
    free(raw);

    if (make_pal8(&raw, &n) != 0 || bitmap_load_mem(raw, n, &bm) != 0) {
        fprintf(stderr, "FAIL 8\n");
        return 1;
    }
    if (bm.w != 2 || bm.h != 2 || bm.bpp != 8 || bm.pixels[0] != 3) {
        fprintf(stderr, "FAIL 8 hdr/pix\n");
        return 1;
    }
    bitmap_free(&bm);
    free(raw);

    /* 65536×65536 header must not wrap the size cap (SH-4 size_t is 32-bit). */
    {
        uint8_t huge[54];
        memset(huge, 0, sizeof(huge));
        huge[0] = 'B';
        huge[1] = 'M';
        wr_u32(huge + 10, 54);
        wr_u32(huge + 14, 40);
        wr_u32(huge + 18, 65536);
        wr_u32(huge + 22, 65536);
        wr_u16(huge + 26, 1);
        wr_u16(huge + 28, 24);
        if (bitmap_load_mem(huge, sizeof(huge), &bm) == 0) {
            fprintf(stderr, "FAIL wrap 65536\n");
            return 1;
        }
    }

    printf("OK test_bmp\n");
    return 0;
}
