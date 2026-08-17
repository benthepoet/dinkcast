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

static int try_load(const char *name)
{
    char *buf = NULL;
    size_t n = 0;

    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    if (dinkc_load(name, &buf, &n) == 0) {
        dinkc_free(buf);
        return 0;
    }
    return -1;
}

int script_preload_screen(void)
{
    char seen[32][16];
    int nseen = 0, i, ok = 0;

    if (g_scr == NULL) {
        return 0;
    }
    if (g_scr->script[0] != '\0' && try_load(g_scr->script) == 0) {
        ok++;
    }
    for (i = 1; i <= 99; i++) {
        const char *nm = g_scr->sprite[i].script;
        int j, dup = 0;

        if (!editor_sprite_on_vision(&g_scr->sprite[i], DINK_VISION_DEFAULT)) {
            continue;
        }
        if (nm[0] == '\0') {
            continue;
        }
        for (j = 0; j < nseen; j++) {
            if (strcmp(seen[j], nm) == 0) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            continue;
        }
        if (nseen < 32) {
            strncpy(seen[nseen], nm, sizeof(seen[0]) - 1);
            seen[nseen][sizeof(seen[0]) - 1] = '\0';
            nseen++;
        }
        if (try_load(nm) == 0) {
            ok++;
        }
    }
    printf("dinkc preload ok=%d unique=%d\n", ok, nseen);
    return ok;
}

void script_on_main(int script_id)
{
    const char *name = (g_scr != NULL) ? g_scr->script : "";

    snprintf(g_log, sizeof(g_log), "main script_id=%d script=%s", script_id,
             name);
    printf("%s\n", g_log);
    (void)script_preload_screen();
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
