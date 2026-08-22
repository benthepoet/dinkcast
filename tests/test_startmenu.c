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
    expect(startmenu_tick(0, DINK_PAD_DOWN) == -1, "cycle2");
    expect(startmenu_focus() == STARTMENU_QUIT, "quit");
    expect(startmenu_tick(0, DINK_PAD_A) == STARTMENU_QUIT, "click quit");
    startmenu_reset();
    expect(startmenu_tick(0, DINK_PAD_A) == STARTMENU_NEW, "click new");

    startmenu_slot_reset();
    expect(startmenu_slot_focus() == 1, "slot1");
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
    expect(startpause_tick(0, DINK_PAD_START) == -1, "open");
    expect(startpause_open(), "opened");
    expect(startpause_focus() == STARTPAUSE_SAVE, "save");
    expect(startpause_tick(0, DINK_PAD_DOWN) == -1, "to title");
    expect(startpause_focus() == STARTPAUSE_TITLE, "title");
    expect(startpause_tick(0, DINK_PAD_A) == STARTPAUSE_TITLE, "pick title");
    expect(!startpause_open(), "closed after");
    printf("OK test_startmenu\n");
    return 0;
}
