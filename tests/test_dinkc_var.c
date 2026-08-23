/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_var.h"
#include "dinkc_vm.h"

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
    const char *life =
        "void main(void) { if (&life == 10) { wait(1); } }";
    const char *skip =
        "void main(void) { if (&story > 2) { wait(1); } }";
    const char *loc =
        "void main(void) { int &x = 2; if (&x == 2) { wait(1); } }";
    int slot, i;

    dinkc_var_init();
    expect(dinkc_var_get("&life", 0, 1) == 10, "life");
    expect(dinkc_var_get("&strength", 0, 1) == 3, "strength");
    expect(dinkc_var_get("&player_map", 0, 1) == 1, "player_map");
    expect(dinkc_var_get("&s2-map", 0, 1) == 0, "s2-map");
    expect(dinkc_var_get("&nope", 0, 1) == 0, "unknown 0");
    expect(dinkc_var_get("&current_sprite", 3, 7) == 7, "engine sprite");
    {
        char out[64];

        dinkc_var_expand(out, sizeof(out), "life is &life", 0, 1);
        expect(strcmp(out, "life is 10") == 0, out);
    }

    dinkc_var_make("&gold", 5, 2);
    expect(dinkc_var_get("&gold", 2, 1) == 5, "local gold");
    expect(dinkc_var_get("&gold", 0, 1) == 0, "global gold still 0");
    dinkc_var_make_global("&life", 99);
    expect(dinkc_var_get("&life", 0, 1) == 10, "make_global no overwrite");
    dinkc_var_kill_scope(2);
    expect(dinkc_var_get("&gold", 2, 1) == 0, "local gold gone");

    dinkc_vm_reset();
    slot = dinkc_vm_start(life, strlen(life), 1);
    expect(dinkc_vm_state(slot) == DINKC_WAIT_MS, "if life");
    for (i = 0; i < 4 && dinkc_vm_live(); i++) {
        dinkc_vm_tick(i * DINKC_TICK_MS);
    }
    expect(dinkc_vm_live() == 0, "life wait done");

    dinkc_vm_reset();
    slot = dinkc_vm_start(skip, strlen(skip), 1);
    expect(dinkc_vm_live() == 0, "story>2 skipped");

    dinkc_vm_reset();
    slot = dinkc_vm_start(loc, strlen(loc), 1);
    expect(dinkc_vm_state(slot) == DINKC_WAIT_MS, "local if");
    for (i = 0; i < 4 && dinkc_vm_live(); i++) {
        dinkc_vm_tick(i * DINKC_TICK_MS);
    }
    expect(dinkc_vm_live() == 0, "local wait done");

    printf("OK test_dinkc_var\n");
    return 0;
}
