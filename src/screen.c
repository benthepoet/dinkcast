/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "screen.h"

#include <stdio.h>

static int g_process_warp;
static int g_screenlock;

int screen_lock_get(void)
{
    return g_screenlock;
}

void screen_lock_set(int on)
{
    g_screenlock = on ? 1 : 0;
}

int screen_map_rec(const struct World *w, int player_map)
{
    if (w == NULL || player_map < 1 || player_map >= DINK_WORLD_SLOTS) {
        return 0;
    }
    return (int)w->loc[player_map];
}

/* FreeDink game_load_screen(loc[&player_map]): record only. No kill_all. */
int screen_game_load(const struct World *w, int player_map, struct MapScreen *scr)
{
    int rec;

    rec = screen_map_rec(w, player_map);
    if (rec < 1 || scr == NULL) {
        return -1;
    }
    if (map_load_record(rec, scr) != 0) {
        return -1;
    }
    /* game_load_screen: screenlock = 0. Command itself is 0.2 item 4. */
    screen_lock_set(0);
    return rec;
}

int screen_process_warp(void)
{
    return g_process_warp;
}

void screen_warp_clear(void)
{
    g_process_warp = 0;
}

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
    g_process_warp = 0;
    printf("warp ed=%d map=%d xy=%d,%d\n", editor, *player_map, p->x, p->y);
    return 0;
}

/* FreeDink special_block: parm_seq plays on the live sprite first. */
int screen_special_block(const struct World *w, const struct MapScreen *scr,
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
    if (es->parm_seq != 0) {
        g_process_warp = editor;
        return 1;
    }
    return screen_try_warp(w, scr, editor, player_map, p);
}
