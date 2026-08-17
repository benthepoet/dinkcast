/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "script.h"

#include "dinkc_file.h"

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

static void try_load(const char *name)
{
    char *buf = NULL;
    size_t n = 0;

    if (name == NULL || name[0] == '\0') {
        return;
    }
    if (dinkc_load(name, &buf, &n) == 0) {
        dinkc_free(buf);
    }
}

void script_on_main(int script_id)
{
    const char *name = (g_scr != NULL) ? g_scr->script : "";

    snprintf(g_log, sizeof(g_log), "main script_id=%d script=%s", script_id,
             name);
    printf("%s\n", g_log);
    try_load(name);
}

void script_on_talk(int sprite)
{
    snprintf(g_log, sizeof(g_log), "talk sprite=%d script=%s", sprite,
             slot_script(sprite));
    printf("%s\n", g_log);
    try_load(slot_script(sprite));
}

void script_on_hit(int sprite)
{
    snprintf(g_log, sizeof(g_log), "hit sprite=%d script=%s", sprite,
             slot_script(sprite));
    printf("%s\n", g_log);
    try_load(slot_script(sprite));
}
