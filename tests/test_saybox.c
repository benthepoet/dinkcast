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
    expect(saybox_x() == 200 - DINK_SAY_XOFF, "xoff");
    expect(saybox_y() == 180 - DINK_SAY_YOFF, "yoff");
    saybox_set("Mother, how do I feed the pigs?  I forgot!", 1);
    expect(strchr(saybox_text(), '\n') != NULL, "wrap");
    saybox_set("hi", 0);
    expect(saybox_x() == 334 - DINK_SAY_XOFF, "sprite 0 is Dink");
    pl.x = 400;
    pl.y = 200;
    expect(saybox_x() == 400 - DINK_SAY_XOFF, "text_brain follows Dink");
    expect(saybox_y() == 200 - DINK_SAY_YOFF, "text_brain y");
    saybox_set("`#YES, NOW.", 26);
    scr.sprite[26].x = 250;
    scr.sprite[26].y = 220;
    expect(saybox_x() == 250 - DINK_SAY_XOFF, "follows editor sprite");
    expect(saybox_y() == 220 - DINK_SAY_YOFF, "follows editor y");
    saybox_set("narrator", 1000);
    expect(saybox_x() == 100, "narrator x");
    pl.x = 500;
    expect(saybox_x() == 100, "narrator does not follow");
    saybox_clear();
    expect(!saybox_active(), "off");
    printf("OK test_saybox\n");
    return 0;
}
