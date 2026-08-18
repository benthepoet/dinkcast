/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_vm.h"
#include "fs.h"
#include "mapscr.h"
#include "script.h"
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

    memset(&scr, 0, sizeof(scr));
    strncpy(scr.script, "s1-h1", sizeof(scr.script) - 1);
    strncpy(scr.sprite[3].script, "s1-h1-m", sizeof(scr.sprite[3].script) - 1);
    scr.sprite[3].active = 1;
    scr.sprite[3].type = 1;
    strncpy(scr.sprite[4].script, "s1-h1-m", sizeof(scr.sprite[4].script) - 1);
    scr.sprite[4].active = 1;
    scr.sprite[4].type = 1;

    script_bind_screen(&scr);
    script_on_main(0);
    expect(strstr(script_stub_log(), "main script_id=0 script=s1-h1") != NULL,
           script_stub_log());
    script_on_talk(3);
    expect(strcmp(script_stub_log(), "talk sprite=3 script=s1-h1-m") == 0,
           script_stub_log());
    script_on_hit(3);
    expect(strcmp(script_stub_log(), "hit sprite=3 script=s1-h1-m") == 0,
           script_stub_log());
    script_on_talk(0);
    expect(strcmp(script_stub_log(), "talk sprite=0 script=") == 0,
           script_stub_log());
    script_on_hit(100);
    expect(strcmp(script_stub_log(), "hit sprite=100 script=") == 0,
           script_stub_log());
    script_bind_screen(NULL);
    script_on_talk(3);
    expect(strcmp(script_stub_log(), "talk sprite=3 script=") == 0,
           "unbound");

    if (dink_fs_init() == 0) {
        struct MapScreen house;
        int n;

        memset(&house, 0, sizeof(house));
        /* type 0 must not attach even with a script (game_place_sprites). */
        strncpy(house.sprite[2].script, "s1-h1-2",
                sizeof(house.sprite[2].script) - 1);
        house.sprite[2].active = 1;
        house.sprite[2].type = 0;
        house.sprite[2].y = 10;
        strncpy(house.sprite[4].script, "sfood",
                sizeof(house.sprite[4].script) - 1);
        house.sprite[4].active = 1;
        house.sprite[4].type = 1;
        house.sprite[4].y = 80;
        strncpy(house.sprite[5].script, "s1-h1-m",
                sizeof(house.sprite[5].script) - 1);
        house.sprite[5].active = 1;
        house.sprite[5].type = 1;
        house.sprite[5].y = 40;
        house.sprite[6].active = 1;
        house.sprite[6].type = 1;
        strncpy(house.sprite[6].script, "x", sizeof(house.sprite[6].script) - 1);
        dinkc_vm_reset();
        script_bind_screen(&house);
        n = script_attach_screen();
        expect(n == 2, "type1 + strlen>1 only");
        expect(dinkc_vm_live() >= 1, "mom main yields or lives");
        dinkc_vm_reset();
        {
            struct World w;
            int rec, na;

            expect(world_load(&w) == 0, "world");
            rec = (int)w.loc[DINK_START_PLAYER_MAP];
            expect(map_load_record(rec, &house) == 0, "house map");
            expect((int)sizeof(struct EditorSprite) == 80, "esz 80");
            expect(strcmp(house.sprite[26].script, "s1-h1-m") == 0, "mom name");
            expect(house.sprite[26].type == 1 && house.sprite[26].active,
                   "mom live");
            script_bind_screen(&house);
            na = script_attach_screen();
            expect(na >= 1, "house attach");
            dinkc_vm_reset();
        }
    }

    printf("OK test_script\n");
    return 0;
}
