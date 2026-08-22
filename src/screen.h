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
/* special_block: -1 none, 0 warped now, 1 wait parm_seq (process_warp). */
int screen_special_block(const struct World *w, const struct MapScreen *scr,
                         int editor, int *player_map, struct Player *p);
/* Commit warp after parm_seq finishes. 0 = warped. */
int screen_try_warp(const struct World *w, const struct MapScreen *scr,
                    int editor, int *player_map, struct Player *p);
int screen_process_warp(void);
void screen_warp_clear(void);

/* dink.dat loc[] for &player_map. 0 if empty or out of range. */
int screen_map_rec(const struct World *w, int player_map);
/* FreeDink game_load_screen: map.dat record + screenlock=0. Not draw. */
int screen_game_load(const struct World *w, int player_map, struct MapScreen *scr);
int screen_lock_get(void);
void screen_lock_set(int on);

#endif
