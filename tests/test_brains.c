/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "start_map.h"
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
    struct MapScreen scr;
    struct SeqInfo seqs[DINK_MAX_SEQ];
    struct HardMask mask;
    int i, f1, f2;

    memset(&scr, 0, sizeof(scr));
    memset(&seqs, 0, sizeof(seqs));
    memset(&mask, 0, sizeof(mask));
    seqs[86].delay = 75;
    seqs[86].nframes = 4;
    strncpy(seqs[86].prefix, "graphics/inside/details/fire-",
            sizeof(seqs[86].prefix) - 1);
    seqs[351].delay = 100;
    seqs[351].nframes = 2;
    strncpy(seqs[351].prefix, "graphics/people/mom/c08w1-",
            sizeof(seqs[351].prefix) - 1);
    seqs[353].delay = 100;
    strncpy(seqs[353].prefix, "graphics/people/mom/c08w3-",
            sizeof(seqs[353].prefix) - 1);
    seqs[357].delay = 100;
    strncpy(seqs[357].prefix, "graphics/people/mom/c08w7-",
            sizeof(seqs[357].prefix) - 1);
    seqs[359].delay = 100;
    strncpy(seqs[359].prefix, "graphics/people/mom/c08w9-",
            sizeof(seqs[359].prefix) - 1);

    scr.sprite[20].active = 1;
    scr.sprite[20].type = 1;
    scr.sprite[20].brain = 6;
    scr.sprite[20].seq = 86;
    scr.sprite[20].frame = 1;
    scr.sprite[20].x = 309;
    scr.sprite[20].y = 114;
    scr.sprite[20].timing = 33;
    scr.sprite[26].active = 1;
    scr.sprite[26].type = 1;
    scr.sprite[26].brain = 16;
    scr.sprite[26].seq = 351;
    scr.sprite[26].frame = 1;
    scr.sprite[26].x = 202;
    scr.sprite[26].y = 157;
    scr.sprite[26].speed = 1;
    scr.sprite[26].base_walk = 350;
    scr.sprite[26].timing = 66;
    scr.sprite[1].active = 1;
    scr.sprite[1].type = 0;
    scr.sprite[1].brain = 0;
    scr.sprite[1].seq = 31;
    scr.sprite[1].x = 199;
    scr.sprite[1].y = 88;
    scr.sprite[9].active = 1;
    scr.sprite[9].type = 1;
    scr.sprite[9].brain = 9;
    scr.sprite[9].seq = 131;
    scr.sprite[9].x = 100;
    scr.sprite[9].y = 100;
    scr.sprite[10].active = 1;
    scr.sprite[10].type = 1;
    scr.sprite[10].brain = 5;
    scr.sprite[10].seq = 1;
    scr.sprite[10].x = 10;
    scr.sprite[10].y = 10;
    scr.sprite[11].active = 1;
    scr.sprite[11].type = 1;
    scr.sprite[11].brain = 7;
    scr.sprite[11].seq = 1;
    scr.sprite[11].x = 20;
    scr.sprite[11].y = 20;
    scr.sprite[12].active = 1;
    scr.sprite[12].type = 1;
    scr.sprite[12].brain = 12;
    scr.sprite[12].seq = 1;
    scr.sprite[12].x = 30;
    scr.sprite[12].y = 30;

    brains_enter(&scr, DINK_VISION_DEFAULT);
    brains_apply(&scr);
    expect((int)scr.sprite[20].seq == 86, "fire pseq overlay");
    expect((int)scr.sprite[1].seq == 31, "type0 not live overlay");

    brains_set_freeze(26, 1);
    expect(brains_freeze(26) == 1, "mom freeze");
    f1 = (int)scr.sprite[20].frame;
    for (i = 0; i < 20; i++) {
        brains_tick(&scr, seqs, &mask, i * 16, DINK_VISION_DEFAULT);
    }
    f2 = (int)scr.sprite[20].frame;
    expect(f2 != f1, "fire frame advanced");
    expect(f2 >= 1 && f2 <= 4, "fire frame in seq");
    expect((int)scr.sprite[26].x == 202 && (int)scr.sprite[26].y == 157,
           "frozen mom does not walk");
    expect(brains_unimpl_count() == 4, "5/7/9/12 unimplemented once");
    /* Do not reuse the old wrong map (9=bounce, 12=text). */
    expect((int)scr.sprite[9].x == 100, "pillbug not bounce-moved");
    expect((int)scr.sprite[12].x == 30, "brain 12 not text-moved");

    brains_set_freeze(26, 0);
    {
        struct World w;
        struct MapScreen house;

        if (dink_fs_init() == 0 && world_load(&w) == 0) {
            int rec = (int)w.loc[DINK_START_PLAYER_MAP];

            expect(rec >= 1, "start loc");
            expect(map_load_record(rec, &house) == 0, "house map");
            expect(house.sprite[20].brain == 6 && house.sprite[20].seq == 86,
                   "house fireplace brain 6 seq 86");
            expect(house.sprite[26].brain == 16 && house.sprite[26].speed == 1 &&
                       house.sprite[26].base_walk == 350,
                   "mom brain 16 speed/base_walk");
            expect(house.sprite[20].timing == 33, "fire timing 33");
        }
    }
    printf("OK test_brains\n");
    return 0;
}
