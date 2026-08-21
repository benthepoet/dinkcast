/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_SAYBOX_H
#define DINKCAST_SAYBOX_H

#include "mapscr.h"
#include "player.h"

#include <stdint.h>

/* FreeDink say_text + text_brain: owner.x-75, owner.y-100 each
 * frame (narrator 1000 stays put). Wrap 150. text_draw print_text_wrap
 * hcenter=1 in that box. */
#define DINK_SAY_XOFF 75
#define DINK_SAY_YOFF 100
#define DINK_SAY_BOX_W 150
#define DINK_SAY_BOX_H 150
#define DINK_SAY_PLAYX 620

void saybox_bind(const struct MapScreen *scr, struct Player *pl);
void saybox_bind_live_xy(int (*fn)(int slot, int *x, int *y));
void saybox_set(const char *text, int sprite);
void saybox_clear(void);
int saybox_active(void);
const char *saybox_text(void);
int saybox_x(void);
int saybox_y(void);
/* First line of `line` (up to \\n). print_text_wrap: left + boxw/2 - w/2. */
int saybox_line_x(const char *line);
int saybox_color(void);
/* FreeDink gfx_fonts_init_colors index 0–15 as 0xAARRGGBB. */
uint32_t saybox_argb(int color);

#ifdef _arch_dreamcast
int saybox_upload(void);
void saybox_evict(void);
void saybox_draw_pvr(float z);
void saybox_draw_choices_pvr(float z);
/* FreeDink text_draw damage/exp number (brain 8, damage != -1). */
void saybox_draw_num_pvr(int x, int y, int num, float z);
#endif

#endif
