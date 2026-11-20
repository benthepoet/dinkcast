/* SPDX-License-Identifier: GPL-3.0-or-later */
/* twiddle_8bpp / twiddle_16bpp produce exactly the layout of KOS
 * pvr_txr_load_ex (8bpp and 16bpp loops copied below as the reference,
 * writing to RAM instead of VRAM), for square, wide and tall sizes. */
#include "twiddle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- reference: KOS kernel/arch/dreamcast/hardware/pvr/pvr_texture.c --- */
#define TWIDTAB(x)                                                             \
    ((x & 1) | ((x & 2) << 1) | ((x & 4) << 2) | ((x & 8) << 3) |              \
     ((x & 16) << 4) | ((x & 32) << 5) | ((x & 64) << 6) | ((x & 128) << 7) |  \
     ((x & 256) << 8) | ((x & 512) << 9))
#define TWIDOUT(x, y) (TWIDTAB((y)) | (TWIDTAB((x)) << 1))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void ref_8bpp(const uint8_t *pixels, uint16_t *vtex, uint32_t w,
                     uint32_t h)
{
    uint32_t x, y, yout, min = MIN(w, h), mask = min - 1;

    for (y = 0; y < h; y += 2) {
        yout = y;
        for (x = 0; x < w; x++) {
            vtex[TWIDOUT((yout & mask) / 2, x & mask) +
                 (x / min + yout / min) * min * min / 2] =
                pixels[y * w + x] | (pixels[(y + 1) * w + x] << 8);
        }
    }
}

static void ref_16bpp(const uint16_t *pixels, uint16_t *vtex, uint32_t w,
                      uint32_t h)
{
    uint32_t x, y, yout, min = MIN(w, h), mask = min - 1;

    for (y = 0; y < h; y++) {
        yout = y;
        for (x = 0; x < w; x++) {
            vtex[TWIDOUT(x & mask, yout & mask) +
                 (x / min + yout / min) * min * min] = pixels[y * w + x];
        }
    }
}
/* --- end reference --- */

int main(void)
{
    static const int sz[][2] = {{8, 8},     {16, 8},    {8, 16},
                                {64, 64},   {256, 256}, {128, 512},
                                {512, 128}, {64, 1024}, {1024, 512},
                                {512, 512}, {32, 256},  {8, 1024},
                                {1024, 8},  {1024, 1024}, {256, 8},
                                {8, 256}};
    int i, fails = 0;

    srand(1234);
    for (i = 0; i < (int)(sizeof(sz) / sizeof(sz[0])); i++) {
        int w = sz[i][0], h = sz[i][1], k;
        size_t n = (size_t)w * (size_t)h;
        uint8_t *s8 = malloc(n);
        uint16_t *s16 = malloc(n * 2);
        uint16_t *a = calloc(n, 2), *b = calloc(n, 2);

        for (k = 0; k < (int)n; k++) {
            s8[k] = (uint8_t)rand();
            s16[k] = (uint16_t)rand();
        }
        ref_8bpp(s8, a, (uint32_t)w, (uint32_t)h);
        twiddle_8bpp(s8, b, w, h);
        /* Whole buffer: a stray write past the texture also fails. */
        if (memcmp(a, b, n * 2) != 0) {
            printf("FAIL 8bpp %dx%d\n", w, h);
            fails++;
        }
        memset(a, 0, n * 2);
        memset(b, 0, n * 2);
        ref_16bpp(s16, a, (uint32_t)w, (uint32_t)h);
        twiddle_16bpp(s16, b, w, h);
        if (memcmp(a, b, n * 2) != 0) {
            printf("FAIL 16bpp %dx%d\n", w, h);
            fails++;
        }
        free(s8);
        free(s16);
        free(a);
        free(b);
    }
    if (fails) {
        return 1;
    }
    if (twiddle_size_ok(2048, 8) || twiddle_size_ok(8, 4) ||
        !twiddle_size_ok(1024, 8) || twiddle_size_ok(24, 8)) {
        printf("FAIL twiddle_size_ok\n");
        return 1;
    }
    printf("OK test_twiddle (%d sizes, 8bpp + 16bpp match KOS)\n", i);
    return 0;
}
