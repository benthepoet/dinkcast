/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_SCREEN_H
#define DINKCAST_SCREEN_H

#include "mapscr.h"
#include "player.h"
#include "world.h"

#define DINK_PLAYL 20
#define DINK_PLAYX 619
#define DINK_PLAYY 399
#define DINK_MAP_COLS 32

/* did_player_cross_screen: 1 = map changed. */
int screen_try_cross(const struct World *w, int *player_map, struct Player *p);
/* special_block: 0 = warped (player_map/x/y set). dest loc must be nonzero. */
int screen_try_warp(const struct World *w, const struct MapScreen *scr,
                    int editor, int *player_map, struct Player *p);

#endif
