/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "screen.h"

#include <stdio.h>

int screen_try_cross(const struct World *w, int *player_map, struct Player *p)
{
    int m;

    if (w == NULL || player_map == NULL || p == NULL) {
        return 0;
    }
    m = *player_map;
    if (p->x < DINK_PLAYL) {
        if (m - 1 >= 1 && w->loc[m - 1] > 0) {
            *player_map = m - 1;
            p->x = DINK_PLAYX;
            printf("screen west map %d\n", *player_map);
            return 1;
        }
        p->x = DINK_PLAYL;
        return 0;
    }
    if (p->x > DINK_PLAYX) {
        if (m + 1 <= 24 * 32 && w->loc[m + 1] > 0) {
            *player_map = m + 1;
            p->x = DINK_PLAYL;
            printf("screen east map %d\n", *player_map);
            return 1;
        }
        p->x = DINK_PLAYX;
        return 0;
    }
    if (p->y < 0) {
        if (m - 32 >= 1 && w->loc[m - 32] > 0) {
            *player_map = m - 32;
            p->y = DINK_PLAYY;
            printf("screen north map %d\n", *player_map);
            return 1;
        }
        p->y = 0;
        return 0;
    }
    if (p->y > DINK_PLAYY) {
        if (m + 32 <= 24 * 32 && w->loc[m + 32] > 0) {
            *player_map = m + 32;
            p->y = 0;
            printf("screen south map %d\n", *player_map);
            return 1;
        }
        p->y = DINK_PLAYY;
        return 0;
    }
    return 0;
}

int screen_try_warp(const struct World *w, const struct MapScreen *scr,
                    int editor, int *player_map, struct Player *p)
{
    const struct EditorSprite *es;
    int dest;

    if (w == NULL || scr == NULL || player_map == NULL || p == NULL ||
        editor < 1 || editor > 100) {
        return -1;
    }
    es = &scr->sprite[editor];
    if (!es->is_warp) {
        return -1;
    }
    dest = (int)es->warp_map;
    if (dest < 1 || dest > 24 * 32 || w->loc[dest] < 1) {
        return -1;
    }
    *player_map = dest;
    p->x = (int)es->warp_x;
    p->y = (int)es->warp_y;
    printf("warp ed=%d map=%d xy=%d,%d\n", editor, *player_map, p->x, p->y);
    return 0;
}
