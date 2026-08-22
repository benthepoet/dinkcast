/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_STATUS_H
#define DINKCAST_STATUS_H

#include "ini.h"

#include <stddef.h>

/* Plan §1.2 HUD atlas: digits + health chunks + magic gauge. */
#define DINK_HUD_ATLAS 256
#define DINK_HUD_ATLAS_BYTES (DINK_HUD_ATLAS * DINK_HUD_ATLAS * 2)
#define DINK_STATUS_SEQ 180
#define DINK_STATUS_EXP_SEQ 181
#define DINK_STATUS_LIFEMAX_SEQ 190
#define DINK_STATUS_LIFE_SEQ 451
#define DINK_STATUS_LEVEL_SEQ 442
#define DINK_MAP_MARK_SEQ 165

int status_next_raise(int level);
int status_load(struct SeqInfo *seqs);
void status_free(void);
void status_draw_all(void);
void status_update(int now_ms);
int status_flife(void);
int status_fgold(void);
int status_fexp(void);
int status_glyph(int seq, int frame, int *sl, int *st, int *sr, int *sb,
                 int *adv);
int status_glyph_opaque_n(int seq, int frame);
int status_chrome_opaque_n(void);
size_t status_cpu_bytes(void);
void status_drop_cpu(void);
int status_atlas_bytes(void);
int status_show_bmp(const char *rel, int showdot, int fiber);
int status_map_active(void);
void status_map_tick(int now_ms);
void status_map_dismiss(void);
int status_map_ready(void);

#ifdef _arch_dreamcast
int status_upload_pvr(void);
void status_draw_pvr(float z);
void status_draw_map_pvr(float z);
#endif

#endif
