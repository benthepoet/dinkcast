/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_HIT_H
#define DINKCAST_HIT_H

#include "edraw.h"
#include "ini.h"
#include "mapscr.h"

/* FreeDink run_through_tag_list: first live sprite (1..99) whose
 * hardbox (inflate 5/5/5/10 + dir range 28/36) contains (dx,dy).
 * Returns slot or 0. */
int hit_probe(const struct MapScreen *scr, struct EdGfx *edg, int ned,
              struct SeqInfo *seqs, int dx, int dy, int dir, int vision);

#endif
