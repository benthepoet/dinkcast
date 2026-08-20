/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_BRAINS_H
#define DINKCAST_BRAINS_H

#include "hard.h"
#include "ini.h"
#include "mapscr.h"

/* Bite 15.1: FreeDink update_frame switch (all stock ids). Player is spr[1] / player_step. */

void brains_enter(const struct MapScreen *scr, int vision);
void brains_tick(struct MapScreen *scr, const struct SeqInfo *seqs,
                 const struct HardMask *mask, int now_ms, int vision);
/* Copy live x/y/pseq/pframe onto editor slots after a snapshot restore. */
void brains_apply(struct MapScreen *scr);
void brains_set_freeze(int slot, int on);
int brains_freeze(int slot);
/* FreeDink change_sprite: val -1 reads. prop = DINKC_SP_* in dinkc_cmd.h. */
int brains_change_prop(int slot, int prop, int val);
int brains_unimpl_count(void);

#endif
