/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mapscr.h"
#include "pad.h"
#include "player.h"
#include "talk.h"

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
    es->brain = 0;
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

    memset(&scr, 0, sizeof(scr));
    memset(&seqs, 0, sizeof(seqs));

    fill_npc(&scr.sprite[1], 200, 200, "s1-h1-m");
    fill_npc(&scr.sprite[2], 200, 200, "later");
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 1, "first slot");

    scr.sprite[1].type = 0;
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 2, "skip type 0");
    scr.sprite[1].type = 1;

    scr.sprite[1].brain = 8;
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 2, "skip brain 8");
    scr.sprite[1].brain = 0;

    scr.sprite[1].script[0] = '\0';
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 2, "skip empty script");
    strncpy(scr.sprite[1].script, "s1-h1-m", sizeof(scr.sprite[1].script) - 1);

    scr.sprite[1].active = 0;
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 2, "skip inactive");
    scr.sprite[1].active = 1;

    scr.sprite[1].vision = 2;
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200, 4, 0) == 2, "skip vision 2");
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200, 4, 2) == 1, "vision 2 talk");
    scr.sprite[1].vision = 0;

    /* Default hardbox ±10 + inflate 10 → [x-20,y-20]–[x+20,y+20];
     * dir 4 (right) extends r by 50. */
    expect(talk_probe(&scr, NULL, 0, seqs, 200 + 40, 200, 4, 0) == 1,
           "dir 4 reach +40");
    expect(talk_probe(&scr, NULL, 0, seqs, 200 + 40, 200, 6, 0) == 0,
           "dir 6 no +40");
    expect(talk_probe(&scr, NULL, 0, seqs, 200 - 40, 200, 6, 0) == 1,
           "dir 6 reach -40");
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200 + 40, 8, 0) == 1,
           "dir 8 reach +40 y");
    expect(talk_probe(&scr, NULL, 0, seqs, 200, 200 - 40, 2, 0) == 1,
           "dir 2 reach -40 y");
    expect(talk_probe(&scr, NULL, 0, seqs, 200 + 90, 200, 4, 0) == 0, "too far");

    expect(talk_probe(NULL, NULL, 0, seqs, 0, 0, 4, 0) == 0, "null screen");

    expect(pad_just_pressed(0, DINK_PAD_A, DINK_PAD_A) == 1, "A edge");
    expect(pad_just_pressed(DINK_PAD_A, DINK_PAD_A, DINK_PAD_A) == 0, "A hold");
    expect(pad_just_pressed(DINK_PAD_A, 0, DINK_PAD_A) == 0, "A release");

    memset(&mask, 0, sizeof(mask));
    player_init(&p);
    p.x = 100;
    p.y = 100;
    p.freeze = 1;
    player_step(&p, 6, &mask, seqs);
    expect(p.x == 100, "freeze skips move");

    printf("OK test_talk\n");
    return 0;
}
