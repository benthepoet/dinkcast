/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "font.h"

#include <stdio.h>
#include <stdlib.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    int col = -1, row = -1;
    const uint16_t *px;
    int a_col, bang_col, bang_row;

    expect(font_init() == 0, "init");
    expect(font_atlas_bytes() == DINK_FONT_ATLAS_W * DINK_FONT_ATLAS_H * 2,
           "bytes");
    expect(font_atlas_bytes() <= DINK_FONT_ATLAS_CAP, "cap 64k");
    expect(font_advance('A') == DINK_FONT_CELL, "advance");
    expect(font_glyph_cell('A', &col, &row) == 1, "A cell");
    expect(col == ('A' - 32) % 16 && row == ('A' - 32) / 16, "A pos");
    expect(font_glyph_cell(1, &col, &row) == 0, "ctrl");
    px = font_atlas_argb1555();
    expect(px != NULL, "atlas");
    a_col = ('A' - 32) % 16;
    /* 'A' has ink; space cell is empty. */
    {
        int ink = 0, sx, sy;

        for (sy = 0; sy < 8; sy++) {
            for (sx = 0; sx < 8; sx++) {
                if (px[(0 + sy) * DINK_FONT_ATLAS_W + (0 + sx)] != 0) {
                    ink = 1;
                }
            }
        }
        expect(ink == 0, "space empty");
        ink = 0;
        for (sy = 0; sy < 8; sy++) {
            for (sx = 0; sx < 8; sx++) {
                int x = a_col * 8 + sx;
                int y = (('A' - 32) / 16) * 8 + sy;

                if (px[y * DINK_FONT_ATLAS_W + x] != 0) {
                    ink = 1;
                }
            }
        }
        expect(ink == 1, "A ink");
    }
    /* '!' row 0 is 0x18: bits 3 and 4 if bit0 is left (font8x8). */
    expect(font_glyph_cell('!', &bang_col, &bang_row) == 1, "bang");
    {
        int sx, base;

        base = bang_row * 8 * DINK_FONT_ATLAS_W + bang_col * 8;
        for (sx = 0; sx < 8; sx++) {
            int on = px[base + sx] != 0;

            if (sx == 3 || sx == 4) {
                expect(on, "bang bit");
            } else {
                expect(!on, "bang gap");
            }
        }
    }
    font_free();
    expect(font_atlas_argb1555() == NULL, "free");
    printf("OK test_font\n");
    return 0;
}
