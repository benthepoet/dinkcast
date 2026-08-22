/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "player.h"
#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    uint8_t buf[DINK_SAVE_MAX];
    size_t n = 0;
    struct Player a, b;
    char line[80];
    char dir[] = "build/savetest";
    int args[8];

    dinkc_var_init();
    dinkc_cmd_reset_inv();
    player_init(&a);
    a.x = 323;
    a.y = 390;
    a.dir = 8;
    a.strength = 5;
    dinkc_cmd_bind_player(&a);
    dinkc_var_set("&player_map", 192, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&story", 4, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&gold", 77, DINKC_GLOBAL_SCOPE, 1);
    memset(args, 0, sizeof(args));
    args[1] = 438;
    args[2] = 1;
    expect(dinkc_cmd("add_item", args, 3, "item-fst", NULL, NULL, NULL) == 1,
           "add fist");
    dinkc_cmd_spmap_put(439, 13, 1, 61, 2);
    save_set_info("Level &level");
    expect(save_pack(buf, sizeof(buf), &n, &a) == 0, "pack");
    expect(n > 64 && n <= DINK_SAVE_MAX, "size");
    expect(n < 8192, "under 8k");

    player_init(&b);
    dinkc_var_init();
    dinkc_cmd_reset_inv();
    expect(save_unpack(buf, n, &b) == 0, "unpack");
    expect(b.x == 323 && b.y == 390 && b.dir == 8, "xy dir");
    expect(b.strength == 5, "str");
    expect(dinkc_var_get("&player_map", DINKC_GLOBAL_SCOPE, 1) == 192, "map");
    expect(dinkc_var_get("&story", DINKC_GLOBAL_SCOPE, 1) == 4, "story");
    expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 77, "gold");
    expect(dinkc_cmd_inv_active(0, 0) == 1, "fist kept");
    {
        int map = 0, ed = 0, type = 0, seq = 0, fr = 0;

        expect(dinkc_cmd_spmap_count() == 1, "sparse 1");
        expect(dinkc_cmd_spmap_at(0, &map, &ed, &type, &seq, &fr) == 0,
               "spmap at");
        expect(map == 439 && ed == 13 && type == 1 && seq == 61 && fr == 2,
               "spmap fields");
    }

    (void)system("mkdir -p build/savetest");
    save_set_dir(dir);
    expect(save_game_exist(1) == 0, "empty exist");
    expect(save_game_slot(1, &b) == 0, "host write");
    expect(save_game_exist(1) == 1, "exist");
    player_init(&a);
    dinkc_var_init();
    dinkc_cmd_reset_inv();
    expect(save_load_slot(1, &a) == 0, "host load");
    expect(a.x == 323 && dinkc_var_get("&story", DINKC_GLOBAL_SCOPE, 1) == 4,
           "reload");
    save_info_line(1, line, sizeof(line));
    expect(line[0] != '\0', "info line");
    save_info_line(2, line, sizeof(line));
    expect(strstr(line, "Empty") != NULL || strstr(line, "empty") != NULL,
           "empty line");

    {
        int ret = 0;

        memset(args, 0, sizeof(args));
        args[0] = 1;
        expect(dinkc_cmd("game_exist", args, 1, NULL, NULL, NULL, &ret) == 1 &&
                   ret == 1,
               "game_exist 1");
        args[0] = 2;
        expect(dinkc_cmd("game_exist", args, 1, NULL, NULL, NULL, &ret) == 1 &&
                   ret == 0,
               "game_exist 2");
        expect(dinkc_cmd("set_save_game_info", args, 0, "Level &level", NULL,
                        NULL, NULL) == 1,
               "set_save_game_info");
        args[0] = 1;
        expect(dinkc_cmd("save_game", args, 1, NULL, NULL, NULL, &ret) == 1 &&
                   ret == 1,
               "save_game cmd");
        player_init(&b);
        dinkc_var_init();
        dinkc_cmd_reset_inv();
        dinkc_cmd_bind_player(&b);
        expect(dinkc_cmd("load_game", args, 1, NULL, NULL, NULL, &ret) == 1 &&
                   ret == 1,
               "load_game cmd");
        expect(b.x == 323 && dinkc_var_get("&story", DINKC_GLOBAL_SCOPE, 1) == 4,
               "load_game restore");
    }
    unlink("build/savetest/save01.bin");
    rmdir(dir);
    printf("OK test_save n=%u\n", (unsigned)n);
    return 0;
}
