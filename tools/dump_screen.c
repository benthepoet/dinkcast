/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"
#include "mapscr.h"
#include "start_map.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    struct World w;
    struct MapScreen s;
    int map_i = DINK_START_PLAYER_MAP;
    int rec, y, x, i, actives;

    if (argc > 1) {
        map_i = atoi(argv[1]);
    }
    if (dink_fs_init() != 0 || world_load(&w) != 0) {
        fprintf(stderr, "world load failed\n");
        return 1;
    }
    if (map_i < 1 || map_i >= DINK_WORLD_SLOTS) {
        fprintf(stderr, "bad map %d\n", map_i);
        return 1;
    }
    rec = (int)w.loc[map_i];
    if (rec < 1) {
        fprintf(stderr, "empty loc[%d]\n", map_i);
        return 1;
    }
    if (map_load_record(rec, &s) != 0) {
        fprintf(stderr, "map_load_record %d failed\n", rec);
        return 1;
    }
    printf("player_map %d loc %d\n", map_i, rec);
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 12; x++) {
            printf("%d%s", (int)s.t[y * 12 + x].square_full_idx0, x == 11 ? "" : " ");
        }
        printf("\n");
    }
    actives = 0;
    for (i = 1; i <= 99; i++) {
        if (s.sprite[i].active) {
            printf("sprite %d seq=%d frame=%d xy=%d,%d script=%s\n", i,
                   (int)s.sprite[i].seq, (int)s.sprite[i].frame,
                   (int)s.sprite[i].x, (int)s.sprite[i].y, s.sprite[i].script);
            actives++;
        }
    }
    printf("actives %d script=%s\n", actives, s.script);
    return 0;
}
