/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_HIT_H
#define DINKCAST_HIT_H

#include "edraw.h"
#include "ini.h"
#include "mapscr.h"
#include "player.h"

/* FreeDink run_through_tag_list: first live sprite (1..99) whose
 * hardbox (inflate 5/5/5/10 + dir range 28/36) contains (dx,dy).
 * Returns slot or 0. Geometry test helper; damage uses hit_tag_list. */
int hit_probe(const struct MapScreen *scr, struct EdGfx *edg, int ned,
              struct SeqInfo *seqs, int dx, int dy, int dir, int vision);

void hit_bind_player(struct Player *p);
void hit_bind_hit(void (*fn)(int slot, int attacker));
void hit_bind_push(void (*fn)(int slot));
void hit_bind_touch(void (*fn)(int slot));
/* FreeDink run_through_tag_list: hurt every sprite in the box. */
void hit_tag_list(int attacker, int ax, int ay, int dir, int strength,
                  int range, struct EdGfx *edg, int ned, struct SeqInfo *seqs);
/* FreeDink run_through_tag_list_push: PUSH proc, inflate ±2. */
void hit_tag_list_push(int ax, int ay, struct EdGfx *edg, int ned,
                       struct SeqInfo *seqs);
/* FreeDink run_through_touch_damage_list (player brain 1). -1 locates
 * TOUCH; >0 hurts Dink and sets notouch 400 ms. */ 
void hit_touch_list(int dx, int dy, int now_ms, struct EdGfx *edg, int ned,
                    struct SeqInfo *seqs);

#endif
