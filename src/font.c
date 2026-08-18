/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "font.h"

#include <string.h>

#define FONT_INK 0xFFFFu /* ARGB1555 opaque white */

#include "font_glyphs.inc"

static uint16_t g_atlas[DINK_FONT_ATLAS_W * DINK_FONT_ATLAS_H];
static int g_ready;

int font_init(void)
{
    int ch, r, c, x, y;

    memset(g_atlas, 0, sizeof(g_atlas));
    for (ch = DINK_FONT_FIRST; ch <= DINK_FONT_LAST; ch++) {
        int gi = ch - DINK_FONT_FIRST;
        int col = gi % 16;
        int row = gi / 16;

        for (r = 0; r < DINK_FONT_CELL; r++) {
            uint8_t bits = k_gl[gi][r];

            for (c = 0; c < DINK_FONT_CELL; c++) {
                /* font8x8_basic: bit 0 is the leftmost pixel. */
                if ((bits & (1u << c)) == 0) {
                    continue;
                }
                x = col * DINK_FONT_CELL + c;
                y = row * DINK_FONT_CELL + r;
                g_atlas[y * DINK_FONT_ATLAS_W + x] = FONT_INK;
            }
        }
    }
    g_ready = 1;
    return 0;
}

void font_free(void)
{
    g_ready = 0;
}

int font_atlas_w(void)
{
    return DINK_FONT_ATLAS_W;
}

int font_atlas_h(void)
{
    return DINK_FONT_ATLAS_H;
}

int font_atlas_bytes(void)
{
    return (int)sizeof(g_atlas);
}

const uint16_t *font_atlas_argb1555(void)
{
    return g_ready ? g_atlas : NULL;
}

int font_advance(int ch)
{
    if (ch < DINK_FONT_FIRST || ch > DINK_FONT_LAST) {
        return DINK_FONT_CELL;
    }
    return DINK_FONT_CELL;
}

int font_glyph_cell(int ch, int *col, int *row)
{
    int gi;

    if (ch < DINK_FONT_FIRST || ch > DINK_FONT_LAST) {
        if (col != NULL) {
            *col = 0;
        }
        if (row != NULL) {
            *row = 0;
        }
        return 0;
    }
    gi = ch - DINK_FONT_FIRST;
    if (col != NULL) {
        *col = gi % 16;
    }
    if (row != NULL) {
        *row = gi / 16;
    }
    return 1;
}
