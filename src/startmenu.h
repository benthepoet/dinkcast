/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_STARTMENU_H
#define DINKCAST_STARTMENU_H

#include <stdint.h>

/* START.c start-1 New + start-2 Load. No start-4 Quit on DC. Pad, not mouse. */
#define STARTMENU_NEW 0
#define STARTMENU_LOAD 1
#define STARTMENU_QUIT 2
#define STARTMENU_N 2

#define STARTMENU_SEQ_LOGO 196
#define STARTMENU_SEQ_NEW 194
#define STARTMENU_SEQ_LOAD 195
#define STARTMENU_SEQ_QUIT 193
/* start-1/2/4 buttonon create_sprite + seq. */
#define STARTMENU_HOVER_NEW 199
#define STARTMENU_HOVER_LOAD 200
#define STARTMENU_HOVER_QUIT 198
#define STARTMENU_HOVER_NEW_X 204
#define STARTMENU_HOVER_NEW_Y 86
#define STARTMENU_HOVER_LOAD_X 358
#define STARTMENU_HOVER_LOAD_Y 93
#define STARTMENU_HOVER_QUIT_X 446
#define STARTMENU_HOVER_QUIT_Y 417

void startmenu_reset(void);
int startmenu_focus(void);
/* start-1.c buttonon / buttonoff. live_sprite_animate + brain 7. */
void startmenu_hover_bind(int i, int seq, int x, int y, int nframes, int delay);
void startmenu_hover_on(int i);
void startmenu_hover_off(int i);
void startmenu_hover_tick(int now_ms);
int startmenu_hover_live(int i);
int startmenu_hover_pframe(int i);
int startmenu_hover_x(int i);
int startmenu_hover_y(int i);
/* but7-/but9- pframe 2 is a glow around the idle word (larger BMP).
 * Keep the letters still: extra pixels grow equally on all sides. */
void startmenu_highlight_center(int w0, int h0, int cx0, int cy0, int w1,
                                int h1, int *cx1, int *cy1);
/* -1 still choosing; else STARTMENU_*. */
int startmenu_tick(uint32_t prev, uint32_t now);
const char *startmenu_script(int focus);
/* start-2.c load(): lines 1–10 slots, 11 Nevermind. */
#define STARTMENU_SLOT_N 11
#define STARTMENU_SLOT_NEVERMIND 11
void startmenu_slot_reset(void);
int startmenu_slot_focus(void);
/* -1 still; 1–10 slot; 11 Nevermind. */
int startmenu_slot_tick(uint32_t prev, uint32_t now);

/* In-play Start pause: Continue / Title. Save is SAVEBOT.c only. */
#define STARTPAUSE_CONTINUE 0
#define STARTPAUSE_TITLE 1
#define STARTPAUSE_N 2

void startpause_reset(void);
int startpause_focus(void);
int startpause_open(void);
/* True only when Start-pause should own the pad.
 * Normal play (A talk, Y inv, B hit) must not match. */
int startpause_eats_pad(uint32_t prev, uint32_t now);
/* -1 still open; -2 closed (B); else STARTPAUSE_*. */
int startpause_tick(uint32_t prev, uint32_t now);

#ifdef _arch_dreamcast
struct SeqInfo;
/* Official 196 + start-1/2. Pad, not START.c main(). */
int startmenu_present_pvr(struct SeqInfo *seqs);
/* start-2 load() choice on the START picture. 1–10 slot; 11 Nevermind. */
int startmenu_present_slots_pvr(struct SeqInfo *seqs);
#endif

#endif
