/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Confirmed playtest pictures — see docs/PLAYTEST.md. */
#include "brains.h"
#include "dinkc_cmd.h"
#include "hard.h"
#include "hit.h"
#include "mapscr.h"
#include "player.h"
#include "tiles.h"

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

    /* S1-BRG2: dummy seq 64 is hardness only; spr.disabled skips draw. */
    {
        int dumb, sq = 0, fr = 0;

        brains_reset();
        brains_bind_screen(&scr);
        dumb = brains_create(360, 300, 0, 64, 1);
        expect(dumb >= 2, "bridge dummy slot");
        expect(brains_change_prop(dumb, DINKC_SP_HARD, 0) == 0, "dummy hard=0");
        expect(brains_change_prop(dumb, DINKC_SP_DISABLED, 1) == 1,
               "dummy disabled");
        expect(brains_slot_disabled(dumb), "dummy not drawn");
        expect(brains_seq_frame(dumb, &sq, &fr) && sq == 64,
               "dummy seq still 64");
        expect(brains_slot_live(dumb), "dummy stays for hardness");
    }
    /* S1-NOPAS: seq 64 lean-to is touch only; spr.nodraw skips draw. */
    {
        int wall, sq = 0, fr = 0;

        brains_reset();
        brains_bind_screen(&scr);
        wall = brains_create(310, 122, 0, 64, 2);
        expect(wall >= 2, "nopas slot");
        expect(brains_change_prop(wall, DINKC_SP_NODRAW, 1) == 1, "nodraw set");
        expect(brains_slot_nodraw(wall), "nopas not drawn");
        expect(brains_seq_frame(wall, &sq, &fr) && sq == 64, "nopas seq 64");
        expect(brains_slot_live(wall), "nopas still live for touch");
    }

    /* After fire: vis-0 pie table (seq 87/9) must not leave hardness when
     * 14.4c refused the pixels. Draw already skips edraw_find == NULL.
     * Type 2 hardness-only still stamps SET_SPRITE_INFO geom. */
    {
        struct HardMask table;
        int hl = -39, ht = -33, hr = 43, hb = 7;

        memset(&table, 0, sizeof(table));
        table.pix = calloc((size_t)DINK_PLAY_W * DINK_PLAY_H, 1);
        expect(table.pix != NULL, "table mask");
        expect(!hard_stamp_without_pixels(0), "type 0 needs pixels");
        expect(!hard_stamp_without_pixels(1), "type 1 needs pixels");
        expect(hard_stamp_without_pixels(2), "type 2 geom ok");
        hard_stamp_editor(&table, 319, 243, 1, 1, 0, hl, ht, hr, hb);
        expect(hard_get(&table, 319, 243) == 0, "missing table pixels no hard");
        hard_stamp_editor(&table, 319, 243, 1, 1, 1, hl, ht, hr, hb);
        expect(hard_get(&table, 319, 243) != 0, "loaded table stamps");
        memset(table.pix, 0, (size_t)DINK_PLAY_W * DINK_PLAY_H);
        hard_stamp_editor(&table, 319, 243, 2, 1, 0, hl, ht, hr, hb);
        expect(hard_get(&table, 319, 243) != 0, "type 2 stamps without pixels");
        hard_mask_free(&table);
    }

    /* Official S1-H1-4.c &story > 3: sp_active table (22) + beds (23/24)
     * then draw_hard_map. Type 1 hardness is live sprites only. */
    {
        struct HardMask house;
        int hl = -39, ht = -33, hr = 43, hb = 7;

        expect(!hard_stamp_editor_slot(1, 0), "dead type 1 no stamp");
        expect(hard_stamp_editor_slot(1, 1), "live type 1 stamps");
        expect(hard_stamp_editor_slot(0, 0), "type 0 still stamps");
        expect(hard_stamp_editor_slot(2, 0), "type 2 still stamps");
        memset(&house, 0, sizeof(house));
        house.pix = calloc((size_t)DINK_PLAY_W * DINK_PLAY_H, 1);
        expect(house.pix != NULL, "house mask");
        hard_stamp_editor(&house, 319, 243, 1, 1, 1, hl, ht, hr, hb);
        hard_stamp_editor(&house, 235, 298, 1, 1, 1, hl, ht, hr, hb);
        hard_stamp_editor(&house, 448, 297, 1, 1, 1, hl, ht, hr, hb);
        expect(hard_get(&house, 319, 243) != 0, "table hard while live");
        expect(hard_get(&house, 235, 298) != 0, "dink bed hard while live");
        expect(hard_get(&house, 448, 297) != 0, "mom bed hard while live");
        memset(house.pix, 0, (size_t)DINK_PLAY_W * DINK_PLAY_H);
        if (hard_stamp_editor_slot(1, 0)) {
            hard_stamp_editor(&house, 319, 243, 1, 1, 1, hl, ht, hr, hb);
            hard_stamp_editor(&house, 235, 298, 1, 1, 1, hl, ht, hr, hb);
            hard_stamp_editor(&house, 448, 297, 1, 1, 1, hl, ht, hr, hb);
        }
        hard_stamp_editor(&house, 199, 88, 0, 1, 1, -20, -20, 20, 20);
        expect(hard_get(&house, 319, 243) == 0, "table hard gone after sp_active");
        expect(hard_get(&house, 235, 298) == 0, "dink bed hard gone");
        expect(hard_get(&house, 448, 297) == 0, "mom bed hard gone");
        expect(hard_get(&house, 199, 88) != 0, "type 0 wall stays");
        hard_mask_free(&house);
    }

    printf("OK test_playtest\n");
    return 0;
}
