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
               struct SeqInfo *seqs, int dx, int dy, int dir);

#endif
