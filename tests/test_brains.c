/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"
#include "dinkc_cmd.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "player.h"
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
    scr.sprite[9].speed = 1;
    scr.sprite[9].base_walk = 130;
    brains_enter(&scr, DINK_VISION_DEFAULT);

    seqs[61].delay = 75;
    seqs[61].nframes = 4;
    strncpy(seqs[61].prefix, "graphics/struct/details/door/odor1-",
            sizeof(seqs[61].prefix) - 1);
    {
        struct MapScreen door;
        int j, saw = 0;

        memset(&door, 0, sizeof(door));
        door.sprite[13].active = 1;
        door.sprite[13].type = 1;
        door.sprite[13].brain = 0;
        door.sprite[13].seq = 61;
        door.sprite[13].frame = 1;
        door.sprite[13].is_warp = 1;
        door.sprite[13].parm_seq = 61;
        door.sprite[13].x = 368;
        door.sprite[13].y = 280;
        brains_enter(&door, DINK_VISION_DEFAULT);
        expect(brains_slot_live(13), "door live");
        expect(brains_slot_seq(13) == 0, "door idle seq 0");
        expect(brains_change_prop(13, 8, 61) == 61, "play parm_seq");
        for (j = 0; j < 80; j++) {
            brains_tick(&door, seqs, &mask, j * 16, DINK_VISION_DEFAULT);
            if ((int)door.sprite[13].frame > 1) {
                saw = 1;
            }
            if (j > 2 && brains_slot_seq(13) == 0) {
                break;
            }
        }
        expect(saw, "door frames advanced");
        expect(brains_slot_seq(13) == 0, "parm_seq finished");
    }
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
    scr.sprite[12].size = 100;
    scr.sprite[12].x = 30;
    scr.sprite[12].y = 30;

    {
        struct EditorSprite snap[101];

        memcpy(snap, scr.sprite, sizeof(snap));
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
        expect(brains_unimpl_count() == 0, "stock brains grafted");
        expect((int)scr.sprite[12].x == 30, "brain 12 not text-moved");
        expect((int)scr.sprite[10].seq == 1 || (int)scr.sprite[10].frame >= 1,
               "5 stays after one-time");

        /* Play loop: memcpy editor snapshot then brains_apply. */
        memcpy(scr.sprite, snap, sizeof(snap));
        expect((int)scr.sprite[20].frame == 1, "snapshot wipes fire frame");
        brains_apply(&scr);
        expect((int)scr.sprite[20].frame == f2, "apply restores fire after snapshot");
        expect((int)scr.sprite[20].seq == 86, "apply restores fire seq");
        expect((int)scr.sprite[1].seq == 31, "type0 snapshot not overlaid");
    }

    {
        int c, j, kept;

        brains_bind_screen(&scr);
        brains_enter(&scr, DINK_VISION_DEFAULT);
        c = brains_create(40, 50, 0, 32, 1);
        expect(c >= 2 && c <= 100, "create_sprite slot");
        expect(brains_slot_hard(c) == 1, "add_sprite hard=1");
        expect(brains_change_prop(c, 2, 3) == 3, "created speed");
        expect(brains_change_prop(c, DINKC_SP_FRAME_DELAY, 200) == 200,
               "frame_delay");
        expect(brains_change_prop(c, DINKC_SP_DISABLED, 1) == 1,
               "sp_disabled set");
        expect(brains_slot_disabled(c), "disabled flag");
        expect(brains_slot_live(c), "disabled still live");
        expect(brains_change_prop(c, DINKC_SP_DISABLED, 0) == 0,
               "sp_disabled clear");
        expect(brains_change_prop(c, 5, 80) == 80, "sp_x created");
        brains_apply(&scr);
        expect((int)scr.sprite[c].active == 1 && (int)scr.sprite[c].type == 1 &&
                   (int)scr.sprite[c].x == 80 && (int)scr.sprite[c].hard == 1,
               "created overlay after apply");
        brains_set_script(c, "s1-lg");
        scr.sprite[c].script[0] = '\0';
        brains_apply(&scr);
        expect(strcmp(scr.sprite[c].script, "s1-lg") == 0,
               "apply copies sp_script for talk");
        expect(brains_move(c, 6, 200, 1) == 1, "move");
        for (j = 0; j < 80; j++) {
            brains_tick(&scr, seqs, &mask, j * 16, DINK_VISION_DEFAULT);
        }
        expect((int)scr.sprite[c].x > 80, "move walked east");
        expect(!brains_moving(c) || (int)scr.sprite[c].x >= 200,
               "move toward dest");
        kept = brains_change_prop(c, 5, -1);
        brains_enter(&scr, DINK_VISION_DEFAULT);
        expect(brains_change_prop(c, 5, -1) == kept, "enter keeps create");
        expect(brains_slot_created(c), "still created");
    }
    {
        int food, sq = 0, fr = 0, t;
        struct MapScreen empty;

        memset(&empty, 0, sizeof(empty));
        seqs[421].nframes = 31;
        seqs[421].delay = 1;
        brains_reset();
        brains_bind_screen(&empty);
        brains_enter(&empty, 0);
        food = brains_create(349, 359, 6, 421, 1);
        expect(food >= 2, "MAKE food1 slot");
        for (t = 0; t < 40; t++) {
            brains_tick(&empty, seqs, &mask, t * 16, 0);
        }
        expect(brains_seq_frame(food, &sq, &fr) && sq == 421 && fr == 1,
               "food stays pframe 1");
    }

    {
        struct Player pl;
        struct MapScreen fol;
        int c, j, x0;

        memset(&fol, 0, sizeof(fol));
        memset(&pl, 0, sizeof(pl));
        player_init(&pl);
        pl.x = 400;
        pl.y = 200;
        brains_bind_player(&pl);
        brains_bind_screen(&fol);
        brains_reset();
        c = brains_create(200, 200, 3, 20, 1);
        expect(c >= 2, "follow duck slot");
        expect(brains_change_prop(c, DINKC_SP_SPEED, 1) == 1, "follow speed");
        expect(brains_change_prop(c, DINKC_SP_BASE_WALK, 20) == 20,
               "follow base_walk");
        expect(brains_change_prop(c, DINKC_SP_FOLLOW, 1) == 1, "sp_follow");
        expect(brains_follow(c) == 1, "follow field");
        x0 = brains_change_prop(c, DINKC_SP_X, -1);
        for (j = 0; j < 40; j++) {
            brains_tick(&fol, seqs, &mask, j * 16, DINK_VISION_DEFAULT);
        }
        expect(brains_change_prop(c, DINKC_SP_X, -1) > x0,
               "process_follow walks east");
        expect(brains_change_prop(c, DINKC_SP_Y, -1) == 200,
               "process_follow nosmooth same y");
        /* nosmooth: equal Y leaves distancey=5000. Offset Y so the
         * larger axis is the real 20 px and dist < 40 holds. */
        expect(brains_change_prop(c, DINKC_SP_X, 380) == 380, "close x");
        expect(brains_change_prop(c, DINKC_SP_Y, 190) == 190, "close y");
        for (j = 0; j < 10; j++) {
            brains_tick(&fol, seqs, &mask, 1000 + j * 16, DINK_VISION_DEFAULT);
        }
        expect(brains_change_prop(c, DINKC_SP_X, -1) == 380 &&
                   brains_change_prop(c, DINKC_SP_Y, -1) == 190,
               "process_follow hold under 40");
        brains_bind_player(NULL);
        brains_tick(&fol, seqs, &mask, 2000, DINK_VISION_DEFAULT);
        expect(brains_follow(c) == 0, "follow kill inactive");
        brains_bind_player(&pl);
        pl.x = 400;
        pl.y = 200;
        expect(brains_change_prop(c, DINKC_SP_X, 200) == 200, "tgt reset x");
        expect(brains_change_prop(c, DINKC_SP_Y, 200) == 200, "tgt reset y");
        expect(brains_change_prop(c, DINKC_SP_BRAIN, 9) == 9, "pill for target");
        expect(brains_change_prop(c, DINKC_SP_FOLLOW, 0) == 0, "no follow");
        expect(brains_change_prop(c, DINKC_SP_TARGET, 1) == 1, "sp_target");
        expect(brains_target(c) == 1, "target field");
        expect(brains_change_prop(c, DINKC_SP_DISTANCE, 5) == 5, "sp_distance");
        x0 = 200;
        for (j = 0; j < 40; j++) {
            brains_tick(&fol, seqs, &mask, 3000 + j * 16, DINK_VISION_DEFAULT);
        }
        expect(brains_change_prop(c, DINKC_SP_X, -1) > x0,
               "process_target walks east");
        brains_bind_player(NULL);
    }

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
            {
                int c;

                brains_bind_screen(&house);
                brains_reset();
                c = brains_create(630, 180, 9, 331, 4);
                expect(c >= 2 && c <= 99, "house create slot");
                expect(!house.sprite[c].active, "empty editor slot");
                if (house.sprite[2].active) {
                    expect(c != 2, "not type0 wall 2");
                }
                brains_enter(&house, DINK_VISION_DEFAULT);
                expect(brains_slot_created(c), "house enter keeps girl");
                expect(brains_change_prop(c, 5, -1) == 630, "girl x");
            }
            {
                struct MapScreen pig;
                int prec = (int)w.loc[407];
                int moved = 0, j;

                expect(prec >= 1 && map_load_record(prec, &pig) == 0, "pig map");
                expect(pig.sprite[2].brain == 4 && pig.sprite[2].speed == 1 &&
                           pig.sprite[2].base_walk == 40,
                       "pig brain 4");
                brains_enter(&pig, DINK_VISION_DEFAULT);
                for (j = 0; j < 400; j++) {
                    brains_tick(&pig, seqs, &mask, j * 16, DINK_VISION_DEFAULT);
                }
                if ((int)pig.sprite[2].x != 250 || (int)pig.sprite[2].y != 225) {
                    moved = 1;
                }
                if ((int)pig.sprite[5].x != 289 || (int)pig.sprite[5].y != 302) {
                    moved = 1;
                }
                if ((int)pig.sprite[6].x != 397 || (int)pig.sprite[6].y != 211) {
                    moved = 1;
                }
                expect(moved, "pig_brain walked");
                expect(brains_unimpl_count() == 0, "pig screen no unimplemented");
            }
            {
                struct MapScreen pill;
                int prec = (int)w.loc[378];
                int ox, oy, j;

                expect(prec >= 1 && map_load_record(prec, &pill) == 0,
                       "pill map 378");
                expect(pill.sprite[7].brain == 0 && pill.sprite[7].speed == 0 &&
                           pill.sprite[7].base_walk == 130,
                       "editor pill is brain 0");
                brains_enter(&pill, DINK_VISION_DEFAULT);
                expect(brains_change_prop(7, 1, 9) == 9, "sp_brain 9");
                expect(brains_change_prop(7, 2, 1) == 1, "sp_speed 1");
                expect(brains_change_prop(7, 3, 130) == 130, "sp_base_walk");
                ox = (int)pill.sprite[7].x;
                oy = (int)pill.sprite[7].y;
                for (j = 0; j < 400; j++) {
                    brains_tick(&pill, seqs, &mask, j * 16, DINK_VISION_DEFAULT);
                }
                expect((int)pill.sprite[7].x != ox || (int)pill.sprite[7].y != oy,
                       "en-pill main then pill_brain walks");
            }
        }
    }
    {
        struct MapScreen look;
        int a, b, r, j, saw_a = 0, saw_b = 0;

        memset(&look, 0, sizeof(look));
        brains_bind_screen(&look);
        brains_reset();
        a = brains_create(10, 10, 9, 130, 1);
        b = brains_create(20, 20, 9, 130, 1);
        expect(a >= 2 && b > a, "two pills");
        expect(brains_first_with_brain(9, 0, 1) == a, "first brain 9");
        expect(brains_first_with_brain(9, a, 1) == b, "ignore first");
        expect(brains_first_with_brain(9, a, b + 1) == 0, "next past last");
        expect(brains_first_with_brain(9, 0, b) == b, "next from b");
        expect(brains_first_with_brain(16, 0, 1) == 0, "no people");
        expect(brains_first_with_brain(9, 0, 0) == a, "start 0 is 1");
        for (j = 0; j < 40; j++) {
            r = brains_rand_with_brain(9, 0);
            expect(r == a || r == b, "rand is a match");
            if (r == a) {
                saw_a = 1;
            }
            if (r == b) {
                saw_b = 1;
            }
        }
        expect(saw_a && saw_b, "rand hits both");
        expect(brains_rand_with_brain(9, a) == b, "rand ignore leaves one");
        expect(brains_rand_with_brain(3, 0) == 0, "rand miss");
    }
    printf("OK test_brains\n");
    return 0;
}
