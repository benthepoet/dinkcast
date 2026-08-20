/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_TALK_H
#define DINKCAST_TALK_H

#include "edraw.h"
#include "ini.h"
#include "mapscr.h"

/* FreeDink run_through_tag_list_talk: first live sprite (slot 1..99)
 * whose hardbox+10, extended 50/35 in Dink's dir, contains (dx,dy).
 * Returns slot or 0. */
int talk_probe(const struct MapScreen *scr, struct EdGfx *edg, int ned,
               struct SeqInfo *seqs, int dx, int dy, int dir, int vision);
/* FreeDink human_brain: r is 1..6 (rand()%6)+1. */
const char *talk_miss_line(int r);
const char *magic_miss_line(int r);

#endif
