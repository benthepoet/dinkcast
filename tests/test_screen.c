/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "player.h"
#include "screen.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    struct World w;
    struct Player p;
    int map;

    memset(&w, 0, sizeof(w));
    memset(&p, 0, sizeof(p));
    map = 1;
    p.x = 10;
    p.y = 160;
    expect(screen_try_cross(&w, &map, &p) == 0, "no west");
    expect(p.x == DINK_PLAYL && map == 1, "clamp west");
    w.loc[2] = 5;
    map = 1;
    p.x = 700;
    expect(screen_try_cross(&w, &map, &p) == 1, "east");
    expect(map == 2 && p.x == DINK_PLAYL, "east wrap");
    {
        struct MapScreen s;

        memset(&s, 0, sizeof(s));
        s.sprite[7].is_warp = 1;
        s.sprite[7].warp_map = 3;
        s.sprite[7].warp_x = 100;
        s.sprite[7].warp_y = 200;
        map = 1;
        expect(screen_try_warp(&s, 7, &map, &p) == 0, "warp");
        expect(map == 3 && p.x == 100 && p.y == 200, "warp dest");
        expect(screen_try_warp(&s, 8, &map, &p) != 0, "no warp");
    }
    printf("OK test_screen\n");
    return 0;
}
