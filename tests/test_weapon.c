/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "ff.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "player.h"
#include "residency.h"
#include "script.h"

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
    struct SeqInfo *seqs;
    struct Player pl;
    int args[8];
    int yld = 0, rv = 0;

    if (getenv("DINK_DATA") == NULL || getenv("DINK_DATA")[0] == '\0') {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    expect(dink_fs_init() == 0, "fs");
    expect(residency_is_always("graphics/dink/sword/walk/dir.ff"), "sword always");
    expect(residency_is_always("graphics/dink/bow/walk/dir.ff"), "bow always");
    expect(residency_is_always("graphics/dink/die/dir.ff"), "die always");
    expect(!residency_is_always("graphics/people/mom/dir.ff"), "mom not always");

    seqs = (struct SeqInfo *)calloc(DINK_MAX_SEQ, sizeof(*seqs));
    expect(seqs != NULL, "seqs");
    expect(ini_load(seqs, DINK_MAX_SEQ) == 0, "ini");

    memset(&scr, 0, sizeof(scr));
    memset(args, 0, sizeof(args));
    dinkc_vm_reset();
    player_init(&pl);
    pl.dir = 6;
    dinkc_cmd_bind_player(&pl);
    dinkc_cmd_bind_seqs(seqs);
    dinkc_cmd_bind_create(brains_create);
    dinkc_cmd_bind_sprite_change(brains_change_prop);
    brains_reset();
    script_bind_screen(&scr);

    args[1] = 438;
    args[2] = 1;
    expect(dinkc_cmd("add_item", args, 3, "item-fst", NULL, &yld, &rv) == 1,
           "add fists");
    dinkc_var_set("&cur_weapon", 1, DINKC_GLOBAL_SCOPE, 1);
    expect(dinkc_cmd("arm_weapon", NULL, 0, NULL, NULL, &yld, &rv) == 1, "arm fst");
    expect(dinkc_cmd_weapon_armed(), "fists armed");
    expect(dinkc_cmd("compare_weapon", args, 0, "item-fst", NULL, &yld, &rv) == 1 &&
               rv == 1,
           "compare fists");
    expect(dinkc_cmd_weapon_use() == 1, "use fists");
    expect(pl.seq == 106, "fist seq 100+dir");
    expect(pl.nocontrol == 1, "nocontrol");
    expect(pl.base_hit == 100, "base_hit");

    args[1] = 438;
    args[2] = 7;
    expect(dinkc_cmd("add_item", args, 3, "item-sw1", NULL, &yld, &rv) == 1,
           "add sword");
    dinkc_var_set("&cur_weapon", 2, DINKC_GLOBAL_SCOPE, 1);
    expect(dinkc_var_get("&strength", DINKC_GLOBAL_SCOPE, 1) == 3, "str before");
    expect(dinkc_cmd("arm_weapon", NULL, 0, NULL, NULL, &yld, &rv) == 1, "arm sw");
    expect(dinkc_var_get("&strength", DINKC_GLOBAL_SCOPE, 1) == 7, "str +4");
    expect(pl.range == 40, "sword range");
    expect(strstr(seqs[71].prefix, "sword") != NULL, "init sword walk");
    expect(ff_is_cached("graphics/dink/sword/walk/dir.ff"), "sword pack cached");
    printf("arm always_bytes=%u blob=%u\n",
           (unsigned)residency_bytes_always(), (unsigned)dink_blob_bytes());
    expect(dinkc_cmd("compare_weapon", args, 0, "item-sw1", NULL, &yld, &rv) == 1 &&
               rv == 1,
           "compare sword");
    memset(args, 0, sizeof(args));
    args[0] = 531;
    expect(dinkc_cmd("preload_seq", args, 1, NULL, NULL, &yld, &rv) == 1,
           "npc preload 531");
    expect(ff_is_cached("graphics/foes/bonca/walk/dir.ff"), "bonca walk open");
    residency_swap_begin();
    residency_swap_end();
    residency_swap_begin();
    residency_swap_end();
    expect(!ff_is_cached("graphics/foes/bonca/walk/dir.ff"),
           "npc preload_seq not Always");
    expect(ff_is_cached("graphics/dink/sword/walk/dir.ff"), "sword still Always");

    args[1] = 438;
    args[2] = 8;
    expect(dinkc_cmd("add_item", args, 3, "item-b1", NULL, &yld, &rv) == 1,
           "add bow");
    dinkc_var_set("&cur_weapon", 3, DINKC_GLOBAL_SCOPE, 1);
    expect(dinkc_cmd("arm_weapon", NULL, 0, NULL, NULL, &yld, &rv) == 1, "arm bow");
    expect(dinkc_cmd_weapon_use() == 1, "bow use 1");
    expect(dinkc_cmd_weapon_armed(), "bow keep after return");
    expect(dinkc_cmd_weapon_use() == 1, "bow use 2");

    args[1] = 437;
    args[2] = 1;
    expect(dinkc_cmd("add_magic", args, 3, "item-fb", NULL, &yld, &rv) == 1,
           "add fb");
    dinkc_var_set("&cur_magic", 1, DINKC_GLOBAL_SCOPE, 1);
    expect(dinkc_cmd("arm_magic", NULL, 0, NULL, NULL, &yld, &rv) == 1, "arm fb");
    expect(dinkc_cmd_magic_armed(), "magic armed");
    expect(dinkc_var_get("&magic_cost", DINKC_GLOBAL_SCOPE, 1) == 100, "cost");
    expect(!residency_is_always("graphics/effects/comets/sm-comt1/dir.ff"),
           "comet not named always");
    expect(ff_is_cached("graphics/effects/comets/sm-comt1/dir.ff"), "comet cached");
    /* Holds are best-effort: with sword+bow+fireball armed the blob is
     * near cap, so splode's ARM load last-resort drops the largest held
     * pack (treefire). Comets (small) must survive. */
    expect(ff_is_cached("graphics/effects/splode/dir.ff"),
           "splode fits via held last-resort drop");
    residency_touch("graphics/effects/comets/sm-comt1/dir.ff");
    residency_swap_begin();
    residency_swap_end();
    residency_swap_begin();
    residency_swap_end();
    /* ARM holds are Screen-held until DISARM: swaps must not demote them
     * to Prev and drop them (that was the impact-hitch GD-ROM re-read). */
    expect(ff_is_cached("graphics/effects/comets/sm-comt1/dir.ff"),
           "fb ARM preload held across swaps");
    /* DISARM releases the hold; two swaps later the packs are droppable. */
    dinkc_var_set("&cur_magic", 0, DINKC_GLOBAL_SCOPE, 1);
    expect(dinkc_cmd("arm_magic", NULL, 0, NULL, NULL, &yld, &rv) == 1,
           "disarm fb");
    residency_swap_begin();
    residency_swap_end();
    residency_swap_begin();
    residency_swap_end();
    expect(!ff_is_cached("graphics/effects/comets/sm-comt1/dir.ff"),
           "fb preload droppable after DISARM");
    /* Re-arm for the fireball-use check below. */
    dinkc_var_set("&cur_magic", 1, DINKC_GLOBAL_SCOPE, 1);
    expect(dinkc_cmd("arm_magic", NULL, 0, NULL, NULL, &yld, &rv) == 1,
           "re-arm fb");
    dinkc_var_set("&magic_level", 100, DINKC_GLOBAL_SCOPE, 1);
    dinkc_vm_set_now(1);
    expect(dinkc_cmd_magic_use() == 1, "use fb");
    dinkc_vm_tick(200);
    expect(brains_slot_live(2) || brains_slot_live(3) || brains_slot_live(4),
           "fireball sprite");

    free(seqs);
    printf("OK test_weapon\n");
    return 0;
}
