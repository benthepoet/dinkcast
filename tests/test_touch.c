/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "fs.h"
#include "hit.h"
#include "ini.h"
#include "mapscr.h"
#include "player.h"
#include "script.h"
#include "start_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_touch_slot;

static void on_touch(int slot)
{
    g_touch_slot = slot;
}

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

static int has_item(const char *name)
{
    int i, yld = 0, rv = 0, args[8];

    memset(args, 0, sizeof(args));
    for (i = 1; i <= 16; i++) {
        dinkc_var_set("&cur_weapon", i, DINKC_GLOBAL_SCOPE, 1);
        rv = 0;
        if (dinkc_cmd("compare_weapon", args, 0, name, NULL, &yld, &rv) == 1 &&
            rv == 1) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    struct MapScreen scr;
    struct SeqInfo seqs[DINK_MAX_SEQ];
    struct Player pl;

    memset(&scr, 0, sizeof(scr));
    memset(&seqs, 0, sizeof(seqs));
    scr.sprite[5].active = 1;
    scr.sprite[5].type = 1;
    scr.sprite[5].x = 200;
    scr.sprite[5].y = 200;
    scr.sprite[5].seq = 10;
    seqs[10].nframes = 1;
    seqs[10].hl = -10;
    seqs[10].ht = -10;
    seqs[10].hr = 10;
    seqs[10].hb = 10;

    brains_reset();
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    expect(brains_slot_live(5), "live 5");
    expect(brains_change_prop(5, DINKC_SP_TOUCH, -1) == -1, "touch -1");
    expect(brains_touch_damage(5) == -1, "stored -1");

    player_init(&pl);
    pl.x = 200;
    pl.y = 200;
    hit_bind_player(&pl);
    hit_bind_touch(on_touch);
    g_touch_slot = 0;
    hit_touch_list(pl.x, pl.y, 1, NULL, 0, seqs);
    expect(g_touch_slot == 5, "overlap -1 locates TOUCH");

    g_touch_slot = 0;
    hit_touch_list(400, 400, 2, NULL, 0, seqs);
    expect(g_touch_slot == 0, "far miss");

    expect(brains_change_prop(5, DINKC_SP_TOUCH, 4) == 4, "touch dmg");
    pl.damage = 0;
    pl.notouch = 0;
    hit_touch_list(pl.x, pl.y, 10, NULL, 0, seqs);
    expect(pl.damage == 4 && pl.notouch == 1 && pl.last_hit == 5, "hurt + notouch");
    {
        int d0 = pl.damage;

        hit_touch_list(pl.x, pl.y, 20, NULL, 0, seqs);
        expect(pl.damage == d0, "notouch blocks");
        hit_touch_list(pl.x, pl.y, 411, NULL, 0, seqs);
        expect(pl.damage == d0 + 4, "notouch expired");
    }

    if (getenv("DINK_DATA") == NULL || getenv("DINK_DATA")[0] == '\0') {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    expect(dink_fs_init() == 0, "fs");
    memset(&scr, 0, sizeof(scr));
    strncpy(scr.sprite[5].script, "s1-sack", sizeof(scr.sprite[5].script) - 1);
    scr.sprite[5].active = 1;
    scr.sprite[5].type = 1;
    scr.sprite[5].x = 200;
    scr.sprite[5].y = 200;
    scr.sprite[5].seq = 10;
    dinkc_vm_reset();
    dinkc_cmd_reset_inv();
    dinkc_var_set("&player_map", DINK_START_PLAYER_MAP, DINKC_GLOBAL_SCOPE, 1);
    player_init(&pl);
    pl.x = 200;
    pl.y = 200;
    dinkc_cmd_bind_player(&pl);
    dinkc_cmd_bind_sprite_change(brains_change_prop);
    dinkc_cmd_bind_item(script_item_arm, script_item_locate, script_item_pickup);
    hit_bind_player(&pl);
    hit_bind_touch(script_on_touch);
    brains_reset();
    brains_bind_screen(&scr);
    brains_enter(&scr, 0);
    script_bind_screen(&scr);
    script_bind_note_script(brains_set_script);
    expect(script_attach_live() >= 1, "sack main");
    expect(brains_touch_damage(5) == -1, "main set touch -1");
    hit_touch_list(pl.x, pl.y, 1, NULL, 0, seqs);
    dinkc_vm_tick(1);
    expect(has_item("item-pig"), "add_item item-pig");
    expect(brains_slot_live(5), "sack still live for scale_brain");
    expect(brains_change_prop(5, DINKC_SP_BRAIN, -1) == 12, "brain 12");
    {
        int t;

        for (t = 0; t < 40; t++) {
            brains_tick(&scr, seqs, NULL, 16 + t * 16, 0);
        }
        expect(!brains_slot_live(5), "scale_brain removed sack");
    }
    dinkc_cmd_apply_spmap(&scr, DINK_START_PLAYER_MAP);
    expect(scr.sprite[5].active == 0, "spmap type 1");

    {
        struct MapScreen barrel;
        int yld = 0, rv = 0, args[2];

        memset(&barrel, 0, sizeof(barrel));
        barrel.sprite[4].active = 1;
        barrel.sprite[4].type = 1;
        barrel.sprite[4].hard = 0;
        dinkc_var_set("&player_map", DINK_START_PLAYER_MAP, DINKC_GLOBAL_SCOPE,
                      1);
        args[0] = 4;
        args[1] = 3;
        expect(dinkc_cmd("editor_type", args, 2, "", "", &yld, &rv) == 1,
               "editor_type 3");
        dinkc_cmd_apply_spmap(&barrel, DINK_START_PLAYER_MAP);
        expect(barrel.sprite[4].type == 0 && barrel.sprite[4].hard == 1,
               "type 3 is bg not hard");
    }

    printf("OK test_touch\n");
    return 0;
}
