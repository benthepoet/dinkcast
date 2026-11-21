/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"
#include "saybox.h"
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
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
static int *g_missile_dmg;

static int frame_box_tall(int seq, int frame, int *w, int *h, int *cx, int *cy)
{
    (void)frame;
    if (seq != 41) {
        return 0;
    }
    *w = 40;
    *h = 90;
    *cx = 10;
    *cy = 200;
    return 1;
}

static int frame_box_80x90(int seq, int frame, int *w, int *h, int *cx, int *cy)
{
    if (seq != 40 || frame != 1) {
        return 0;
    }
    *w = 80;
    *h = 90;
    *cx = 35;
    *cy = 80;
    return 1;
}

static int on_missile_damage(int slot, const char *proc)
{
    if (proc == NULL || strcmp(proc, "DAMAGE") != 0) {
        return -1;
    }
    if (g_missile_dmg != NULL) {
        *g_missile_dmg = slot;
    }
    (void)brains_change_prop(slot, DINKC_SP_SEQ, 70);
    (void)brains_change_prop(slot, DINKC_SP_BRAIN, 7);
    return 0;
}

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

static int g_dd_calls, g_dd_dmg, g_dd_life_seen;
static const int *g_dd_life;

/* Stands in for main.c on_dink_damage: a 60x70 Dink frame, offset 30,60. */
static void dink_damage_hook(const struct Player *p, int damage)
{
    g_dd_calls++;
    g_dd_dmg = damage;
    g_dd_life_seen = g_dd_life != NULL ? *g_dd_life : -1;
    brains_draw_damage_box(p->x, p->y, 60, 70, 30, 60, damage, 1);
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

    /* ITEM-FB sp_brain_parm(junk, 1): missile must not eat itself on Dink. */
    pl.damage = 0;
    brains_reset();
    memset(&scr, 0, sizeof(scr));
    scr.sprite[3].active = 1;
    scr.sprite[3].type = 1;
    scr.sprite[3].brain = 11;
    scr.sprite[3].x = 200;
    scr.sprite[3].y = 200;
    scr.sprite[3].strength = 10;
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    expect(brains_change_prop(3, DINKC_SP_BRAIN_PARM, 1) == 1, "parm 1");
    brains_tick(&scr, seqs, &mask, 16, 0);
    expect(brains_slot_live(3), "fireball skips Dink parm");
    expect(pl.damage == 0, "fireball no self hit");

    {
        static int dmg_slot;

        int ms;

        dmg_slot = 0;
        dinkc_var_init();
        seqs[20].hl = -5;
        seqs[20].ht = -5;
        seqs[20].hr = 40;
        seqs[20].hb = 5;
        brains_bind_proc(on_missile_damage);
        g_missile_dmg = &dmg_slot;
        brains_reset();
        memset(&scr, 0, sizeof(scr));
        scr.sprite[8].active = 1;
        scr.sprite[8].type = 1;
        scr.sprite[8].brain = 9;
        scr.sprite[8].x = 200;
        scr.sprite[8].y = 200;
        scr.sprite[8].seq = 20;
        scr.sprite[8].hitpoints = 8;
        brains_bind_screen(&scr);
        brains_enter(&scr, 0);
        ms = brains_create(220, 200, 11, 20, 1);
        expect(ms >= 2, "missile create");
        expect(brains_slot_live(ms) && brains_slot_brain(ms) == 11, "ms live 11");
        expect(brains_slot_live(8), "tgt live");
        expect(brains_change_prop(ms, DINKC_SP_STRENGTH, 5) == 5, "str");
        expect(brains_change_prop(ms, DINKC_SP_BRAIN_PARM, 1) == 1, "parm Dink");
        brains_set_script(ms, "dam-fire");
        brains_tick(&scr, seqs, &mask, 16, 0);
        expect(dmg_slot == ms, "DAMAGE ran");
        expect(dinkc_var_get("&missile_target", DINKC_GLOBAL_SCOPE, 1) == 8,
               "missile_target");
        expect(brains_slot_live(ms), "DAMAGE keeps missile");
        expect(brains_slot_brain(ms) == 7, "seq 70 brain 7");
        expect(brains_slot_seq(ms) == 70, "explo seq 70");
        brains_bind_proc(NULL);
        g_missile_dmg = NULL;
    }

    /* DAM-BOM brain 17: keep going after a lower-slot overlap so Jack
     * (created after furniture) still takes strength. */
    {
        static int bomb_dmg;
        int ms, hp;

        bomb_dmg = 0;
        brains_bind_proc(on_missile_damage);
        g_missile_dmg = &bomb_dmg;
        brains_reset();
        memset(&scr, 0, sizeof(scr));
        seqs[20].hl = -20;
        seqs[20].ht = -20;
        seqs[20].hr = 20;
        seqs[20].hb = 20;
        scr.sprite[2].active = 1;
        scr.sprite[2].type = 1;
        scr.sprite[2].brain = 0;
        scr.sprite[2].x = 200;
        scr.sprite[2].y = 200;
        scr.sprite[2].seq = 20;
        scr.sprite[8].active = 1;
        scr.sprite[8].type = 1;
        scr.sprite[8].brain = 16;
        scr.sprite[8].x = 200;
        scr.sprite[8].y = 200;
        scr.sprite[8].seq = 20;
        scr.sprite[8].hitpoints = 50;
        brains_bind_screen(&scr);
        brains_enter(&scr, 0);
        ms = brains_create(200, 200, 17, 20, 1);
        expect(ms >= 2, "bomb create");
        expect(brains_change_prop(ms, DINKC_SP_STRENGTH, 8) == 8, "bomb str");
        expect(brains_change_prop(ms, DINKC_SP_RANGE, 30) == 30, "bomb range");
        brains_set_script(ms, "dam-bomn");
        srand(1);
        brains_tick(&scr, seqs, &mask, 16, 0);
        hp = brains_hitpoints(8);
        expect(hp > 0 && hp < 50, "bomb damages Jack past furniture");
        expect(brains_slot_live(ms), "bomb DAMAGE keeps missile");
        brains_bind_proc(NULL);
        g_missile_dmg = NULL;
    }

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

        int lf;

        expect(dinkc_cmd("hurt", args, 2, "", "", &yld, &rv) == 1, "hurt cmd");
        /* FreeDink dc_hurt queues spr[1].damage; human_brain applies it. */
        expect(dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1) == 10 &&
                   pl.damage == 3,
               "hurt queued, not applied");
        lf = dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1);
        (void)player_apply_life(&pl, &lf);
        expect(lf == 7 && pl.damage == 0, "hurt life on apply");
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

    /* FreeDink draw_damage: bitmap box, not the hardbox. 80x90, offset 35,80. */
    {
        int slot, i, fx = 0, fy = 0, fnum = 0, saw = 0;

        brains_reset();
        brains_bind_screen(NULL);
        brains_bind_frame_box(frame_box_80x90);
        slot = brains_create(200, 200, 16, 40, 1);
        expect(slot > 1, "floater host");
        expect(brains_hurt(slot, 5) == 5, "floater damage");
        brains_draw_damage(slot, NULL);
        for (i = 2; i <= 99; i++) {
            if (brains_floater_num(i, &fx, &fy, &fnum)) {
                saw = 1;
                break;
            }
        }
        expect(saw && fnum == 5, "floater 5");
        expect(fx == 200 - 35 + 80 / 5, "floater x bitmap/5");
        expect(fy == 200 - 80 - 90 / 3, "floater y bitmap/3");
        /* text_draw: 50 px box, hcenter, glyph 1 px left. Cell is 8. */
        expect(saybox_num_x(fx, 5) == fx + 25 - 8 / 2 - 1, "one digit");
        expect(saybox_num_x(fx, 10) == fx + 25 - 16 / 2 - 1, "two digits");
        brains_bind_frame_box(frame_box_tall);
        slot = brains_create(10, 10, 16, 41, 1);
        expect(brains_hurt(slot, 1) == 1, "clamp damage");
        brains_draw_damage(slot, NULL);
        saw = 0;
        for (i = 2; i <= 99; i++) {
            if (brains_floater_num(i, &fx, &fy, &fnum) && fnum == 1) {
                saw = 1;
                break;
            }
        }
        expect(saw && fy == 0, "floater y clamped");
        brains_bind_frame_box(NULL);
    }

    /* FreeDink human_brain: draw_damage(1) before *plife -= damage, one
     * floater per hit, placed from Dink's own frame (not editor sprites). */
    {
        struct Player dp;
        int life = 20, i, fx = 0, fy = 0, fnum = 0, saw = 0;

        brains_reset();
        brains_bind_screen(NULL);
        memset(&dp, 0, sizeof(dp));
        dp.x = 300;
        dp.y = 250;
        dp.damage = 7;
        g_dd_calls = 0;
        g_dd_life = &life;
        player_bind_damage_draw(dink_damage_hook);
        expect(player_apply_life(&dp, &life) == 0, "dink survives");
        expect(g_dd_calls == 1 && g_dd_dmg == 7, "one number per hit");
        expect(g_dd_life_seen == 20 && life == 13,
               "number drawn before life drops");
        expect(player_apply_life(&dp, &life) == 0 && g_dd_calls == 1,
               "no number when damage is 0");
        for (i = 2; i <= 99; i++) {
            if (brains_floater_num(i, &fx, &fy, &fnum)) {
                saw = 1;
                break;
            }
        }
        expect(saw && fnum == 7, "dink floater 7");
        expect(fx == 300 - 30 + 60 / 5 && fy == 250 - 60 - 70 / 3,
               "dink floater from his frame");
        brains_reset();
        brains_draw_damage_box(300, 250, 0, 0, 0, 0, 3, 1);
        saw = 0;
        for (i = 2; i <= 99; i++) {
            if (brains_floater_num(i, &fx, &fy, &fnum) && fnum == 3) {
                saw = 1;
                break;
            }
        }
        expect(saw && fx == 300 && fy == 210, "unknown frame: 40 above");
        /* DinkC hurt(1, n) on Dink: post-defense damage, one number. */
        {
            static const char hs[] = "void main(void)\n{\n  hurt(1, 5);\n}\n";
            int lifev;

            brains_reset();
            dinkc_vm_reset();
            /* make_global keeps an existing slot's value: set explicitly. */
            dinkc_var_make_global("&life", 20);
            dinkc_var_make_global("&defense", 3);
            dinkc_var_set("&life", 20, DINKC_GLOBAL_SCOPE, 1);
            dinkc_var_set("&defense", 3, DINKC_GLOBAL_SCOPE, 1);
            memset(&dp, 0, sizeof(dp));
            dp.x = 300;
            dp.y = 250;
            dinkc_cmd_bind_player(&dp);
            g_dd_calls = 0;
            g_dd_life = NULL;
            (void)dinkc_vm_start(hs, strlen(hs), 0);
            expect(g_dd_calls == 0 && dp.damage == 2 &&
                       dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1) == 20,
                   "DinkC hurt(1,5) def 3: queued 2, nothing drawn yet");
            /* main.c's single apply site (human_brain). */
            lifev = dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1);
            (void)player_apply_life(&dp, &lifev);
            expect(g_dd_calls == 1 && g_dd_dmg == 2,
                   "apply: one number, 2");
            expect(lifev == 18, "apply took 2 life");
            dinkc_cmd_bind_player(NULL);
        }
        player_bind_damage_draw(NULL);
        g_dd_life = NULL;
    }

    /* Part 2 bindings: set_dink_speed (game_compute_speed table, 0
     * ignored), stop_entire_game (only while a choice is up),
     * dink_can_walk_off_screen, sp_sound (change_sprite). */
    {
        int yld = 0, rv = 0, a[2], slot;

        player_reset_dink_speed();
        expect(player_walk_speed() == 3, "dinkspeed 3 -> 3 px");
        a[0] = 2;
        (void)dinkc_cmd("set_dink_speed", a, 1, "", "", &yld, &rv);
        expect(player_dink_speed() == 2 && player_walk_speed() == 6,
               "herb boots: dinkspeed 2 -> 6 px");
        a[0] = 0;
        (void)dinkc_cmd("set_dink_speed", a, 1, "", "", &yld, &rv);
        expect(player_dink_speed() == 2, "set_dink_speed(0) ignored (1.08)");
        a[0] = 1;
        (void)dinkc_cmd("set_dink_speed", a, 1, "", "", &yld, &rv);
        expect(player_walk_speed() == 12, "dinkspeed 1 -> 12 px");
        a[0] = 7;
        (void)dinkc_cmd("set_dink_speed", a, 1, "", "", &yld, &rv);
        expect(player_walk_speed() == 1, "dinkspeed >= 4 -> 1 px");
        player_reset_dink_speed();
        expect(player_walk_speed() == 3, "load_game reset to 3");

        a[0] = 1;
        (void)dinkc_cmd("stop_entire_game", a, 1, "", "", &yld, &rv);
        expect(dinkc_cmd_stop_entire_game(1) == 1, "stopped while choice up");
        expect(dinkc_cmd_stop_entire_game(1) == 1, "still stopped");
        expect(dinkc_cmd_stop_entire_game(0) == 0, "choice gone: clears");
        expect(dinkc_cmd_stop_entire_game(1) == 0, "stays cleared");

        expect(!dinkc_cmd_walk_off_screen(), "walk off default 0");
        a[0] = 1;
        (void)dinkc_cmd("dink_can_walk_off_screen", a, 1, "", "", &yld, &rv);
        expect(dinkc_cmd_walk_off_screen(), "S2-CULG walk off on");
        a[0] = 0;
        (void)dinkc_cmd("dink_can_walk_off_screen", a, 1, "", "", &yld, &rv);
        expect(!dinkc_cmd_walk_off_screen(), "walk off off");

        brains_reset();
        brains_bind_screen(NULL);
        dinkc_cmd_bind_sprite_change(brains_change_prop);
        slot = brains_create(100, 100, 0, 30, 1);
        a[0] = slot;
        a[1] = 34;
        (void)dinkc_cmd("sp_sound", a, 2, "", "", &yld, &rv);
        expect(brains_change_prop(slot, DINKC_SP_SOUND, -1) == 34,
               "sp_sound sets the sprite's sound (SAVEBOT 34)");
        dinkc_cmd_bind_sprite_change(NULL);
    }

    printf("OK test_hurt\n");
    return 0;
}
