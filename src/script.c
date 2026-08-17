/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "script.h"

#include <stdio.h>
#include <string.h>

static const struct MapScreen *g_scr;
static char g_log[96];

void script_bind_screen(const struct MapScreen *scr)
{
    g_scr = scr;
}

const char *script_stub_log(void)
{
    return g_log;
}

static const char *slot_script(int sprite)
{
    if (g_scr == NULL || sprite < 1 || sprite > 99) {
        return "";
    }
    return g_scr->sprite[sprite].script;
}

void script_on_main(int script_id)
{
    const char *name = (g_scr != NULL) ? g_scr->script : "";

    snprintf(g_log, sizeof(g_log), "main script_id=%d script=%s", script_id,
             name);
    printf("%s\n", g_log);
}

void script_on_talk(int sprite)
{
    snprintf(g_log, sizeof(g_log), "talk sprite=%d script=%s", sprite,
             slot_script(sprite));
    printf("%s\n", g_log);
}

void script_on_hit(int sprite)
{
    snprintf(g_log, sizeof(g_log), "hit sprite=%d script=%s", sprite,
             slot_script(sprite));
    printf("%s\n", g_log);
}
