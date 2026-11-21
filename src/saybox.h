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
/* FreeDink text.cpp add_text_sprite. */
#define DINK_SAY_TEXT_TIMER 77
#define DINK_SAY_TEXT_MIN 2700

void saybox_bind(const struct MapScreen *scr, struct Player *pl);
void saybox_bind_live_xy(int (*fn)(int slot, int *x, int *y));
/* 1 if the owner sprite is still active. NULL skips the text_brain drop. */
void saybox_bind_alive(int (*fn)(int slot));
/* FreeDink dc_say: one text sprite per call. Owner != 1000 replaces
 * that sprite's current line only. script is the creating fiber
 * (add_text_sprite's script), 0 if none. */
void saybox_set(const char *text, int sprite);
/* Text-sprite id (400 + slot), or 0 if the line was not created.
 * FreeDink returns the sprite from add_text_sprite and stores it in
 * &last_text. */
int saybox_set_from(const char *text, int sprite, int script);
int saybox_last_id(void);
/* dc_sp_kill: 1 if id is a live text line. */
int saybox_set_ttl(int id, int ttl);
/* FreeDink say_text_xy: text_owner 1000, box at (x,y) size 620×400. */
void saybox_set_xy(const char *text, int x, int y);
int saybox_set_xy_from(const char *text, int x, int y, int script);
/* Drop say_stop callbacks for this fiber. The line stays (kill_returning_stuff). */
void saybox_clear_callback(int fiber);
void saybox_forget_script(int script);
/* kill_text_owned_by: drop the line, do not run its callback. */
void saybox_kill_owner(int sprite);
/* ACTION_TALK: kill_ttl = 1 on lines this script created. saybox_tick
 * runs the callbacks. */
void saybox_expire_script(int script, int now_ms);
/* FreeDink dc_say_stop / dc_say_stop_npc. 1 if the script must yield.
 * npc is a no-op when that sprite already has a line, and does not
 * become play.last_talk. */
int saybox_stop(const char *text, int sprite, int fiber, int npc);
/* FreeDink dc_say_stop_xy: last_talk, no kill of other owners. */
int saybox_stop_xy(const char *text, int x, int y, int fiber);
/* FreeDink live_sprite_is_expired: 1 if this tick killed at least one line.
 * Callbacks are saybox_expired_fiber, not every waiting script. */
int saybox_tick(int now_ms);
int saybox_expired_n(void);
int saybox_expired_fiber(int i);
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
/* First glyph x: centered in the 50 px box, then 1 px left. */
int saybox_num_x(int x, int num);
/* FreeDink text_draw: damage is white, experience (brain_parm 5000) is
 * color 14. brightness < 256 forces color 15. */
uint32_t saybox_num_argb(int exp, int bright);
/* Dialogue fg. brightness < 256 forces color 15 (text_draw). */
uint32_t saybox_text_argb(int color, int bright);

#ifdef _arch_dreamcast
int saybox_upload(void);
void saybox_evict(void);
void saybox_draw_pvr(float z);
void saybox_draw_choices_pvr(float z);
void saybox_draw_num_pvr(int x, int y, int num, float z, int exp);
#endif

#endif
