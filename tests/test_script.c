/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mapscr.h"
#include "script.h"

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

    memset(&scr, 0, sizeof(scr));
    strncpy(scr.script, "s1-h1", sizeof(scr.script) - 1);
    strncpy(scr.sprite[3].script, "s1-h1-m", sizeof(scr.sprite[3].script) - 1);
    scr.sprite[3].active = 1;
    scr.sprite[3].type = 1;
    strncpy(scr.sprite[4].script, "s1-h1-m", sizeof(scr.sprite[4].script) - 1);
    scr.sprite[4].active = 1;
    scr.sprite[4].type = 1;

    script_bind_screen(&scr);
    script_on_main(0);
    expect(strcmp(script_stub_log(), "main script_id=0 script=s1-h1") == 0,
           script_stub_log());
    script_on_talk(3);
    expect(strcmp(script_stub_log(), "talk sprite=3 script=s1-h1-m") == 0,
           script_stub_log());
    script_on_hit(3);
    expect(strcmp(script_stub_log(), "hit sprite=3 script=s1-h1-m") == 0,
           script_stub_log());
    script_on_talk(0);
    expect(strcmp(script_stub_log(), "talk sprite=0 script=") == 0,
           script_stub_log());
    script_on_hit(100);
    expect(strcmp(script_stub_log(), "hit sprite=100 script=") == 0,
           script_stub_log());
    script_bind_screen(NULL);
    script_on_talk(3);
    expect(strcmp(script_stub_log(), "talk sprite=3 script=") == 0,
           "unbound");

    printf("OK test_script\n");
    return 0;
}
