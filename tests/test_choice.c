/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "choice.h"
#include "font.h"
#include "fs.h"
#include "ini.h"

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
    struct ChoiceLayout lay;
    int h[21];
    int i;

    expect(font_init() == 0, "font");
    expect(choice_center_x("Leave") == DINK_CHOICE_BOX_L +
                                          (DINK_CHOICE_BOX_R - DINK_CHOICE_BOX_L) /
                                              2 -
                                          (5 * DINK_FONT_CELL) / 2,
           "hcenter Leave");
    expect(choice_wrap_height("Leave") == DINK_FONT_CELL, "one line h");
    memset(h, 0, sizeof(h));
    for (i = 1; i <= 6; i++) {
        h[i] = DINK_FONT_CELL;
    }
    /* game_choice_logic: measure i < last, then center. */
    choice_layout(6, 1, h, DINK_CHOICE_NEWY_NONE, &lay);
    expect(lay.view_lo == 1 && lay.view_hi == 6, "one page");
    expect(lay.choices_y == DINK_CHOICE_Y0 +
                                ((DINK_CHOICE_X_DEPTH - (DINK_CHOICE_Y0 + 5 * DINK_FONT_CELL)) /
                                 2) -
                                20,
           "vertical center");
    for (i = 1; i <= 10; i++) {
        h[i] = 40;
    }
    choice_layout(10, 1, h, DINK_CHOICE_NEWY_NONE, &lay);
    expect(lay.view_lo == 1 && lay.view_hi == 6, "page 1");
    expect(lay.choices_y == DINK_CHOICE_Y0, "no center when paged");
    choice_layout(10, 7, h, DINK_CHOICE_NEWY_NONE, &lay);
    expect(lay.view_lo == 7 && lay.view_hi == 10, "page 2");
    choice_layout(3, 1, h, 240, &lay);
    expect(lay.choices_y != DINK_CHOICE_Y0, "set_y");

    if (dink_fs_init() == 0) {
        struct SeqInfo *seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
        struct SpriteFrame *fr;

        expect(seqs != NULL && ini_load(seqs, DINK_MAX_SEQ) == 0, "ini");
        expect(choice_load(seqs) == 0, "choice_load");
        fr = choice_frame(DINK_CHOICE_SEQ, 2);
        expect(fr != NULL && fr->w == 192 && fr->h == 331, "seq 30 fr 2");
        fr = choice_frame(DINK_CHOICE_SEQ, 3);
        expect(fr != NULL && fr->w == 200 && fr->h == 250, "seq 30 fr 3");
        fr = choice_frame(DINK_CHOICE_SEQ, 4);
        expect(fr != NULL && fr->w == 189 && fr->h == 330, "seq 30 fr 4");
        fr = choice_frame(DINK_CHOICE_AROWL, 1);
        expect(fr != NULL && fr->w > 0 && fr->h > 0, "arowl");
        fr = choice_frame(DINK_CHOICE_AROWR, 1);
        expect(fr != NULL && fr->w > 0 && fr->h > 0, "arowr");
        choice_tick(0);
        expect(choice_curf() == 1, "curf 1");
        choice_tick(100);
        expect(choice_curf() == 2, "curf 2");
        choice_free();
        free(seqs);
    }
    printf("OK test_choice\n");
    return 0;
}
