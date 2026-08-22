/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "hard.h"
#include "player.h"
#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_stub_brain[100];
static int g_stub_x[100];
static int g_move_on;
static const char *g_ext_src;
static const char *g_spawn_src;
static int g_load_map;
static int g_draw_spr;
static int g_nload;
static int g_ndraw;

static int stub_load_screen(int map)
{
    g_load_map = map;
    g_nload++;
    return 0;
}

static int stub_draw_screen(int sprite)
{
    g_draw_spr = sprite;
    g_ndraw++;
    dinkc_vm_kill_all();
    return 0;
}

static int stub_bar_sh_external(int sprite, const char *file, const char *proc,
                                const int *args, int nargs)
{
    (void)file;
    (void)args;
    (void)nargs;
    if (g_ext_src == NULL || proc == NULL) {
        return 0;
    }
    return dinkc_vm_start_proc(g_ext_src, strlen(g_ext_src), sprite, proc);
}

static int stub_spawn(const char *file)
{
    (void)file;
    if (g_spawn_src == NULL) {
        return 0;
    }
    return dinkc_vm_start_keep(g_spawn_src, strlen(g_spawn_src), 1000, "main");
}

static int stub_sp_change(int slot, int prop, int val)
{
    int *p = NULL;

    if (slot < 1 || slot > 99) {
        return -1;
    }
    if (prop == DINKC_SP_BRAIN) {
        p = &g_stub_brain[slot];
    } else if (prop == DINKC_SP_X) {
        p = &g_stub_x[slot];
    } else {
        return -1;
    }
    if (val != -1) {
        *p = val;
    }
    return *p;
}

static int stub_create(int x, int y, int brain, int seq, int fr)
{
    (void)y;
    (void)brain;
    (void)seq;
    (void)fr;
    g_stub_x[5] = x;
    return 5;
}

static int stub_move(int slot, int dir, int dest, int nohard)
{
    (void)slot;
    (void)dir;
    (void)dest;
    (void)nohard;
    g_move_on = 1;
    return 1;
}

