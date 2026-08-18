/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "player.h"

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
    dinkc_vm_advance_say();
    expect(dinkc_vm_live() == 0 && pl.freeze == 0, "unfreeze");

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

    printf("OK test_dinkc_vm\n");
    return 0;
}
