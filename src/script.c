/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "script.h"

#include "dinkc_cmd.h"
#include "dinkc_file.h"
#include "dinkc_lex.h"
#include "dinkc_parse.h"
#include "dinkc_vm.h"

#include <stdio.h>
#include <stdlib.h>
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
        int ntok = dinkc_lex_count(buf, n);
        struct DinkcProg pr;
        char perr[96];

        printf("dinkc lex %s ntok=%d\n", name, ntok);
        if (dinkc_parse(buf, n, &pr, perr, sizeof(perr)) != 0) {
            printf("dinkc parse %s fail %s\n", name, perr);
            dinkc_free(buf);
            return -1;
        }
        printf("dinkc parse %s procs=%d\n", name, pr.nproc);
        dinkc_free(buf);
        return ntok < 0 ? -1 : 0;
    }
    return -1;
}

static int start_main(const char *name, int sprite)
{
    char *buf = NULL;
    size_t n = 0;

    if (name == NULL || strlen(name) <= 1) {
        return -1;
    }
    if (dinkc_load(name, &buf, &n) != 0) {
        return -1;
    }
    if (dinkc_vm_start(buf, n, sprite) < 0) {
        printf("dinkc attach no main %s spr=%d\n", name, sprite);
        dinkc_free(buf);
        return -1;
    }
    printf("dinkc attach spr=%d script=%s\n", sprite, name);
    dinkc_free(buf);
    return 0;
}

int script_preload_screen(void)
{
    char seen[32][16];
    int nseen = 0, i, ok = 0;

    if (g_scr == NULL) {
        return 0;
    }
    if (g_scr->script[0] != '\0') {
        strncpy(seen[nseen], g_scr->script, sizeof(seen[0]) - 1);
        seen[nseen][sizeof(seen[0]) - 1] = '\0';
        nseen++;
        if (try_load(g_scr->script) == 0) {
            ok++;
        }
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
        if (nseen >= 32) {
            continue;
        }
        strncpy(seen[nseen], nm, sizeof(seen[0]) - 1);
        seen[nseen][sizeof(seen[0]) - 1] = '\0';
        nseen++;
        if (try_load(nm) == 0) {
            ok++;
        }
    }
    printf("dinkc preload ok=%d unique=%d\n", ok, nseen);
    return ok;
}

int script_attach_screen(void)
{
    int rank[100];
    int nrank = 0, i, a, nstart = 0;

    if (g_scr == NULL) {
        return 0;
    }
    (void)script_preload_screen();
    /* draw_screen_game: screen script MAIN before game_place_sprites. */
    if (start_main(g_scr->script, 0) == 0) {
        nstart++;
    }
    /* game_place_sprites: type 1 + vision + strlen(script) > 1, rank order. */
    for (i = 1; i <= 99; i++) {
        if (!editor_sprite_on_vision(&g_scr->sprite[i], DINK_VISION_DEFAULT)) {
            continue;
        }
        if (g_scr->sprite[i].type != 1) {
            continue;
        }
        if (strlen(g_scr->sprite[i].script) <= 1) {
            continue;
        }
        rank[nrank++] = i;
    }
    for (a = 1; a < nrank; a++) {
        int t = rank[a];
        int ty = editor_sprite_rank_y(&g_scr->sprite[t]);
        int b = a;

        while (b > 0 &&
               editor_sprite_rank_y(&g_scr->sprite[rank[b - 1]]) > ty) {
            rank[b] = rank[b - 1];
            b--;
        }
        rank[b] = t;
    }
    for (i = 0; i < nrank; i++) {
        if (start_main(g_scr->sprite[rank[i]].script, rank[i]) == 0) {
            nstart++;
        }
    }
    printf("dinkc attach n=%d\n", nstart);
    return nstart;
}

void script_on_main(int script_id)
{
    const char *name = (g_scr != NULL) ? g_scr->script : "";
    int n;

    n = script_attach_screen();
    snprintf(g_log, sizeof(g_log), "main script_id=%d script=%s attach=%d",
             script_id, name, n);
    printf("%s\n", g_log);
    if (getenv("DINKC_DUMP_FNS") != NULL &&
        getenv("DINKC_DUMP_FNS")[0] != '\0' &&
        getenv("DINKC_DUMP_FNS")[0] != '0') {
        dinkc_cmd_dump();
    }
}

void script_on_talk(int sprite)
{
    char *buf = NULL;
    size_t n = 0;
    const char *nm = slot_script(sprite);

    snprintf(g_log, sizeof(g_log), "talk sprite=%d script=%s", sprite, nm);
    printf("%s\n", g_log);
    if (nm[0] == '\0') {
        return;
    }
    if (dinkc_load(nm, &buf, &n) != 0) {
        return;
    }
    if (dinkc_vm_start_proc(buf, n, sprite, "talk") < 0) {
        printf("dinkc talk no proc %s\n", nm);
    }
    dinkc_free(buf);
}

void script_on_hit(int sprite)
{
    snprintf(g_log, sizeof(g_log), "hit sprite=%d script=%s", sprite,
             slot_script(sprite));
    printf("%s\n", g_log);
    try_load(slot_script(sprite));
}
