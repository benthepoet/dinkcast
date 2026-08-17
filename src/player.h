/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PLAYER_H
#define DINKCAST_PLAYER_H

#include "hard.h"
#include "ini.h"

#define DINK_BASE_WALK 70
#define DINK_BASE_IDLE 10
#define DINK_BASE_ATTACK 100 /* fists; START-1.c / item-fst */
#define DINK_SPEED 3

struct Player {
    int x, y, dir, seq, frame, acc;
    int freeze; /* FreeDink spr[1].freeze nest */
    int nocontrol; /* attack lock (item-fst / spr.nocontrol) */
    int just_hit; /* 1 after landing on SET_FRAME_SPECIAL */
    int base_attack;
};

void player_init(struct Player *p);
/* Snap diag like item-fst; start base_attack+dir. No-op if freeze/nocontrol. */
void player_attack(struct Player *p, const struct SeqInfo *seqs);
void player_seq_for_input(const struct Player *p, int pad_dir, int *seq,
                          int *nframes_hint);
void player_step(struct Player *p, int pad_dir, const struct HardMask *mask,
                 const struct SeqInfo *seqs);

#endif
