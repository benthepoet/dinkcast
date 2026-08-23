/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "fs.h"
#include "ini.h"
#include "inv.h"
#include "mapscr.h"
#include "pad.h"
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
    int x = 0, y = 0;
    struct SpriteFrame *fr;

    inv_cell_xy(0, 0, &x, &y);
    expect(x == DINK_INV_WX && y == DINK_INV_WY, "weapon 0");
    inv_cell_xy(0, 1, &x, &y);
    expect(x == DINK_INV_WX + DINK_INV_DX && y == DINK_INV_WY, "weapon 1");
    inv_cell_xy(0, 4, &x, &y);
    expect(x == DINK_INV_WX && y == DINK_INV_WY + DINK_INV_DY, "weapon 4");
    inv_cell_xy(1, 0, &x, &y);
    expect(x == DINK_INV_MX && y == DINK_INV_MY, "magic 0");
    inv_cell_xy(1, 1, &x, &y);
    expect(x == DINK_INV_MX + DINK_INV_DX && y == DINK_INV_MY, "magic 1");

    inv_reset();
    inv_open(0);
    expect(inv_showing() && inv_curitem() == 0 && inv_item_type() == 0,
           "open fists cell");
    inv_tick(0, DINK_PAD_RIGHT, 10);
    expect(inv_curitem() == 1, "right");
    inv_tick(0, DINK_PAD_RIGHT, 20);
    inv_tick(0, DINK_PAD_RIGHT, 30);
    inv_tick(0, DINK_PAD_RIGHT, 40);
    expect(inv_curitem() == 3, "no wrap right");
    inv_tick(0, DINK_PAD_LEFT, 50);
    inv_tick(0, DINK_PAD_LEFT, 60);
    inv_tick(0, DINK_PAD_LEFT, 70);
    expect(inv_curitem() == 0, "back to col 0");
    inv_tick(0, DINK_PAD_LEFT, 80);
    expect(inv_item_type() == DINK_INV_MAGIC_KIND && inv_curitem() == 1,
           "left to magic col 1");
    inv_tick(0, DINK_PAD_RIGHT, 90);
    expect(inv_item_type() == DINK_INV_WEAPON && inv_curitem() == 0,
           "right to weapon col 0");
    inv_tick(0, DINK_PAD_Y, 100);
    expect(!inv_showing(), "Y closes");

    if (getenv("DINK_DATA") == NULL || getenv("DINK_DATA")[0] == '\0') {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    expect(dink_fs_init() == 0, "fs");
    expect(residency_is_always("graphics/inter/menu/dir.ff"), "menu always");

    seqs = (struct SeqInfo *)calloc(DINK_MAX_SEQ, sizeof(*seqs));
    expect(seqs != NULL, "seqs");
    expect(ini_load(seqs, DINK_MAX_SEQ) == 0, "ini");

    memset(&scr, 0, sizeof(scr));
    memset(args, 0, sizeof(args));
    dinkc_vm_reset();
    player_init(&pl);
    dinkc_cmd_bind_player(&pl);
    dinkc_cmd_bind_seqs(seqs);
    script_bind_screen(&scr);

    args[1] = 438;
    args[2] = 1;
    expect(dinkc_cmd("add_item", args, 3, "item-fst", NULL, &yld, &rv) == 1,
           "add fists");
    args[1] = 438;
    args[2] = 2;
    expect(dinkc_cmd("add_item", args, 3, "item-pig", NULL, &yld, &rv) == 1,
           "add pig");
    expect(dinkc_cmd("free_items", NULL, 0, NULL, NULL, &yld, &rv) == 1 &&
               rv == 14,
           "free after 2");
    expect(dinkc_cmd("count_item", NULL, 0, "item-fst", NULL, &yld, &rv) == 1 &&
               rv == 1,
           "count fists");
    expect(dinkc_cmd("kill_this_item", NULL, 0, "item-pig", NULL, &yld, &rv) ==
               1,
           "kill pig");
    expect(dinkc_cmd("count_item", NULL, 0, "item-pig", NULL, &yld, &rv) == 1 &&
               rv == 0,
           "pig gone");
    args[1] = 438;
    args[2] = 2;
    expect(dinkc_cmd("add_item", args, 3, "item-pig", NULL, &yld, &rv) == 1,
           "readd pig");
    {
        const char *nut =
            "void touch(void)\n{\nint &junk = free_items();\n"
            "if (&junk < 1)\n{\nreturn;\n}\n"
            "add_item(\"item-nut\", 438, 19);\n}\n";
        int slot = dinkc_vm_start_proc(nut, strlen(nut), 1, "touch");
        int i;

        expect(slot > 0, "nut fiber");
        dinkc_vm_tick(0);
        expect(dinkc_cmd("free_items", NULL, 0, NULL, NULL, &yld, &rv) == 1 &&
                   rv == 13,
               "s1-nut path added");
        args[1] = 438;
        args[2] = 19;
        for (i = 0; i < 13; i++) {
            expect(dinkc_cmd("add_item", args, 3, "item-nut", NULL, &yld, &rv) ==
                       1,
                   "fill slot");
        }
        expect(dinkc_cmd("free_items", NULL, 0, NULL, NULL, &yld, &rv) == 1 &&
                   rv == 0,
               "full");
        slot = dinkc_vm_start_proc(nut, strlen(nut), 1, "touch");
        expect(slot > 0, "full fiber");
        dinkc_vm_tick(0);
        expect(dinkc_cmd("free_items", NULL, 0, NULL, NULL, &yld, &rv) == 1 &&
                   rv == 0,
               "full skip add");
    }
    dinkc_var_set("&cur_weapon", 1, DINKC_GLOBAL_SCOPE, 1);
    expect(dinkc_cmd("arm_weapon", NULL, 0, NULL, NULL, &yld, &rv) == 1,
           "arm fists");
    expect(dinkc_cmd("compare_weapon", args, 0, "item-fst", NULL, &yld, &rv) ==
                   1 &&
               rv == 1,
           "armed fists");

    expect(inv_load(seqs) == 0, "inv_load");
    fr = inv_menu_frame(1);
    expect(fr != NULL && fr->w > 0 && fr->h > 0, "seq 423 fr 1");
    expect(fr->w == 600 && fr->h == 400, "menu-01 600x400");
    expect((size_t)fr->tw * (size_t)fr->th * 2u == 1048576u, "POT 1024x512");
    expect(inv_cpu_bytes() >= 1048576u, "cpu before drop");
    inv_drop_cpu();
    expect(inv_menu_frame(1) != NULL && inv_menu_frame(1)->w == 600,
           "geom after drop");
    expect(inv_menu_frame(1)->argb1555 == NULL, "cpu freed");
    expect(inv_cpu_bytes() == 0, "inv cpu 0");
    fr = inv_icon_frame(438, 1);
    expect(fr != NULL && fr->w > 0 && fr->h > 0, "fists icon");
    fr = inv_icon_frame(438, 2);
    expect(fr != NULL && fr->w > 0 && fr->h > 0, "pig icon");

    inv_reset();
    inv_open(0);
    expect(inv_curitem() == 0, "cursor fists");
    inv_tick(0, DINK_PAD_RIGHT, 10);
    expect(inv_curitem() == 1, "cursor pig");
    inv_tick(0, DINK_PAD_A, 20);
    expect(dinkc_cmd("compare_weapon", args, 0, "item-pig", NULL, &yld, &rv) ==
                   1 &&
               rv == 1,
           "A arms pig");
    expect(dinkc_var_get("&cur_weapon", DINKC_GLOBAL_SCOPE, 1) == 2, "slot 2");
    inv_tick(0, DINK_PAD_Y, 30);
    expect(!inv_showing(), "close after arm");

    inv_free();
    free(seqs);
    printf("OK test_inv\n");
    return 0;
}
