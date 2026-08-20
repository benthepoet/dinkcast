/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "script.h"

#include "dinkc_cmd.h"
#include "dinkc_file.h"
#include "dinkc_lex.h"
#include "dinkc_parse.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct MapScreen *g_scr;
static void (*g_note_script)(int slot, const char *name);
static char g_log[96];
static int g_dink_dying;
static int start_main(const char *name, int sprite);
static int start_named(int sprite, const char *file, const char *proc);

static int bind_sp_script(int slot, const char *name)
{
    dinkc_vm_kill_sprite(slot);
    if (g_note_script != NULL) {
        g_note_script(slot, name);
    }
    return start_main(name, slot) == 0 ? 1 : 0;
}

static int bind_external(int sprite, const char *file, const char *proc,
                         const int *args, int nargs)
{
    char *buf = NULL;
    size_t n = 0;
    int slot;

    if (dinkc_load(file, &buf, &n) != 0) {
        printf("dinkc external miss %s\n", file != NULL ? file : "");
        return 0;
    }
    slot = dinkc_vm_start_proc(buf, n, sprite, proc);
    if (slot > 0 && args != NULL && nargs > 2) {
        dinkc_vm_set_args(slot, args + 2, nargs - 2);
    }
    dinkc_free(buf);
    return slot > 0 ? slot : 0;
}

void script_bind_screen(const struct MapScreen *scr)
{
    g_scr = scr;
    dinkc_cmd_bind_sp_script(bind_sp_script);
    dinkc_cmd_bind_external(bind_external);
    printf("script bind esz=%d spr26=%s type=%d\n",
           (int)sizeof(struct EditorSprite),
           (scr != NULL) ? scr->sprite[26].script : "",
           (scr != NULL) ? (int)scr->sprite[26].type : -1);
}

void script_bind_note_script(void (*fn)(int slot, const char *name))
{
    g_note_script = fn;
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

static int start_named(int sprite, const char *file, const char *proc)
{
    char *buf = NULL;
    size_t n = 0;

    if (file == NULL || file[0] == '\0' || proc == NULL) {
        return -1;
    }
    if (dinkc_load(file, &buf, &n) != 0) {
        return -1;
    }
    dinkc_vm_kill_sprite(sprite);
    if (dinkc_vm_start_proc(buf, n, sprite, proc) < 0) {
        printf("dinkc %s no proc %s\n", proc, file);
        dinkc_free(buf);
        return -1;
    }
    dinkc_free(buf);
    return 0;
}

int script_play_vision(void)
{
    return dinkc_var_get("&vision", DINKC_GLOBAL_SCOPE, 1);
}

int script_preload_screen(void)
{
    char seen[32][16];
    int nseen = 0, i, ok = 0, vis;

    if (g_scr == NULL) {
        return 0;
    }
    vis = script_play_vision();
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

        if (!editor_sprite_on_vision(&g_scr->sprite[i], vis)) {
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
    printf("dinkc preload ok=%d unique=%d vis=%d\n", ok, nseen, vis);
    if (nseen == 0 && g_scr != NULL) {
        printf("dinkc preload empty esz=%d spr26=%s act=%d vis=%d type=%d\n",
               (int)sizeof(struct EditorSprite), g_scr->sprite[26].script,
               (int)g_scr->sprite[26].active, (int)g_scr->sprite[26].vision,
               (int)g_scr->sprite[26].type);
    }
    return ok;
}

void script_enter_vision(void)
{
    /* draw_screen_game: *pvision = 0, then screen MAIN. */
    dinkc_var_set("&vision", 0, DINKC_GLOBAL_SCOPE, 1);
    if (g_scr == NULL) {
        return;
    }
    (void)script_preload_screen();
    if (start_main(g_scr->script, 0) == 0) {
        printf("dinkc screen main %s vis=%d\n", g_scr->script,
               script_play_vision());
    }
}

int script_attach_live(void)
{
    int rank[100];
    int nrank = 0, i, a, nstart = 0, vis;

    if (g_scr == NULL) {
        return 0;
    }
    vis = script_play_vision();
    (void)script_preload_screen();
    /* game_place_sprites: type 1 + *pvision + strlen(script) > 1. */
    for (i = 1; i <= 99; i++) {
        if (!editor_sprite_on_vision(&g_scr->sprite[i], vis)) {
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
    printf("dinkc attach n=%d vis=%d\n", nstart, vis);
    return nstart;
}

int script_attach_screen(void)
{
    script_enter_vision();
    return script_attach_live();
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
    dinkc_vm_kill_sprite(sprite);
    if (dinkc_vm_start_proc(buf, n, sprite, "talk") < 0) {
        printf("dinkc talk no proc %s\n", nm);
    }
    dinkc_free(buf);
}

void script_on_hit(int sprite)
{
    script_on_hit_from(sprite, 1);
}

void script_on_hit_from(int sprite, int attacker)
{
    const char *nm = slot_script(sprite);

    snprintf(g_log, sizeof(g_log), "hit sprite=%d script=%s", sprite, nm);
    printf("%s\n", g_log);
    if (attacker > 0) {
        dinkc_var_set("&enemy_sprite", attacker, DINKC_GLOBAL_SCOPE, 1);
        dinkc_var_set("&missle_source", attacker, DINKC_GLOBAL_SCOPE, 1);
    }
    (void)start_named(sprite, nm, "hit");
}

void script_on_kill(int sprite, const char *proc)
{
    const char *nm = slot_script(sprite);
    const char *p = proc != NULL ? proc : "die";

    snprintf(g_log, sizeof(g_log), "kill sprite=%d script=%s proc=%s", sprite,
             nm, p);
    printf("%s\n", g_log);
    (void)start_named(sprite, nm, p);
}

void script_on_push(int sprite)
{
    const char *nm = slot_script(sprite);

    snprintf(g_log, sizeof(g_log), "push sprite=%d script=%s", sprite, nm);
    printf("%s\n", g_log);
    (void)start_named(sprite, nm, "push");
}

int script_on_dink_die(void)
{
    if (g_dink_dying) {
        return 0;
    }
    g_dink_dying = 1;
    snprintf(g_log, sizeof(g_log), "dink die dinfo");
    printf("%s\n", g_log);
    return start_named(1000, "dinfo", "die") == 0 ? 1 : 0;
}

void script_clear_dink_die(void)
{
    g_dink_dying = 0;
}
