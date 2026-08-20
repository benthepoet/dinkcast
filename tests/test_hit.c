/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hit.h"
#include "ini.h"
#include "mapscr.h"
#include "player.h"

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

static void fill_npc(struct EditorSprite *es, int x, int y, const char *script)
{
    memset(es, 0, sizeof(*es));
    es->x = x;
    es->y = y;
    es->type = 1;
    es->active = 1;
    if (script != NULL) {
        strncpy(es->script, script, sizeof(es->script) - 1);
    }
}

int main(void)
{
    struct MapScreen scr;
    struct SeqInfo seqs[DINK_MAX_SEQ];
    struct HardMask mask;
    struct Player p;
    const char *ini =
        "set_frame_special 108 3 1\n";
    int i;

    memset(&scr, 0, sizeof(scr));
    memset(&seqs, 0, sizeof(seqs));
    memset(&mask, 0, sizeof(mask));

    fill_npc(&scr.sprite[1], 200, 200, "s1-h1-m");
    fill_npc(&scr.sprite[2], 200, 200, "later");
    expect(hit_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 1, "first slot");

    scr.sprite[1].type = 0;
    expect(hit_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 2, "skip type 0");
    scr.sprite[1].type = 1;

    /* default ±10 + inflate 5/5/5/10 → [x-15,y-15]–[x+15,y+20];
     * dir 4 adds +28 on r. */
    expect(hit_probe(&scr, NULL, 0, seqs, 200 + 30, 200, 4, 0) == 1,
           "dir 4 reach +30");
    expect(hit_probe(&scr, NULL, 0, seqs, 200 + 30, 200, 6, 0) == 0,
           "dir 6 no +30");
    expect(hit_probe(&scr, NULL, 0, seqs, 200 - 30, 200, 6, 0) == 1,
           "dir 6 reach -30");
    expect(hit_probe(&scr, NULL, 0, seqs, 200 + 80, 200, 4, 0) == 0, "too far");
    expect(hit_probe(NULL, NULL, 0, seqs, 0, 0, 4, 0) == 0, "null screen");

    expect(ini_parse_mem(ini, strlen(ini), seqs, DINK_MAX_SEQ) == 0, "ini");
    expect(ini_frame_special(108, 3) == 1, "special 108.3");
    expect(ini_frame_special(108, 2) == 0, "not special 108.2");

    seqs[108].delay = 16;
    seqs[108].nframes = 4;
    seqs[DINK_BASE_IDLE + 8].delay = 50;
    seqs[DINK_BASE_IDLE + 8].nframes = 1;
    player_init(&p);
    p.x = 100;
    p.y = 100;
    p.dir = 9;
    player_attack(&p, seqs);
    expect(p.dir == 8 && p.seq == DINK_BASE_ATTACK + 8 && p.nocontrol,
           "attack snap 9→8");
    expect(p.x == 100, "attack no move");
    {
        int ox = p.x;

        player_step(&p, 6, &mask, seqs, 0);
        expect(p.x == ox && p.nocontrol, "nocontrol no walk");
    }
    expect(!p.just_hit, "frame 1 not special");
    for (i = 0; i < 8 && !p.just_hit; i++) {
        player_step(&p, 0, &mask, seqs, 0);
    }
    expect(p.just_hit && p.frame == 3, "hit on special frame 3");
    while (p.nocontrol) {
        player_step(&p, 0, &mask, seqs, 0);
    }
    expect(p.seq == DINK_BASE_IDLE + 8, "idle after punch");

    player_init(&p);
    p.freeze = 1;
    player_attack(&p, seqs);
    expect(!p.nocontrol, "freeze skips attack");

    printf("OK test_hit\n");
    return 0;
}
