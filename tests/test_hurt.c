/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "hit.h"
#include "hurt.h"
#include "mapscr.h"
#include "player.h"
#include "tiles.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int g_die_slot;
static char g_die_proc[16];
static int g_exp_add;
static int g_hit_slot;
static int g_hit_from;

static void on_kill(int slot, const char *proc)
{
    g_die_slot = slot;
    strncpy(g_die_proc, proc != NULL ? proc : "", sizeof(g_die_proc) - 1u);
}

static void on_exp(int num)
{
    g_exp_add += num;
}

static void on_hit(int slot, int attacker)
{
    g_hit_slot = slot;
    g_hit_from = attacker;
}

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

static void le_i32_put(uint8_t *p, int off, int32_t v)
{
    p[off] = (uint8_t)(v & 0xff);
    p[off + 1] = (uint8_t)((v >> 8) & 0xff);
    p[off + 2] = (uint8_t)((v >> 16) & 0xff);
    p[off + 3] = (uint8_t)((v >> 24) & 0xff);
}

int main(void)
{
    struct MapScreen scr;
    struct SeqInfo seqs[DINK_MAX_SEQ];
    struct HardMask mask;
    struct Player pl;
    uint8_t *raw;
    int life;

    expect(hurt_roll(0, 0) == 0, "hurt 0");
    expect(hurt_roll(5, 0) == 5, "hurt 5-0");
    expect(hurt_roll(5, 2) == 3, "hurt 5-2");

    memset(&scr, 0, sizeof(scr));
    memset(&seqs, 0, sizeof(seqs));
    memset(&mask, 0, sizeof(mask));
    strncpy(seqs[164].prefix, "graphics/effects/die-", sizeof(seqs[164].prefix) - 1);
    strncpy(seqs[167].prefix, "graphics/effects/die-", sizeof(seqs[167].prefix) - 1);
    seqs[164].nframes = 1;
    seqs[167].nframes = 1;

    scr.sprite[5].active = 1;
    scr.sprite[5].type = 1;
    scr.sprite[5].brain = 16;
    scr.sprite[5].x = 200;
    scr.sprite[5].y = 200;
    scr.sprite[5].hitpoints = 4;
    scr.sprite[5].defense = 0;
    scr.sprite[5].exp = 7;
    scr.sprite[5].base_die = -1;
    strncpy(scr.sprite[5].script, "en-bonc", sizeof(scr.sprite[5].script) - 1);

    brains_reset();
    brains_bind_screen(&scr);
    brains_bind_kill(on_kill);
    brains_bind_exp(on_exp);
    brains_enter(&scr, 0);
    expect(brains_slot_live(5), "npc live");
    expect(brains_hitpoints(5) == 4, "map hp");

    brains_set_last_hit(5, 1);
    expect(brains_hurt(5, 5) == 5, "hurt_thing 5");
    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(g_die_slot == 5 && strcmp(g_die_proc, "die") == 0, "DIE");
    expect(!brains_slot_live(5), "npc gone");
    expect(g_exp_add == 7, "exp last_hit 1");
    {
        int i, corpse = 0, fx, fy, fnum, dmg = 0;

        for (i = 2; i <= 99; i++) {
            if (brains_floater_num(i, &fx, &fy, &fnum)) {
                dmg = fnum;
            }
            if (brains_slot_created(i) && brains_slot_live(i) &&
                brains_slot_brain(i) != 8) {
                corpse = i;
            }
        }
        expect(dmg == 5, "draw_damage number");
        expect(corpse != 0, "corpse created");
    }

    raw = (uint8_t *)calloc(1, (size_t)DINK_MAP_RECSIZE);
    expect(raw != NULL, "raw");
    le_i32_put(raw, 8020 + 220 * 3 + 168, 12);
    le_i32_put(raw, 8020 + 220 * 3 + 176, 2);
    le_i32_put(raw, 8020 + 220 * 3 + 180, 9);
    le_i32_put(raw, 8020 + 220 * 3 + 192, 1);
    {
        struct MapScreen parsed;

        memset(&parsed, 0, sizeof(parsed));
        expect(map_parse_mem(raw, DINK_MAP_RECSIZE, &parsed) == 0, "parse");
        expect(parsed.sprite[3].hitpoints == 12, "parse hp");
        expect(parsed.sprite[3].defense == 2, "parse def");
        expect(parsed.sprite[3].exp == 9, "parse exp");
        expect(parsed.sprite[3].nohit == 1, "parse nohit");
    }
    /* load_screen_to +100/+104/+108 (editor_screen.cpp). */
    le_i32_put(raw, 8020 + 220 * 4 + 24, 1);
    le_i32_put(raw, 8020 + 220 * 4 + 16, 1);
    le_i32_put(raw, 8020 + 220 * 4 + 100, 10);
    le_i32_put(raw, 8020 + 220 * 4 + 104, 540);
    le_i32_put(raw, 8020 + 220 * 4 + 108, 100);
    {
        struct MapScreen parsed;

        memset(&parsed, 0, sizeof(parsed));
        expect(map_parse_mem(raw, DINK_MAP_RECSIZE, &parsed) == 0,
               "parse bases");
        expect(parsed.sprite[4].base_idle == 10, "parse base_idle");
        expect(parsed.sprite[4].base_attack == 540, "parse base_attack");
        expect(parsed.sprite[4].base_hit == 100, "parse base_hit");
        parsed.sprite[4].active = 1;
        parsed.sprite[4].type = 1;
        brains_reset();
        brains_bind_screen(&parsed);
        brains_enter(&parsed, 0);
        expect(brains_base_idle(4) == 10, "enter base_idle");
        expect(brains_base_attack(4) == 540, "enter base_attack");
        expect(brains_base_hit(4) == 100, "enter base_hit");
        brains_reset();
    }
    free(raw);

    brains_reset();
    memset(&scr, 0, sizeof(scr));
    scr.sprite[8].active = 1;
    scr.sprite[8].type = 1;
    scr.sprite[8].brain = 9;
    scr.sprite[8].x = 200;
    scr.sprite[8].y = 200;
    scr.sprite[8].hitpoints = 6;
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    player_init(&pl);
    pl.x = 200;
    pl.y = 200;
    pl.dir = 4;
    hit_bind_player(&pl);
    hit_bind_hit(on_hit);
    srand(1);
    hit_tag_list(1, 200, 200, 4, 2, 0, NULL, 0, seqs);
    expect(g_hit_slot == 8 && g_hit_from == 1, "HIT script");
    brains_tick(&scr, seqs, &mask, 32, 0);
    expect(brains_hitpoints(8) < 6, "hp dropped");

    brains_reset();
    memset(&scr, 0, sizeof(scr));
    g_die_slot = 0;
    g_die_proc[0] = '\0';
    g_exp_add = 0;
    strncpy(seqs[21].prefix, "graphics/animals/duck/dk1w-",
            sizeof(seqs[21].prefix) - 1);
    strncpy(seqs[111].prefix, "graphics/animals/duck/death/dkb1x-",
            sizeof(seqs[111].prefix) - 1);
    strncpy(seqs[121].prefix, "graphics/animals/duck/death/dkh1x-",
            sizeof(seqs[121].prefix) - 1);
    seqs[21].nframes = 1;
    seqs[111].nframes = 1;
    seqs[121].nframes = 1;
    scr.sprite[4].active = 1;
    scr.sprite[4].type = 1;
    scr.sprite[4].brain = 3;
    scr.sprite[4].x = 200;
    scr.sprite[4].y = 200;
    scr.sprite[4].seq = 21;
    scr.sprite[4].base_walk = 20;
    scr.sprite[4].hitpoints = 5;
    scr.sprite[4].exp = 1;
    brains_bind_screen(&scr);
    brains_bind_kill(on_kill);
    brains_bind_exp(on_exp);
    brains_enter(&scr, 0);
    brains_set_last_hit(4, 1);
    expect(brains_hurt(4, 5) == 5, "duck first hurt");
    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(brains_slot_live(4), "duck stays after first hit");
    expect(g_die_slot == 4 && strcmp(g_die_proc, "die") == 0, "duck DIE");
    {
        int i, head = 0;

        for (i = 2; i <= 99; i++) {
            if (i != 4 && brains_slot_created(i) && brains_slot_live(i) &&
                brains_slot_brain(i) != 8) {
                head = i;
                break;
            }
        }
        expect(head != 0, "duck flying head");
    }
    g_die_slot = 0;
    g_die_proc[0] = '\0';
    g_exp_add = 0;
    brains_set_last_hit(4, 1);
    expect(brains_hurt(4, 5) == 5, "duck second hurt");
    brains_tick(&scr, seqs, &mask, 32, 0);
    expect(!brains_slot_live(4), "duck gone on second hit");
    expect(g_die_slot == 4 && strcmp(g_die_proc, "duckdie") == 0,
           "second punch DUCKDIE");

    brains_reset();
    memset(&scr, 0, sizeof(scr));
    g_die_slot = 0;
    g_die_proc[0] = '\0';
    g_exp_add = 0;
    scr.sprite[4].active = 1;
    scr.sprite[4].type = 1;
    scr.sprite[4].brain = 3;
    scr.sprite[4].x = 200;
    scr.sprite[4].y = 200;
    scr.sprite[4].seq = 110;
    scr.sprite[4].hitpoints = 1;
    scr.sprite[4].exp = 3;
    brains_bind_screen(&scr);
    brains_bind_kill(on_kill);
    brains_bind_exp(on_exp);
    brains_enter(&scr, 0);
    brains_set_last_hit(4, 1);
    expect(brains_hurt(4, 5) == 5, "duck hurt");
    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(g_die_slot == 4 && strcmp(g_die_proc, "duckdie") == 0, "DUCKDIE");
    expect(g_exp_add == 3, "duck exp");

    brains_reset();
    memset(&scr, 0, sizeof(scr));
    scr.sprite[1].active = 1;
    scr.sprite[1].type = 1;
    scr.sprite[1].brain = 9;
    scr.sprite[1].x = 200;
    scr.sprite[1].y = 200;
    scr.sprite[1].hitpoints = 6;
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    player_init(&pl);
    pl.x = 200;
    pl.y = 200;
    pl.dir = 4;
    hit_bind_player(&pl);
    g_hit_slot = 0;
    hit_bind_hit(on_hit);
    srand(1);
    hit_tag_list(1, 200, 200, 4, 5, 0, NULL, 0, seqs);
    expect(g_hit_slot == 1, "slot 1 HIT");
    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(brains_hitpoints(1) < 6, "slot 1 npc hit");

    brains_reset();
    memset(&scr, 0, sizeof(scr));
    seqs[20].hl = -5;
    seqs[20].ht = -5;
    seqs[20].hr = 40;
    seqs[20].hb = 5;
    scr.sprite[3].active = 1;
    scr.sprite[3].type = 1;
    scr.sprite[3].brain = 11;
    scr.sprite[3].x = 220;
    scr.sprite[3].y = 200;
    scr.sprite[3].strength = 5;
    scr.sprite[8].active = 1;
    scr.sprite[8].type = 1;
    scr.sprite[8].brain = 9;
    scr.sprite[8].x = 200;
    scr.sprite[8].y = 200;
    scr.sprite[8].seq = 20;
    scr.sprite[8].hitpoints = 1;
    scr.sprite[8].exp = 9;
    g_exp_add = 0;
    brains_bind_kill(on_kill);
    brains_bind_exp(on_exp);
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(!brains_slot_live(8), "missile seq hardbox kill");
    expect(g_exp_add == 9, "missile last_hit 1 exp");

    player_init(&pl);
    pl.x = 200;
    pl.y = 200;
    seqs[pl.seq].hl = -5;
    seqs[pl.seq].ht = -5;
    seqs[pl.seq].hr = 40;
    seqs[pl.seq].hb = 5;
    brains_bind_player(&pl);
    brains_reset();
    memset(&scr, 0, sizeof(scr));
    scr.sprite[3].active = 1;
    scr.sprite[3].type = 1;
    scr.sprite[3].brain = 11;
    scr.sprite[3].x = 220;
    scr.sprite[3].y = 200;
    scr.sprite[3].strength = 4;
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(pl.damage > 0, "missile dink hardbox");

    brains_reset();
    memset(&scr, 0, sizeof(scr));
    scr.sprite[5].active = 1;
    scr.sprite[5].type = 1;
    scr.sprite[5].brain = 16;
    scr.sprite[5].x = 200;
    scr.sprite[5].y = 200;
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    dinkc_cmd_bind_sprite_change(brains_change_prop);
    dinkc_cmd_bind_brain_lookup(brains_first_with_brain, brains_rand_with_brain);
    {
        int yld = 0, rv = 0, args[2] = {5, 4};

        expect(dinkc_cmd("sp_strength", args, 2, "", "", &yld, &rv) == 1,
               "sp_strength cmd");
        expect(brains_strength(5) == 4, "sp_strength npc");
        args[1] = 1;
        expect(dinkc_cmd("sp_follow", args, 2, "", "", &yld, &rv) == 1 &&
                   brains_follow(5) == 1,
               "sp_follow BrainSpr");
        expect(dinkc_cmd("sp_target", args, 2, "", "", &yld, &rv) == 1 &&
                   brains_target(5) == 1,
               "sp_target BrainSpr");
        args[1] = 12;
        expect(dinkc_cmd("sp_distance", args, 2, "", "", &yld, &rv) == 1 &&
                   brains_change_prop(5, DINKC_SP_DISTANCE, -1) == 12,
               "sp_distance BrainSpr");
        dinkc_cmd_set_now(0);
        args[1] = 50;
        expect(dinkc_cmd("sp_attack_wait", args, 2, "", "", &yld, &rv) == 1 &&
                   rv == 50,
               "sp_attack_wait now=0 stores arg");
        dinkc_cmd_set_now(1000);
        args[1] = 50;
        expect(dinkc_cmd("sp_attack_wait", args, 2, "", "", &yld, &rv) == 1 &&
                   rv == 1050,
                   "sp_attack_wait + thisTickCount");
        {
            int look[3] = {16, 0, 0};

            expect(dinkc_cmd("get_sprite_with_this_brain", look, 2, "", "",
                             &yld, &rv) == 1 &&
                       rv == 5,
                   "get_sprite people");
            look[1] = 5;
            expect(dinkc_cmd("get_sprite_with_this_brain", look, 2, "", "",
                             &yld, &rv) == 1 &&
                       rv == 0,
                   "get_sprite ignore self");
            look[0] = 16;
            look[1] = 0;
            look[2] = 5;
            expect(dinkc_cmd("get_next_sprite_with_this_brain", look, 3, "", "",
                             &yld, &rv) == 1 &&
                       rv == 5,
                   "get_next from 5");
        }
    }

    dinkc_var_init();
    player_init(&pl);
    dinkc_cmd_bind_player(&pl);
    {
        int yld = 0, rv = 0, args[2] = {1, 3};

        expect(dinkc_cmd("hurt", args, 2, "", "", &yld, &rv) == 1, "hurt cmd");
        expect(dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1) == 7, "hurt life");
    }
    life = 2;
    pl.damage = 5;
    expect(player_apply_life(&pl, &life) == 1 && life == 0, "life 0");

    player_init(&pl);
    pl.x = 100;
    pl.y = 100;
    pl.dir = 6;
    mask.pix = calloc((size_t)DINK_PLAY_W * DINK_PLAY_H, 1);
    expect(mask.pix != NULL, "mask");
    {
        int i;

        for (i = 0; i < DINK_PLAY_H; i++) {
            mask.pix[i * DINK_PLAY_W + 90] = 1;
        }
        for (i = 0; i < 40; i++) {
            player_step(&pl, 6, &mask, seqs, 0, NULL);
        }
        expect(pl.push_active, "push armed");
        player_step(&pl, 6, &mask, seqs, 601, NULL);
        expect(pl.just_push && pl.nocontrol && pl.seq == DINK_BASE_PUSH + 6,
               "push seq");
    }
    hard_mask_free(&mask);

    printf("OK test_hurt\n");
    return 0;
}
