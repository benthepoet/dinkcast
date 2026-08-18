/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_FONT_H
#define DINKCAST_FONT_H

#include <stdint.h>

/* Bite 13.1: official data has no font BMP. Embedded IBM VGA 8x8
 * (public domain). Not LiberationSans. Atlas ≤ 64 KB. */
#define DINK_FONT_CELL 8
#define DINK_FONT_ATLAS_W 128
#define DINK_FONT_ATLAS_H 64
#define DINK_FONT_ATLAS_CAP 65536
#define DINK_FONT_FIRST 32
#define DINK_FONT_LAST 126

int font_init(void);
void font_free(void);
int font_atlas_w(void);
int font_atlas_h(void);
int font_atlas_bytes(void);
const uint16_t *font_atlas_argb1555(void);
int font_advance(int ch);
/* col/row in the 16-wide sheet; 0 if not a glyph. */
int font_glyph_cell(int ch, int *col, int *row);

#endif
