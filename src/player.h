/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PLAYER_H
#define DINKCAST_PLAYER_H

#include "hard.h"
#include "ini.h"

#define DINK_BASE_WALK 70
#define DINK_BASE_IDLE 10
#define DINK_SPEED 3

struct Player {
    int x, y, dir, seq, frame, acc;
    int freeze; /* FreeDink spr[1].freeze nest */
};

void player_init(struct Player *p);
void player_seq_for_input(const struct Player *p, int pad_dir, int *seq,
                          int *nframes_hint);
void player_step(struct Player *p, int pad_dir, const struct HardMask *mask,
                 const struct SeqInfo *seqs);

#endif
