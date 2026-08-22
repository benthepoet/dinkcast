/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"
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
    screen_lock_set(1);
    map = 1;
    p.x = 700;
    expect(screen_try_cross(&w, &map, &p) == 0, "lock no east");
    expect(map == 1 && p.x == DINK_PLAYX, "lock clamp east");
    screen_lock_set(0);
    map = 1;
    p.x = 700;
    expect(screen_try_cross(&w, &map, &p) == 1, "unlock east");
    expect(map == 2 && p.x == DINK_PLAYL, "unlock wrap");
    {
        struct MapScreen s;

        memset(&s, 0, sizeof(s));
        s.sprite[7].is_warp = 1;
        s.sprite[7].warp_map = 3;
        s.sprite[7].warp_x = 100;
        s.sprite[7].warp_y = 200;
        w.loc[3] = 9;
        map = 1;
        expect(screen_try_warp(&w, &s, 7, &map, &p) == 0, "warp");
        expect(map == 3 && p.x == 100 && p.y == 200, "warp dest");
        expect(screen_try_warp(&w, &s, 8, &map, &p) != 0, "no warp");
        s.sprite[100].is_warp = 1;
        s.sprite[100].warp_map = 3;
        s.sprite[100].warp_x = 50;
        s.sprite[100].warp_y = 60;
        expect(screen_try_warp(&w, &s, 100, &map, &p) == 0, "ed 100");
        s.sprite[7].warp_map = 5;
        expect(screen_try_warp(&w, &s, 7, &map, &p) != 0, "empty dest");
        s.sprite[7].warp_map = 3;
        s.sprite[7].parm_seq = 61;
        map = 1;
        p.x = 10;
        p.y = 160;
        expect(screen_special_block(&w, &s, 7, &map, &p) == 1, "wait parm_seq");
        expect(map == 1 && p.x == 10, "no teleport yet");
        expect(screen_process_warp() == 7, "process_warp");
        expect(screen_try_warp(&w, &s, 7, &map, &p) == 0, "commit after anim");
        expect(map == 3 && p.x == 100 && p.y == 200, "warped");
        expect(screen_process_warp() == 0, "cleared");
        s.sprite[7].parm_seq = 0;
        map = 1;
        expect(screen_special_block(&w, &s, 7, &map, &p) == 0, "no parm instant");
        expect(map == 3, "instant dest");
    }
    w.loc[131] = 7;
    expect(screen_map_rec(&w, 131) == 7, "loc 131");
    expect(screen_map_rec(&w, 0) == 0, "map 0");
    expect(screen_map_rec(NULL, 1) == 0, "null world");
    screen_lock_set(1);
    expect(screen_lock_get() == 1, "lock on");
    expect(screen_game_load(&w, 2, NULL) == -1, "no dest scr");
    {
        struct MapScreen empty;

        memset(&empty, 0, sizeof(empty));
        screen_lock_set(1);
        expect(screen_game_load(&w, 400, &empty) == -1, "empty loc");
        expect(screen_lock_get() == 1, "miss does not clear lock");
    }
    if (getenv("DINK_DATA") != NULL && getenv("DINK_DATA")[0] != '\0' &&
        dink_fs_init() == 0 && world_load(&w) == 0) {
        struct MapScreen hole;
        int rec, dest;

        rec = screen_map_rec(&w, 131);
        expect(rec >= 1, "official hole loc");
        screen_lock_set(1);
        dest = screen_game_load(&w, 131, &hole);
        expect(dest == rec, "hole game_load");
        expect(screen_lock_get() == 0, "game_load_screen clears lock");
        expect(hole.t[0].square_full_idx0 != 0 || hole.script[0] != '\0',
               "hole record");
        expect(screen_map_rec(&w, 439) >= 1, "letter dest loc");
    }
    printf("OK test_screen\n");
    return 0;
}
