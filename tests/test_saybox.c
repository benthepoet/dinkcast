/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "font.h"
#include "player.h"
#include "saybox.h"

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

static int g_lx, g_ly;

static int stub_live_xy(int slot, int *x, int *y)
{
    if (slot != 7 || x == NULL || y == NULL) {
        return 0;
    }
    *x = g_lx;
    *y = g_ly;
    return 1;
}

int main(void)
{
    struct MapScreen scr;
    struct Player pl;

    memset(&scr, 0, sizeof(scr));
    memset(&pl, 0, sizeof(pl));
    pl.x = 334;
    pl.y = 161;
    scr.sprite[26].active = 1;
    scr.sprite[26].x = 200;
    scr.sprite[26].y = 180;
    expect(font_init() == 0, "font");
    saybox_bind(&scr, &pl);
    saybox_set("`#YES, NOW.", 26);
    expect(saybox_active(), "on");
    expect(strcmp(saybox_text(), "YES, NOW.") == 0, saybox_text());
    expect(saybox_color() == 13, "color 13");
    expect(saybox_argb(13) == 0xFFFF8484u, "argb 13 hot pink");
    expect(saybox_x() == 200 - DINK_SAY_XOFF, "xoff");
    expect(saybox_y() == 180 - DINK_SAY_YOFF, "yoff");
    saybox_set("Mother, how do I feed the pigs?  I forgot!", 1);
    expect(strchr(saybox_text(), '\n') != NULL, "wrap");
    saybox_set("hi", 0);
    expect(saybox_x() == 334 - DINK_SAY_XOFF, "sprite 0 is Dink");
    /* print_text_wrap hcenter: box left + 150/2 - line_w/2. "hi" is 16px. */
    expect(saybox_line_x("hi") == (334 - DINK_SAY_XOFF) + DINK_SAY_BOX_W / 2 -
                                     (2 * DINK_FONT_CELL) / 2,
           "hcenter hi over Dink");
    pl.x = 400;
    pl.y = 200;
    expect(saybox_x() == 400 - DINK_SAY_XOFF, "text_brain follows Dink");
    expect(saybox_y() == 200 - DINK_SAY_YOFF, "text_brain y");
    expect(saybox_line_x(saybox_text()) ==
               (400 - DINK_SAY_XOFF) + DINK_SAY_BOX_W / 2 -
                   (2 * DINK_FONT_CELL) / 2,
           "hcenter follows");
    saybox_set("`#YES, NOW.", 26);
    scr.sprite[26].x = 250;
    scr.sprite[26].y = 220;
    expect(saybox_x() == 250 - DINK_SAY_XOFF, "follows editor sprite");
    expect(saybox_y() == 220 - DINK_SAY_YOFF, "follows editor y");
    saybox_set("narrator", 1000);
    expect(saybox_x() == 100, "narrator x");
    pl.x = 500;
    expect(saybox_x() == 100, "narrator does not follow");
    saybox_bind_live_xy(stub_live_xy);
    g_lx = 300;
    g_ly = 160;
    saybox_set("`5La la laaa La La", 7);
    expect(strcmp(saybox_text(), "La la laaa La La") == 0, "strip `5");
    expect(saybox_color() == 5, "color 5 Chealse");
    expect(saybox_argb(5) == 0xFFDEADFFu, "argb 5 magenta");
    expect(saybox_x() == 300 - DINK_SAY_XOFF, "npc live x");
    g_lx = 410;
    g_ly = 190;
    expect(saybox_x() == 410 - DINK_SAY_XOFF, "follows live npc");
    expect(saybox_y() == 190 - DINK_SAY_YOFF, "follows live npc y");
    saybox_set("no prefix", 1);
    expect(saybox_color() == 14, "default yellow 14");
    expect(saybox_argb(14) == 0xFFFFFF02u, "argb 14 yellow");
    expect(saybox_argb(4) == 0xFFFF9C4Au, "argb 4 orange");
    expect(saybox_argb(15) == 0xFFFFFFFFu, "argb 15 white");
    saybox_set("`@extra", 1);
    expect(saybox_color() == 12, "1.08 @ is 12");
    expect(strcmp(saybox_text(), "extra") == 0, "strip @");
    saybox_set("`!bang", 1);
    expect(saybox_color() == 11, "1.08 ! is 11");
    expect(strcmp(saybox_text(), "bang") == 0, "strip !");
    saybox_clear();
    expect(!saybox_active(), "off");
    printf("OK test_saybox\n");
    return 0;
}
