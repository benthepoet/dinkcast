/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pad.h"
#include "startmenu.h"

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
    startmenu_reset();
    expect(startmenu_focus() == STARTMENU_NEW, "new");
    expect(strcmp(startmenu_script(STARTMENU_NEW), "start-1") == 0, "s1");
    expect(strcmp(startmenu_script(STARTMENU_LOAD), "start-2") == 0, "s2");
    expect(strcmp(startmenu_script(STARTMENU_QUIT), "start-4") == 0, "s4");
    expect(startmenu_tick(0, DINK_PAD_DOWN) == -1, "cycle");
    expect(startmenu_focus() == STARTMENU_LOAD, "load");
    expect(startmenu_tick(0, DINK_PAD_DOWN) == -1, "cycle wrap");
    expect(startmenu_focus() == STARTMENU_NEW, "wrap new");
    expect(startmenu_tick(0, DINK_PAD_A) == STARTMENU_NEW, "click new");
    startmenu_reset();
    expect(startmenu_tick(0, DINK_PAD_A) == STARTMENU_NEW, "click new again");
    {
        int cx = 0, cy = 0;

        /* but7-01 115x29 cx 77 cy 22 → but7-02 135x51 glow. */
        startmenu_highlight_center(115, 29, 77, 22, 135, 51, &cx, &cy);
        expect(cx == 87 && cy == 33, "highlight center");
    }

    /* start-1.c buttonon/off: seq 199 plays then reverses (brain 7). */
    startmenu_hover_bind(STARTMENU_NEW, STARTMENU_HOVER_NEW,
                         STARTMENU_HOVER_NEW_X, STARTMENU_HOVER_NEW_Y, 9, 50);
    startmenu_hover_on(STARTMENU_NEW);
    startmenu_hover_tick(0);
    expect(startmenu_hover_live(STARTMENU_NEW) &&
               startmenu_hover_pframe(STARTMENU_NEW) == 1,
           "hover start fr1");
    startmenu_hover_tick(51);
    expect(startmenu_hover_pframe(STARTMENU_NEW) == 2, "hover move");
    {
        int t, want;

        for (want = 3, t = 102; want <= 9; want++, t += 51) {
            startmenu_hover_tick(t);
            expect(startmenu_hover_pframe(STARTMENU_NEW) == want, "hover to last");
        }
        startmenu_hover_tick(t);
        expect(startmenu_hover_pframe(STARTMENU_NEW) == 9 &&
                   startmenu_hover_live(STARTMENU_NEW),
               "hover hold last");
        startmenu_hover_tick(t + 51);
        expect(startmenu_hover_pframe(STARTMENU_NEW) == 9 &&
                   startmenu_hover_live(STARTMENU_NEW),
               "hover stay last");
    }
    startmenu_hover_off(STARTMENU_NEW);
    startmenu_hover_tick(500);
    expect(startmenu_hover_pframe(STARTMENU_NEW) == 9, "reverse from last");
    startmenu_hover_tick(551);
    expect(startmenu_hover_pframe(STARTMENU_NEW) == 8, "reverse step");
    {
        int t;

        for (t = 602; t <= 1100; t += 51) {
            startmenu_hover_tick(t);
        }
    }
    expect(!startmenu_hover_live(STARTMENU_NEW), "brain 7 gone");

    startmenu_slot_reset();
    expect(startmenu_slot_focus() == 1, "slot1");
    expect(startmenu_slot_tick(DINK_PAD_A, DINK_PAD_A) == -1,
           "held continue a");
    expect(startmenu_slot_tick(DINK_PAD_A, 0) == -1, "release a");
    expect(startmenu_slot_tick(0, DINK_PAD_A) == 1, "confirm slot1");
    startmenu_slot_reset();
    expect(startmenu_slot_focus() == 1, "slot1 again");
    expect(startmenu_slot_tick(0, DINK_PAD_DOWN) == -1, "slot cycle");
    expect(startmenu_slot_focus() == 2, "slot2");
    expect(startmenu_slot_tick(0, DINK_PAD_A) == 2, "pick 2");
    startmenu_slot_reset();
    expect(startmenu_slot_tick(0, DINK_PAD_UP) == -1, "wrap");
    expect(startmenu_slot_focus() == 0, "nevermind up");
    expect(startmenu_slot_tick(0, DINK_PAD_UP) == -1, "to slot10");
    expect(startmenu_slot_focus() == 10, "slot10");
    expect(startmenu_slot_tick(0, DINK_PAD_DOWN) == -1, "to nevermind");
    expect(startmenu_slot_focus() == 0, "nevermind");
    expect(startmenu_slot_tick(0, DINK_PAD_A) == 0, "pick nevermind");

    startpause_reset();
    expect(!startpause_open(), "closed");
    expect(!startpause_eats_pad(0, DINK_PAD_A), "a free");
    expect(!startpause_eats_pad(0, DINK_PAD_Y), "y free");
    expect(!startpause_eats_pad(0, DINK_PAD_B), "b free");
    expect(startpause_eats_pad(0, DINK_PAD_START), "start eats");
    expect(startpause_tick(0, DINK_PAD_START) == -1, "open");
    expect(startpause_open(), "opened");
    expect(startpause_eats_pad(0, DINK_PAD_A), "open eats a");
    expect(startpause_focus() == STARTPAUSE_CONTINUE, "continue");
    expect(startpause_tick(0, DINK_PAD_DOWN) == -1, "to title");
    expect(startpause_focus() == STARTPAUSE_TITLE, "title");
    expect(startpause_tick(0, DINK_PAD_A) == STARTPAUSE_TITLE, "pick title");
    expect(!startpause_open(), "closed after");
    printf("OK test_startmenu\n");
    return 0;
}
