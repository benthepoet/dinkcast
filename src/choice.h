/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_CHOICE_H
#define DINKCAST_CHOICE_H

#include "ini.h"
#include "sprite.h"

/* FreeDink game_choice.cpp / game_choice_renderer.cpp. */
#define DINK_CHOICE_BOX_L 184
#define DINK_CHOICE_BOX_R 463
#define DINK_CHOICE_X_DEPTH 335
#define DINK_CHOICE_Y0 94
#define DINK_CHOICE_NEWY_NONE (-5000)
#define DINK_CHOICE_PX 48
#define DINK_CHOICE_PY 44
#define DINK_CHOICE_CURXL 126
#define DINK_CHOICE_CURXR 462
#define DINK_CHOICE_SEQ 30
#define DINK_CHOICE_AROWL 456
#define DINK_CHOICE_AROWR 457
#define DINK_CHOICE_ARROW_MAX 7

struct ChoiceLayout {
    int choices_y;
    int view_lo; /* 1-based inclusive, game_choice.cur_view */
    int view_hi; /* 1-based inclusive, game_choice.cur_view_end */
};

void choice_wrap(char *s, int max_px);
int choice_wrap_height(const char *s);
int choice_center_x(const char *line);
/* h[1..last] pixel heights. last/cur are 1-based like FreeDink. */
void choice_layout(int last, int cur, const int *h, int newy,
                   struct ChoiceLayout *out);

int choice_load(struct SeqInfo *seqs);
void choice_free(void);
struct SpriteFrame *choice_frame(int seq, int frame);
void choice_tick(int now_ms);
int choice_curf(void);

#ifdef _arch_dreamcast
int choice_upload_pvr(void);
void choice_evict_pvr(void);
#endif

#endif
