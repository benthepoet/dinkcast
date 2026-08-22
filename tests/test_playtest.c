/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Confirmed playtest pictures — see docs/PLAYTEST.md. */
#include "brains.h"
#include "dinkc_cmd.h"
#include "hit.h"
#include "mapscr.h"
#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_hit_slot;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

static void on_hit(int slot, int attacker)
{
    (void)attacker;
    g_hit_slot = slot;
}

int main(void)
{
    struct MapScreen scr;
    struct SeqInfo seqs[DINK_MAX_SEQ];
    struct HardMask mask;
    struct Player pl;
    int i, blood = 0, dmg = 0, hp_before;

    memset(&scr, 0, sizeof(scr));
    memset(&seqs, 0, sizeof(seqs));
    memset(&mask, 0, sizeof(mask));

    /* Confirmed 2026-08-21: blood + hit numbers when punching pigs. */
    scr.sprite[6].active = 1;
    scr.sprite[6].type = 1;
    scr.sprite[6].brain = 4;
    scr.sprite[6].x = 200;
    scr.sprite[6].y = 200;
    scr.sprite[6].hitpoints = 20;
    scr.sprite[6].defense = 0;

    brains_reset();
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    expect(brains_slot_live(6), "pig live");
    hp_before = brains_hitpoints(6);
    expect(hp_before == 20, "pig map hp");

    player_init(&pl);
    pl.x = 200;
    pl.y = 200;
    pl.dir = 4;
    hit_bind_player(&pl);
    g_hit_slot = 0;
    hit_bind_hit(on_hit);
    srand(1);
    hit_tag_list(1, 200, 200, 4, 5, 0, NULL, 0, seqs);
    expect(g_hit_slot == 6, "pig punch HIT");

    for (i = 2; i <= 99; i++) {
        int sq = 0, fr = 0;

        if (!brains_slot_created(i) || brains_slot_brain(i) != 5) {
            continue;
        }
        expect(brains_seq_frame(i, &sq, &fr), "blood seq_frame");
        expect(sq >= 187 && sq <= 189, "pig blood seq 187-189");
        blood = 1;
    }
    expect(blood, "random_blood on pig punch");

    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(brains_hitpoints(6) < hp_before, "pig hp dropped");
    for (i = 2; i <= 99; i++) {
        int fx, fy, fnum;

        if (brains_floater_num(i, &fx, &fy, &fnum)) {
            expect(fnum > 0, "floater number > 0");
            dmg = fnum;
        }
    }
    expect(dmg > 0, "draw_damage hit number");

    /* Confirmed 2026-08-21: pig must not redraw from the map snap after DIE. */
    scr.sprite[7].active = 1;
    scr.sprite[7].type = 1;
    scr.sprite[7].brain = 4;
    scr.sprite[7].x = 300;
    scr.sprite[7].y = 200;
    scr.sprite[7].hitpoints = 1;
    scr.sprite[7].defense = 0;
    brains_enter(&scr, 0);
    expect(brains_slot_live(7), "pig 7 live");
    seqs[164].nframes = 2;
    seqs[164].delay = 1;
    pl.x = 300;
    g_hit_slot = 0;
    hit_tag_list(1, 300, 200, 4, 5, 0, NULL, 0, seqs);
    expect(g_hit_slot == 7, "pig 7 punch HIT");
    for (i = 0; i < 12; i++) {
        brains_tick(&scr, seqs, &mask, 50 * (i + 2), 0);
    }
    expect(!brains_slot_live(7), "pig 7 gone after seq 164");
    expect(!scr.sprite[7].active, "pig 7 editor hidden after kill");

    /* FreeDink one_time_brain: smash last frame is background (under Dink). */
    scr.sprite[8].active = 1;
    scr.sprite[8].type = 1;
    scr.sprite[8].seq = 173;
    scr.sprite[8].frame = 1;
    scr.sprite[8].x = 150;
    scr.sprite[8].y = 250;
    seqs[173].nframes = 2;
    seqs[173].delay = 1;
    brains_enter(&scr, 0);
    expect(brains_slot_live(8), "barrel live");
    expect(brains_change_prop(8, DINKC_SP_BRAIN, 5) == 5, "barrel brain 5");
    expect(brains_change_prop(8, DINKC_SP_SEQ, 173) == 173, "barrel smash seq");
    expect(brains_change_prop(8, DINKC_SP_HARD, 1) == 1, "barrel sp_hard");
    for (i = 0; i < 8; i++) {
        brains_tick(&scr, seqs, &mask, 100 * (i + 20), 0);
    }
    expect(!brains_slot_live(8), "barrel live gone after smash");
    expect(scr.sprite[8].active, "barrel debris stays");
    expect(scr.sprite[8].type == 0, "barrel debris is background");
    expect(scr.sprite[8].seq == 173, "barrel debris seq 173");
    expect(scr.sprite[8].hard == 1, "baked smash not hard");
    expect(brains_slot_hard(8) == 1, "brain hard survives bake");

    printf("OK test_playtest\n");
    return 0;
}
