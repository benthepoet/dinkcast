/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pad.h"

#include <stdio.h>
#include <stdlib.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    expect(pad_title_wants_leave(0, DINK_PAD_A) == 0, "missing pad + A stays");
    expect(pad_title_wants_leave(0, DINK_PAD_START) == 0, "missing pad + Start stays");
    expect(pad_title_wants_leave(0, DINK_PAD_LEAVE) == 0, "missing pad + both stays");
    expect(pad_title_wants_leave(1, 0) == 0, "present, no buttons stays");
    expect(pad_title_wants_leave(1, DINK_PAD_A) == 1, "A leaves");
    expect(pad_title_wants_leave(1, DINK_PAD_START) == 1, "Start leaves");
    expect(pad_title_wants_leave(1, DINK_PAD_A | DINK_PAD_START) == 1, "A+Start leaves");
    expect(pad_title_wants_leave(1, 1u << 4) == 0, "unrelated bit stays");
    expect(pad_dir_from_buttons(0) == 0, "no dir");
    expect(pad_dir_from_buttons(DINK_PAD_DOWN) == 2, "down");
    expect(pad_dir_from_buttons(DINK_PAD_LEFT) == 4, "left");
    expect(pad_just_pressed(0, DINK_PAD_A, DINK_PAD_A) == 1, "A down");
    expect(pad_just_pressed(DINK_PAD_A, DINK_PAD_A, DINK_PAD_A) == 0, "A hold");
    expect(pad_just_pressed(0, DINK_PAD_X, DINK_PAD_X) == 1, "X down");
    expect((DINK_PAD_X & DINK_PAD_A) == 0, "X not A");
    expect((DINK_PAD_X & DINK_PAD_B) == 0, "X not B");
    expect((DINK_PAD_Y & DINK_PAD_X) == 0, "Y not X");
    expect((DINK_PAD_L & DINK_PAD_Y) == 0, "L not Y");
    expect(pad_just_pressed(0, DINK_PAD_L, DINK_PAD_L) == 1, "L down");
    printf("OK test_pad\n");
    return 0;
}