static int stub_moving(int slot)
{
    (void)slot;
    return g_move_on;
}

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    const char *wait1 = "void main(void) { wait(1); }";
    const char *say = "void main(void) { say_stop(\"hi\", 1); }";
    const char *talk =
        "void talk(void) { freeze(1); choice_start(); \"A\" choice_end(); "
        "say_stop(\"fed\", 1); unfreeze(1); }";
    struct Player pl;
    int slot, i;

    dinkc_vm_reset();
    slot = dinkc_vm_start(wait1, strlen(wait1), 1);
    expect(slot > 0, "start wait");
    expect(dinkc_vm_state(slot) == DINKC_WAIT_MS, "yield wait");
    expect(dinkc_vm_live() == 1, "live 1");
    for (i = 0; i < 4 && dinkc_vm_live() > 0; i++) {
        dinkc_vm_tick(i * DINKC_TICK_MS);
    }
    expect(dinkc_vm_live() == 0, "wait(1) done in 60 Hz ticks");

    dinkc_vm_reset();
    slot = dinkc_vm_start(say, strlen(say), 1);
    expect(dinkc_vm_waiting_say() == 1, "say yield");
    dinkc_vm_tick(1000);
    expect(dinkc_vm_waiting_say() == 1, "tick does not skip say");
    dinkc_vm_advance_say();
    expect(dinkc_vm_live() == 0, "A ends say_stop");

    memset(&pl, 0, sizeof(pl));
    dinkc_cmd_bind_player(&pl);
    dinkc_vm_reset();
    slot = dinkc_vm_start_proc(talk, strlen(talk), 26, "talk");
    expect(slot > 0 && dinkc_vm_waiting_choice(), "talk choice");
    expect(pl.freeze == 1, "freeze(1)");
    dinkc_vm_choice_pick(1);
    expect(dinkc_vm_waiting_say(), "say after choice");
    expect(dinkc_var_get("&result", DINKC_GLOBAL_SCOPE, 1) == 1, "result 1");
    expect(dinkc_vm_choice_n() == 0, "choice over");
    dinkc_vm_advance_say();
    expect(dinkc_vm_live() == 0 && pl.freeze == 0, "unfreeze");
    {
        const char *keep =
            "void arm(void) { int &x; } void use(void) { &x = 1; return; }";
        int ks;

        dinkc_vm_reset();
        ks = dinkc_vm_start_keep(keep, strlen(keep), 1000, "arm");
        expect(ks > 0 && dinkc_vm_used(ks), "keep arm");
        expect(dinkc_vm_locate(ks, "use") == ks, "locate use");
        expect(dinkc_vm_used(ks), "keep after return");
        dinkc_vm_kill_all();
        expect(dinkc_vm_used(ks), "item keep survives swap");
        {
            const char *sprk = "void main(void) { int &m; }";
            int ss;

            dinkc_vm_reset();
            ss = dinkc_vm_start_keep(sprk, strlen(sprk), 4, "main");
            expect(ss > 0 && dinkc_vm_used(ss), "sprite keep");
            dinkc_vm_kill_all();
            expect(!dinkc_vm_used(ss), "sprite keep dies on swap");
        }
        {
            const char *gg =
                "void main(void) { int &mrandom; }\n"
                "void hit(void) { &mrandom = random(3, 1);\n"
                " if (&mrandom == 1) say_stop(\"a\", 4);\n"
                " if (&mrandom == 2) say_stop(\"b\", 4);\n"
                " if (&mrandom == 3) say_stop(\"c\", 4); }\n";
            int gs;

            dinkc_vm_reset();
            gs = dinkc_vm_start_keep(gg, strlen(gg), 4, "main");
            expect(gs > 0 && dinkc_vm_sprite_fiber(4) == gs, "gg fiber");
            expect(dinkc_vm_locate(gs, "hit") == gs, "gg hit");
            expect(dinkc_vm_waiting_say(), "gg hit say");
        }
    }
    {
        const char *orphan = "void main(void) { freeze(1); }";

        dinkc_vm_reset();
        memset(&pl, 0, sizeof(pl));
        dinkc_cmd_bind_player(&pl);
        slot = dinkc_vm_start(orphan, strlen(orphan), 1);
        expect(pl.freeze == 1 && dinkc_vm_live() == 0, "died frozen");
        dinkc_cmd_thaw_if_idle();
        expect(pl.freeze == 0, "orphan thaw");
        (void)slot;
    }

    {
        const char *skipc =
            "void main(void) { choice_start(); (&story == 1) \"hid\"; "
            "\"vis\"; choice_end(); }";

        dinkc_vm_reset();
        slot = dinkc_vm_start(skipc, strlen(skipc), 1);
        expect(slot > 0 && dinkc_vm_waiting_choice(), "skip hidden");
        dinkc_vm_choice_pick(1);
        expect(dinkc_var_get("&result", DINKC_GLOBAL_SCOPE, 1) == 2,
               "hidden still counts");
    }
    {
        const char *two =
            "void main(void) { choice_start(); \"A\"; \"B\"; choice_end(); }";

        dinkc_vm_reset();
        slot = dinkc_vm_start(two, strlen(two), 1);
        expect(dinkc_vm_choice_n() == 2 && dinkc_vm_choice_cur() == 1, "cur1");
        expect(strcmp(dinkc_vm_choice_line(1), "A") == 0, "line A");
        dinkc_vm_choice_move(1);
        expect(dinkc_vm_choice_cur() == 2, "cur2");
        dinkc_vm_choice_pick(dinkc_vm_choice_cur());
        expect(dinkc_var_get("&result", DINKC_GLOBAL_SCOPE, 1) == 2, "pick B");
    }
    {
        /* start-2.c load(): ten &savegameinfo + Nevermind. */
        const char *loadc =
            "void main(void) { choice_start(); \"&savegameinfo\"; "
            "\"Nevermind\"; choice_end(); }";
        char dir[] = "build/savetest2";

        (void)system("mkdir -p build/savetest2");
        save_set_dir(dir);
        dinkc_vm_reset();
        dinkc_var_init();
        slot = dinkc_vm_start(loadc, strlen(loadc), 1);
        expect(slot > 0 && dinkc_vm_waiting_choice(), "savegameinfo choice");
        expect(dinkc_vm_choice_n() == 2, "two lines");
        expect(strstr(dinkc_vm_choice_line(1), "Empty") != NULL ||
                   strstr(dinkc_vm_choice_line(1), "empty") != NULL,
               "slot empty token");
        expect(strcmp(dinkc_vm_choice_line(2), "Nevermind") == 0, "nevermind");
        dinkc_vm_choice_pick(2);
        (void)system("rm -rf build/savetest2");
        (void)dir;
    }
    {
        const char *titled =
            "void main(void) { choice_start(); set_y 240; set_title_color 5; "
            "title_start(); Hello title_end(); \"A\"; choice_end(); }";

        dinkc_vm_reset();
        slot = dinkc_vm_start(titled, strlen(titled), 1);
        expect(slot > 0 && dinkc_vm_waiting_choice(), "titled");
        expect(dinkc_vm_choice_newy() == 240, "set_y");
        expect(dinkc_vm_choice_color() == 5, "title color");
        expect(strstr(dinkc_vm_choice_title(), "Hello") != NULL, "title text");
        dinkc_vm_choice_pick(1);
    }
    {
        const char *talk2 =
            "void talk(void) { freeze(1); choice_start(); \"A\"; "
            "choice_end(); unfreeze(1); }";

        dinkc_vm_reset();
        memset(&pl, 0, sizeof(pl));
        dinkc_cmd_bind_player(&pl);
        slot = dinkc_vm_start_proc(talk2, strlen(talk2), 26, "talk");
        expect(dinkc_vm_waiting_choice() && dinkc_vm_live() == 1, "talk1");
        dinkc_vm_kill_sprite(26);
        expect(dinkc_vm_live() == 0, "killed sprite");
        slot = dinkc_vm_start_proc(talk2, strlen(talk2), 26, "talk");
        expect(dinkc_vm_live() == 1, "talk2 one fiber");
        dinkc_vm_choice_pick(1);
        expect(dinkc_vm_live() == 0 && pl.freeze == 0, "talk2 done");
        (void)slot;
    }
    {
        const char *none =
            "void main(void) { choice_start(); (&story == 1) \"hid\"; "
            "choice_end(); }";

        dinkc_vm_reset();
        slot = dinkc_vm_start(none, strlen(none), 1);
        expect(slot > 0 && !dinkc_vm_waiting_choice(), "empty menu");
        expect(dinkc_vm_live() == 0, "empty done");
    }
    {
        const char *wave =
            "void main(void) { add_exp(5, 1); hurt(1, 3); "
            "sp_hitpoints(1, 9); stop(); say_stop(\"no\", 1); }";

        dinkc_vm_reset();
        slot = dinkc_vm_start(wave, strlen(wave), 1);
        expect(dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1) == 5, "exp");
        expect(dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1) == 7, "hurt");
        expect(dinkc_vm_live() == 0, "stop kills");
        (void)slot;
    }
    {
        const char *w3 =
            "void main(void) { "
            "playmidi(\"1003.mid\"); draw_status(); "
            "add_item(\"item-fst\", 438, 1); "
            "&cur_weapon = 1; "
            "compare_weapon(\"item-fst\"); }";
        const char *ed =
            "void main(void) { sp_editor_num(26); }";

        dinkc_vm_reset();
        slot = dinkc_vm_start(w3, strlen(w3), 1);
        expect(dinkc_var_get("&return", DINKC_GLOBAL_SCOPE, 1) == 1, "compare");
        expect(dinkc_vm_live() == 0, "w3 done");
        dinkc_vm_reset();
        slot = dinkc_vm_start(ed, strlen(ed), 1);
        expect(dinkc_var_get("&return", DINKC_GLOBAL_SCOPE, 1) == 26, "editor");
        (void)slot;
    }
    {
        int impl = dinkc_cmd_implemented_count();
        int miss0 = dinkc_cmd_missing_count();
        int yld = 0, rv = 0, args[2] = {0, 0};

        expect(impl >= 60, "table size");
        expect(dinkc_cmd("no_such_fn_zz", args, 0, "", "", &yld, &rv) == 0,
               "unknown");
        expect(dinkc_cmd_missing_count() == miss0 + 1, "miss logged");
        expect(dinkc_cmd("draw_status", args, 0, "", "", &yld, &rv) == 1, "known");
        dinkc_cmd_bind_sprite_change(stub_sp_change);
        args[0] = 7;
        args[1] = 9;
        expect(dinkc_cmd("sp_brain", args, 2, "", "", &yld, &rv) == 1 && rv == 9,
               "sp_brain writes");
        {
            int box[8] = {250, 200, 200, 180, 400, 306, 0, 0};

            expect(dinkc_cmd("inside_box", box, 6, "", "", &yld, &rv) == 1 &&
                       rv == 1,
                   "inside_box hit");
            box[0] = 10;
            expect(dinkc_cmd("inside_box", box, 6, "", "", &yld, &rv) == 1 &&
                       rv == 0,
                   "inside_box miss");
        }
        dinkc_cmd_dump();
    }
    {
        const char *mv =
            "void main(void) { move_stop(5, 6, 400, 1); kill_this_task(); }";
        const char *cr =
            "void main(void) { create_sprite(10, 20, 0, 32, 1); }";

        dinkc_cmd_bind_sprite_change(stub_sp_change);
        dinkc_cmd_bind_create(stub_create);
        dinkc_cmd_bind_move(stub_move);
        dinkc_cmd_bind_moving(stub_moving);
        g_move_on = 0;
        dinkc_vm_reset();
        dinkc_cmd_bind_move(stub_move);
        dinkc_cmd_bind_moving(stub_moving);
        slot = dinkc_vm_start(mv, strlen(mv), 1);
        expect(slot > 0 && dinkc_vm_state(slot) == DINKC_WAIT_MOVE, "move_stop yield");
        dinkc_vm_tick(16);
        expect(dinkc_vm_state(slot) == DINKC_WAIT_MOVE, "not done in 16ms");
        dinkc_vm_resume_move();
        expect(dinkc_vm_state(slot) == DINKC_WAIT_MOVE, "still busy");
        g_move_on = 0;
        dinkc_vm_resume_move();
        expect(dinkc_vm_live() == 0, "move_stop resumes when dest reached");
        dinkc_vm_reset();
        dinkc_cmd_bind_create(stub_create);
        dinkc_cmd_bind_sprite_change(stub_sp_change);
        slot = dinkc_vm_start(cr, strlen(cr), 1);
        expect(dinkc_var_get("&return", DINKC_GLOBAL_SCOPE, 1) == 5,
               "create_sprite slot");
        dinkc_vm_reset();
        dinkc_cmd_bind_sprite_change(stub_sp_change);
        slot = dinkc_vm_start(
            "void main(void) { sp_x(5, 77); }",
            strlen("void main(void) { sp_x(5, 77); }"), 1);
        expect(g_stub_x[5] == 77, "sp_x npc write");
    }
    {
        /* BAR-SH hit: external("make","sheart") then sp_hard/draw_hard.
         * Child is already done; HIT must not stay WAIT_EXT. */
        const char *hit =
            "void main(void) { external(\"make\", \"sheart\"); "
            "sp_brain(4, 5); }";
        const char *child = "void sheart(void) { }";

        dinkc_vm_reset();
        memset(g_stub_brain, 0, sizeof(g_stub_brain));
        dinkc_cmd_bind_sprite_change(stub_sp_change);
        dinkc_cmd_bind_external(stub_bar_sh_external);
        g_ext_src = child;
        slot = dinkc_vm_start(hit, strlen(hit), 4);
        expect(slot > 0, "bar-sh hit start");
        expect(g_stub_brain[4] == 5, "bar-sh hit continues after external");
        expect(dinkc_vm_live() == 0, "bar-sh hit done");
        g_ext_src = NULL;
        dinkc_cmd_bind_external(NULL);
    }
    {
        /* s1-h2-o talk: extra } before unfreeze. FreeDink clamps level. */
        const char *ethel =
            "void talk(void) { freeze(1); if (1) { say_stop(\"thanks\", 1); "
            "} } unfreeze(1); }";
        const char *keep_ethel =
            "void main(void) { } void talk(void) { freeze(1); "
            "say_stop(\"thanks\", 1); } unfreeze(1); }";
        int ks;

        dinkc_vm_reset();
        memset(&pl, 0, sizeof(pl));
        dinkc_cmd_bind_player(&pl);
        slot = dinkc_vm_start_proc(ethel, strlen(ethel), 23, "talk");
        expect(pl.freeze == 1 && dinkc_vm_waiting_say(), "ethel say");
        dinkc_vm_advance_say();
        expect(dinkc_vm_live() == 0 && pl.freeze == 0, "ethel extra } unfreeze");

        dinkc_vm_reset();
        memset(&pl, 0, sizeof(pl));
        dinkc_cmd_bind_player(&pl);
        ks = dinkc_vm_start_keep(keep_ethel, strlen(keep_ethel), 23, "main");
        expect(ks > 0 && dinkc_vm_used(ks), "keep main");
        expect(dinkc_vm_locate(ks, "talk") == ks, "locate talk");
        expect(dinkc_vm_waiting_say(), "keep say");
        dinkc_vm_advance_say();
        expect(pl.freeze == 0, "keep extra } unfreeze");
        expect(dinkc_vm_used(ks), "keep still attached");
    }

    {
        const char *bounded =
            "void main(void) {\n"
            "loop:\n"
            "&gold += 1;\n"
            "if (&gold < 3)\n"
            "goto loop;\n"
            "}\n";
        const char *fwd =
            "void main(void) { goto there; &gold = 1; there: }\n";
        const char *miss =
            "void main(void) { goto nosuch; &gold = 1; }\n";
        const char *waitloop =
            "void main(void) {\n"
            "loop:\n"
            "&gold += 1;\n"
            "wait(1);\n"
            "goto loop;\n"
            "}\n";
        const char *shop =
            "void main(void) {\n"
            "mainloop:\n"
            "&gold += 1;\n"
            "wait(1);\n"
            "goto mainloop;\n"
            "}\n"
            "void hit(void) { goto mainloop; }\n";
        int i;

        dinkc_vm_reset();
        slot = dinkc_vm_start(bounded, strlen(bounded), 1);
        expect(dinkc_vm_live() == 0, "bounded goto done");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 3,
               "goto loop three times");
        (void)slot;

        dinkc_vm_reset();
        slot = dinkc_vm_start(fwd, strlen(fwd), 1);
        expect(dinkc_vm_live() == 0, "fwd goto done");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 0,
               "goto skips assign");

        dinkc_vm_reset();
        slot = dinkc_vm_start(miss, strlen(miss), 1);
        expect(dinkc_vm_live() == 0, "miss goto done");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 0,
               "missing label is EOF not fallthrough");

        dinkc_vm_reset();
        slot = dinkc_vm_start(waitloop, strlen(waitloop), 1);
        expect(dinkc_vm_state(slot) == DINKC_WAIT_MS, "goto wait yield");
        for (i = 0; i < 6; i++) {
            dinkc_vm_tick(i * DINKC_TICK_MS);
        }
        expect(dinkc_vm_live() == 1, "goto wait loop does not fall through");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) >= 2,
               "goto wait loop ran");

        dinkc_vm_reset();
        slot = dinkc_vm_start_proc(shop, strlen(shop), 4, "hit");
        expect(dinkc_vm_state(slot) == DINKC_WAIT_MS, "hit goto mainloop");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 1,
               "file-global label");
        {
            const char *saycol =
                "void main(void) { say_stop(\"bonus: 5\", 1); &gold = 1; }\n";
            const char *fold =
                "void main(void) {\n"
                "loop:\n"
                "&gold += 1;\n"
                "if (&gold < 3)\n"
                "GOTO LOOP;\n"
                "}\n";
            const char *empty =
                "void main(void) { goto; &gold = 1; }\n";
            const char *duck =
                "void main(void) {\n"
                "if (0) {\n"
                "duck:\n"
                "&gold = 2;\n"
                "return;\n"
                "}\n"
                "goto duck;\n"
                "&gold = 1;\n"
                "}\n";

            dinkc_vm_reset();
            slot = dinkc_vm_start(saycol, strlen(saycol), 1);
            expect(dinkc_vm_waiting_say(), "say colon not a label");
            dinkc_vm_advance_say();
            expect(dinkc_vm_live() == 0 &&
                       dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 1,
                   "after bonus: say");

            dinkc_vm_reset();
            slot = dinkc_vm_start(fold, strlen(fold), 1);
            expect(dinkc_vm_live() == 0, "fold goto done");
            expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 3,
                   "GOTO LOOP case fold");

            dinkc_vm_reset();
            slot = dinkc_vm_start(empty, strlen(empty), 1);
            expect(dinkc_vm_live() == 0, "empty goto done");
            expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 0,
                   "empty goto is EOF");

            dinkc_vm_reset();
            slot = dinkc_vm_start(duck, strlen(duck), 1);
            expect(dinkc_vm_live() == 0, "duck goto done");
            expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 2,
                   "goto into skipped if duck:");
        }
    }

    {
        const char *parent =
            "void main(void) { spawn(\"kid\"); &gold = 1; }\n";
        const char *kid =
            "void main(void) { wait(1); &gold = 2; }\n";
        const char *miss =
            "void main(void) { &gold = spawn(\"nope\"); }\n";
        int i, kidslot;

        g_spawn_src = kid;
        dinkc_cmd_bind_spawn(stub_spawn);
        dinkc_vm_reset();
        slot = dinkc_vm_start(parent, strlen(parent), 1);
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 1,
               "spawn parent continues");
        expect(!dinkc_vm_used(slot), "parent done");
        kidslot = dinkc_vm_sprite_fiber(1000);
        expect(kidslot > 0 && dinkc_vm_state(kidslot) == DINKC_WAIT_MS,
               "spawn child wait");
        for (i = 1; i <= 4; i++) {
            dinkc_vm_tick(1000 + i * DINKC_TICK_MS);
        }
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 2,
               "spawn child ran");
        expect(dinkc_vm_used(kidslot), "spawn keep after MAIN wait");

        dinkc_vm_reset();
        slot = dinkc_vm_start(parent, strlen(parent), 1);
        kidslot = dinkc_vm_sprite_fiber(1000);
        expect(kidslot > 0, "spawn keep");
        dinkc_vm_kill_all();
        expect(dinkc_vm_used(kidslot), "spawn 1000 survives swap");

        dinkc_cmd_bind_spawn(NULL);
        dinkc_vm_reset();
        slot = dinkc_vm_start(miss, strlen(miss), 1);
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 0,
               "spawn miss 0");
        expect(dinkc_vm_live() == 0, "miss spawn done");
        (void)slot;
        g_spawn_src = NULL;
    }

    {
        const char *loader =
            "void main(void) { &player_map = 131; load_screen(999); &gold = 1; }\n";
        const char *waiter =
            "void main(void) { wait(500); &gold = 9; }\n";
        const char *hole =
            "void main(void) { draw_screen(); &gold = 2; }\n";
        const char *letter =
            "void main(void) { script_attach(1000); draw_screen(); &gold = 3; }\n";
        const char *keep1000 =
            "void main(void) { script_attach(1000); wait(1); }\n";
        int waitslot, holeslot, letterslot, keepslot;

        g_nload = 0;
        g_ndraw = 0;
        g_load_map = 0;
        g_draw_spr = 0;
        dinkc_cmd_bind_load_screen(stub_load_screen);
        dinkc_cmd_bind_draw_screen(stub_draw_screen);

        dinkc_vm_reset();
        waitslot = dinkc_vm_start(waiter, strlen(waiter), 4);
        expect(dinkc_vm_state(waitslot) == DINKC_WAIT_MS, "waiter");
        slot = dinkc_vm_start(loader, strlen(loader), 1);
        expect(g_nload == 1 && g_load_map == 131, "load uses &player_map");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 1,
               "load parent continues");
        expect(!dinkc_vm_used(slot), "loader done");
        expect(dinkc_vm_used(waitslot), "load_screen does not kill_all");

        dinkc_vm_reset();
        holeslot = dinkc_vm_start(hole, strlen(hole), 4);
        expect(g_ndraw == 1 && g_draw_spr == 4, "draw hole sprite");
        expect(!dinkc_vm_used(holeslot), "draw_screen kills non-1000");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) != 2,
               "nothing after draw_screen on hole");

        dinkc_vm_reset();
        letterslot = dinkc_vm_start(letter, strlen(letter), 7);
        expect(g_draw_spr == 1000, "draw after script_attach");
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 3,
               "letter continues after draw");
        expect(!dinkc_vm_used(letterslot), "letter MAIN finished");

        dinkc_vm_reset();
        keepslot = dinkc_vm_start(keep1000, strlen(keep1000), 8);
        expect(dinkc_vm_state(keepslot) == DINKC_WAIT_MS, "attached wait");
        dinkc_vm_kill_all();
        expect(dinkc_vm_used(keepslot), "attach 1000 survives kill_all");

        dinkc_cmd_bind_load_screen(NULL);
        dinkc_cmd_bind_draw_screen(NULL);
        (void)waitslot;
    }

    {
        const char *lock =
            "void main(void) {\n"
            "  screenlock(1);\n"
            "  &gold = screenlock(-1);\n"
            "  screenlock(2);\n"
            "  &exp = screenlock();\n"
            "  screenlock(0);\n"
            "  &strength = screenlock(-1);\n"
            "}\n";
        int yld = 0, rv = 0, args[2] = {1, 0};

        hard_screenlock_set(0);
        dinkc_vm_reset();
        slot = dinkc_vm_start(lock, strlen(lock), 1);
        expect(dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1) == 1,
               "screenlock(1) then read");
        expect(dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1) == 1,
               "screenlock(2) does not set");
        expect(dinkc_var_get("&strength", DINKC_GLOBAL_SCOPE, 1) == 0,
               "screenlock(0)");
        expect(hard_screenlock_get() == 0, "cleared");
        expect(dinkc_cmd("screenlock", args, 1, "", "", &yld, &rv) == 1 &&
                   rv == 1,
               "cmd set 1");
        args[0] = -1;
        expect(dinkc_cmd("screenlock", args, 1, "", "", &yld, &rv) == 1 &&
                   rv == 1,
               "cmd read");
        args[0] = 0;
        (void)dinkc_cmd("screenlock", args, 1, "", "", &yld, &rv);
        expect(!dinkc_vm_used(slot), "lock script done");
    }

    printf("OK test_dinkc_vm\n");
    return 0;
}
