/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_BRAINS_H
#define DINKCAST_BRAINS_H

#include "hard.h"
#include "ini.h"
#include "mapscr.h"

struct Player;

/* Bite 15.1: FreeDink update_frame switch (all stock ids). Player is spr[1] / player_step. */

void brains_bind_screen(const struct MapScreen *scr);
void brains_reset(void);
void brains_enter(const struct MapScreen *scr, int vision);
void brains_tick(struct MapScreen *scr, const struct SeqInfo *seqs,
                 const struct HardMask *mask, int now_ms, int vision);
/* 1 if slot is a live BrainSpr; writes x/y. Slot 1 is Player, not this. */
int brains_live_xy(int slot, int *x, int *y);
int brains_slot_live(int slot);
int brains_slot_seq(int slot);
int brains_slot_created(int slot);
int brains_slot_pseq(int slot);
int brains_slot_base_walk(int slot);
void brains_set_script(int slot, const char *name);
/* Copy live x/y/pseq/pframe onto editor slots after a snapshot restore. */
void brains_apply(struct MapScreen *scr);
void brains_set_freeze(int slot, int on);
int brains_freeze(int slot);
/* FreeDink change_sprite: val -1 reads. prop = DINKC_SP_* in dinkc_cmd.h. */
int brains_change_prop(int slot, int prop, int val);
/* add_sprite_dumb: first free slot 2..99, skip active editor. 0 if full. */
int brains_create(int x, int y, int brain, int pseq, int pframe);
/* process_move setup (FreeDink dc_move). */
int brains_move(int slot, int dir, int dest, int nohard);
int brains_moving(int slot);
int brains_unimpl_count(void);
/* 15.2 combat. DIE / add_exp callbacks (optional). */
void brains_bind_kill(void (*fn)(int slot, const char *proc));
void brains_bind_exp(void (*fn)(int num));
void brains_bind_player(struct Player *p);
int brains_hurt(int slot, int damage);
int brains_hitpoints(int slot);
int brains_defense(int slot);
int brains_strength(int slot);
int brains_range(int slot);
int brains_nohit(int slot);
int brains_exp(int slot);
int brains_base_attack(int slot);
int brains_touch_damage(int slot);
void brains_set_last_hit(int slot, int who);
void brains_set_target(int slot, int who);
int brains_take_just_hit(int slot);
int brains_seq_frame(int slot, int *seq, int *frame);

#endif
