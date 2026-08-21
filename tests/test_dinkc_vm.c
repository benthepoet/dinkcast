/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_stub_brain[100];
static int g_stub_x[100];
static int g_move_on;

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

    printf("OK test_dinkc_vm\n");
    return 0;
}
