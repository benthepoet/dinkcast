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
    {
        const char *t = saybox_text();
        const char *nl = strchr(t, '\n');
        int n0, n1, box = 334 - DINK_SAY_XOFF;

        expect(nl != NULL, "wrap nl");
        n0 = (int)(nl - t);
        n1 = 0;
        while (nl[1 + n1] != '\0' && nl[1 + n1] != '\n') {
            n1++;
        }
        expect(n0 != n1 && n0 > 0 && n1 > 0, "wrap two lengths");
        expect(saybox_line_x(t) == box + DINK_SAY_BOX_W / 2 -
                                       (n0 * DINK_FONT_CELL) / 2,
               "hcenter wrap line 0");
        expect(saybox_line_x(nl + 1) == box + DINK_SAY_BOX_W / 2 -
                                            (n1 * DINK_FONT_CELL) / 2,
               "hcenter wrap line 1");
    }
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
    pl.x = 580;
    expect(saybox_x() == 470, "right-edge box 470");
    expect(saybox_line_x("hi") == 470 + DINK_SAY_BOX_W / 2 -
                                     (2 * DINK_FONT_CELL) / 2,
           "hcenter in shifted box");
    pl.x = 50;
    pl.y = 50;
    expect(saybox_x() == 1, "text_brain clamp x");
    expect(saybox_y() == 1, "text_brain clamp y");
    expect(saybox_line_x("hi") == 1 + DINK_SAY_BOX_W / 2 -
                                     (2 * DINK_FONT_CELL) / 2,
           "hcenter after clamp");
    pl.x = 400;
    pl.y = 200;
    saybox_set("`#YES, NOW.", 26);
    scr.sprite[26].x = 250;
    scr.sprite[26].y = 220;
    expect(saybox_x() == 250 - DINK_SAY_XOFF, "follows editor sprite");
    expect(saybox_y() == 220 - DINK_SAY_YOFF, "follows editor y");
    saybox_set("narrator", 1000);
    expect(saybox_x() == 100, "narrator x");
    expect(saybox_line_x("narrator") ==
               100 + (DINK_SAY_PLAYX - 20) / 2 -
                   (8 * DINK_FONT_CELL) / 2,
           "narrator hcenter wide box");
    pl.x = 500;
    expect(saybox_x() == 100, "narrator does not follow");
    expect(saybox_line_x("narrator") ==
               100 + (DINK_SAY_PLAYX - 20) / 2 -
                   (8 * DINK_FONT_CELL) / 2,
           "narrator line stays");
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
    /* FreeDink add_text_sprite: strlen*TEXT_TIMER (77), floor TEXT_MIN 2700.
     * live_sprite_set_kill_start on first tick; expire when start+ttl < now. */
    saybox_set("Hi", 1);
    expect(saybox_tick(0) == 0 && saybox_active(), "kill_start first tick");
    expect(saybox_tick(2700) == 0 && saybox_active(), "TEXT_MIN not yet");
    expect(saybox_tick(2701) == 1 && !saybox_active(), "TEXT_MIN expire");
    {
        char long_say[41];

        memset(long_say, 'a', 40);
        long_say[40] = '\0';
        saybox_set(long_say, 1);
        expect(saybox_tick(0) == 0 && saybox_active(), "long start");
        expect(saybox_tick(40 * DINK_SAY_TEXT_TIMER) == 0 && saybox_active(),
               "long not yet");
        expect(saybox_tick(40 * DINK_SAY_TEXT_TIMER + 1) == 1 &&
                   !saybox_active(),
               "long expire");
    }
    saybox_set("`$I'm no wizard!", 1);
    expect(saybox_tick(0) == 0 && saybox_active(), "wizard start");
    expect(saybox_tick(DINK_SAY_TEXT_MIN + 1) == 1 && !saybox_active(),
           "wizard TEXT_MIN");
    /* start-2.c click() empty slot. */
    saybox_set_xy("`%Try loading a saved game that exists, friend.", 0, 390);
    expect(saybox_active(), "xy on");
    expect(saybox_color() == 15, "xy `% is 15");
    expect(strcmp(saybox_text(),
                  "Try loading a saved game that exists, friend.") == 0,
           "xy strip");
    expect(saybox_x() == 0 && saybox_y() == 390, "xy 0,390");
    pl.x = 500;
    expect(saybox_x() == 0 && saybox_y() == 390, "xy does not follow");
    saybox_set("`3Why hello, Dink.", 1);
    expect(saybox_tick(100) == 0 && saybox_active(), "ethel start");
    expect(saybox_tick(100 + DINK_SAY_TEXT_MIN) == 0 && saybox_active(),
           "ethel not yet");
    expect(saybox_tick(100 + DINK_SAY_TEXT_MIN + 1) == 1 && !saybox_active(),
           "ethel expire");
    printf("OK test_saybox\n");
    return 0;
}
