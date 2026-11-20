/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "twiddle.h"

static uint32_t g_tw[1024]; /* bits of i spread to the even positions */
static int g_tw_ready;

static void tw_init(void)
{
    int i, b;

    for (i = 0; i < 1024; i++) {
        uint32_t v = 0;

        for (b = 0; b < 10; b++) {
            if (i & (1 << b)) {
                v |= 1u << (2 * b);
            }
        }
        g_tw[i] = v;
    }
    g_tw_ready = 1;
}

static int log2i(int v)
{
    int s = 0;

    while ((1 << s) < v) {
        s++;
    }
    return s;
}

int twiddle_size_ok(int w, int h)
{
    return w >= 8 && h >= 8 && w <= 1024 && h <= 1024 && (w & (w - 1)) == 0 &&
           (h & (h - 1)) == 0;
}

void twiddle_16bpp(const uint16_t *src, uint16_t *dst, int w, int h)
{
    int min = w < h ? w : h, mask = min - 1, sh = log2i(min);
    uint32_t block = (uint32_t)min * (uint32_t)min;
    int x, y;

    if (!g_tw_ready) {
        tw_init();
    }
    for (y = 0; y < h; y++) {
        uint32_t ty = g_tw[y & mask];
        uint32_t by = (uint32_t)(y >> sh);
        const uint16_t *row = src + (uint32_t)y * (uint32_t)w;

        for (x = 0; x < w; x++) {
            uint32_t i = (ty | ((uint32_t)g_tw[x & mask] << 1)) +
                         (((uint32_t)(x >> sh) + by) * block);

            dst[i] = row[x];
        }
    }
}

void twiddle_8bpp(const uint8_t *src, uint16_t *dst, int w, int h)
{
    int min = w < h ? w : h, mask = min - 1, sh = log2i(min);
    uint32_t half = ((uint32_t)min * (uint32_t)min) >> 1;
    int x, y;

    if (!g_tw_ready) {
        tw_init();
    }
    for (y = 0; y < h; y += 2) {
        /* KOS: TWIDOUT((y & mask) / 2, x & mask) = tw[x] | tw[y/2] << 1 */
        uint32_t ty = (uint32_t)g_tw[(y & mask) >> 1] << 1;
        uint32_t by = (uint32_t)(y >> sh);
        const uint8_t *r0 = src + (uint32_t)y * (uint32_t)w;
        const uint8_t *r1 = r0 + w;

        for (x = 0; x < w; x++) {
            uint32_t i = (g_tw[x & mask] | ty) +
                         (((uint32_t)(x >> sh) + by) * half);

            dst[i] = (uint16_t)(r0[x] | (r1[x] << 8));
        }
    }
}
