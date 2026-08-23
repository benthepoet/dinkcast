/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PLAYER_H
#define DINKCAST_PLAYER_H

#include "hard.h"
#include "ini.h"

struct MapScreen;

#define DINK_BASE_WALK 70
#define DINK_BASE_IDLE 10
#define DINK_BASE_ATTACK 100 /* fists; START-1.c / item-fst */
#define DINK_BASE_PUSH 310 /* human_brain dink_base_push default */
#define DINK_SPEED 3

struct Player {
    int x, y, dir, seq, frame, acc;
    int freeze; /* FreeDink spr[1].freeze nest */
    int nocontrol; /* attack lock (item-fst / spr.nocontrol) */
    int just_hit; /* 1 after landing on SET_FRAME_SPECIAL */
    int just_push; /* 1 the frame push seq starts (tag_list_push) */
    int base_attack;
    int base_idle;
    int base_push;
    int base_hit; /* FreeDink spr[1].base_hit; START-1 / item-fst 100 */
    int distance;
    int warp_hit; /* editor slot if last step hit hardness 100+is_warp */
    int move_active, move_dir, move_num, move_nohard;
    int hitpoints, defense, strength, nohit, range, damage, last_hit;
    int push_active, push_dir, push_timer;
    int notouch; /* run_through_touch_damage_list: skip +damage while set */
    int notouch_timer;
};

void player_init(struct Player *p);
/* Snap diag like item-fst; start base_attack+dir. No-op if freeze/nocontrol. */
void player_attack(struct Player *p, const struct SeqInfo *seqs);
void player_seq_for_input(const struct Player *p, int pad_dir, int *seq,
                          int *nframes_hint);
void player_step(struct Player *p, int pad_dir, const struct HardMask *mask,
                 const struct SeqInfo *seqs, int now_ms,
                 const struct MapScreen *scr);
/* FreeDink hurt_thing on spr[1]. Adds to .damage. */
int player_hurt(struct Player *p, int damage);
/* human_brain: *plife -= damage, clamp 0. Writes *life. Returns 1 if life < 1. */
int player_apply_life(struct Player *p, int *life);

#endif
