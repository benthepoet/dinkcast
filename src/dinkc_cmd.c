/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"

#include "dinkc_var.h"
#include "player.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_cmd(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static struct Player *g_pl;
static int g_hp[100];
static int g_def[100];
static int g_touch[100];
static int g_nitem;
static int g_nmagic;
static int g_range[100];
static int g_target[100];
static int g_atkwait[100];
static int g_midi;
static char g_item[16][16];
static char g_magic[8][16];
static struct {
    int spr;
    int val;
    char key[20];
} g_custom[32];

static int name_eq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void store_name(char dest[][16], int *n, int cap, const char *s)
{
    if (*n >= cap) {
        return;
    }
    if (s == NULL) {
        s = "";
    }
    strncpy(dest[*n], s, 15);
    dest[*n][15] = '\0';
    (*n)++;
}

static int spr_slot(int id)
{
    if (id < 1 || id > 99) {
        return 0;
    }
    return id;
}

static int change_i(int *slot, int nargs, int setv, int *ret)
{
    if (nargs < 2 || setv == -1) {
        if (ret != NULL) {
            *ret = *slot;
        }
        return *slot;
    }
    *slot = setv;
    if (ret != NULL) {
        *ret = setv;
    }
    return setv;
}

#define CMD_VM 1

static const struct {
    const char *n;
    unsigned char flags;
} k_fn[] = {
    {"say", 0},
    {"say_stop", 0},
    {"say_stop_npc", 0},
    {"debug", 0},
    {"playsound", 0},
    {"playmidi", 0},
    {"stopcd", 0},
    {"freeze", 0},
    {"unfreeze", 0},
    {"kill_this_task", 0},
    {"stop", 0},
    {"wait_for_button", 0},
    {"wait", CMD_VM},
    {"move_stop", CMD_VM},
    {"choice_start", CMD_VM},
    {"choice_end", CMD_VM},
    {"sp_x", 0},
    {"sp_y", 0},
    {"sp_dir", 0},
    {"sp_seq", 0},
    {"sp_frame", 0},
    {"sp_base_attack", 0},
    {"sp_base_walk", 0},
    {"sp_base_idle", 0},
    {"sp_speed", 0},
    {"sp_timing", 0},
    {"sp_pseq", 0},
    {"sp_pframe", 0},
    {"sp_brain", 0},
    {"sp_script", 0},
    {"sp_active", 0},
    {"sp_kill", 0},
    {"sp_hitpoints", 0},
    {"sp_defense", 0},
    {"sp_touch_damage", 0},
    {"sp_nohit", 0},
    {"sp_range", 0},
    {"sp_target", 0},
    {"sp_attack_wait", 0},
    {"sp_editor_num", 0},
    {"sp_custom", 0},
    {"sp", 0},
    {"move", 0},
    {"create_sprite", 0},
    {"script_attach", 0},
    {"external", 0},
    {"set_callback_random", 0},
    {"force_vision", 0},
    {"hurt", 0},
    {"add_exp", 0},
    {"add_item", 0},
    {"add_magic", 0},
    {"initfont", 0},
    {"get_next_sprite_with_this_brain", 0},
    {"draw_status", 0},
    {"update_status", 0},
    {"preload_seq", 0},
    {"kill_shadow", 0},
    {"arm_weapon", 0},
    {"arm_magic", 0},
    {"fade_up", 0},
    {"fade_down", 0},
    {"fill_screen", 0},
    {"load_screen", 0},
    {"compare_weapon", 0},
    {"compare_magic", 0},
};

#define CMD_N ((int)(sizeof(k_fn) / sizeof(k_fn[0])))
#define MISS_N 32

static char g_miss[MISS_N][32];
static int g_nmiss;
static int g_dump_hooked;

static int lookup_fn(const char *name)
{
    int i;

    for (i = 0; i < CMD_N; i++) {
        if (name_eq(k_fn[i].n, name)) {
            return i;
        }
    }
    return -1;
}

static void note_miss(const char *name)
{
    int i;

    if (name == NULL || name[0] == '\0') {
        return;
    }
    for (i = 0; i < g_nmiss; i++) {
        if (name_eq(g_miss[i], name)) {
            return;
        }
    }
    if (g_nmiss >= MISS_N) {
        return;
    }
    strncpy(g_miss[g_nmiss], name, 31);
    g_miss[g_nmiss][31] = '\0';
    g_nmiss++;
}

int dinkc_cmd_implemented_count(void)
{
    return CMD_N;
}

int dinkc_cmd_missing_count(void)
{
    return g_nmiss;
}

void dinkc_cmd_dump(void)
{
    int i;

    printf("dinkc dump implemented=%d", CMD_N);
    for (i = 0; i < CMD_N; i++) {
        printf(" %s", k_fn[i].n);
    }
    printf("\n");
    printf("dinkc dump missing=%d", g_nmiss);
    for (i = 0; i < g_nmiss; i++) {
        printf(" %s", g_miss[i]);
    }
    printf("\n");
}

static int dump_wanted(void)
{
    const char *e = getenv("DINKC_DUMP_FNS");

    return e != NULL && e[0] != '\0' && e[0] != '0';
}

void dinkc_cmd_bind_player(struct Player *p)
{
    g_pl = p;
    if (dump_wanted() && !g_dump_hooked) {
        g_dump_hooked = 1;
        dinkc_cmd_dump();
        atexit(dinkc_cmd_dump);
    }
}

static int spr_is_dink(int id)
{
    return id == 1;
}

int dinkc_cmd(const char *name, int *args, int nargs, const char *str,
              int *yield, int *ret)
{
    int a0 = nargs > 0 ? args[0] : 0;
    int a1 = nargs > 1 ? args[1] : 0;

    if (yield != NULL) {
        *yield = 0;
    }
    if (ret != NULL) {
        *ret = 0;
    }
    if (name == NULL) {
        return 0;
    }
    {
        int ix = lookup_fn(name);

        if (ix < 0) {
            note_miss(name);
            return 0;
        }
        if (k_fn[ix].flags == CMD_VM) {
            return 0;
        }
    }
    if (is_cmd(name, "say") || is_cmd(name, "say_stop") ||
        is_cmd(name, "say_stop_npc")) {
        printf("say %s\n", str != NULL ? str : "");
        if (yield != NULL && !is_cmd(name, "say")) {
            *yield = 1;
        }
        return 1;
    }
    if (is_cmd(name, "debug")) {
        printf("debug %s\n", str != NULL ? str : "");
        return 1;
    }
    if (is_cmd(name, "playsound")) {
        printf("playsound stub %d\n", a0);
        return 1;
    }
    if (is_cmd(name, "freeze")) {
        if (spr_is_dink(a0) && g_pl != NULL) {
            g_pl->freeze++;
        }
        return 1;
    }
    if (is_cmd(name, "unfreeze")) {
        if (spr_is_dink(a0) && g_pl != NULL && g_pl->freeze > 0) {
            g_pl->freeze--;
        }
        return 1;
    }
    if (is_cmd(name, "kill_this_task")) {
        if (yield != NULL) {
            *yield = 3; /* kill fiber */
        }
        return 1;
    }
    if (is_cmd(name, "sp_x")) {
        if (spr_is_dink(a0) && g_pl != NULL) {
            if (nargs < 2 || a1 == -1) {
                if (ret != NULL) {
                    *ret = g_pl->x;
                }
            } else {
                g_pl->x = a1;
            }
        }
        return 1;
    }
    if (is_cmd(name, "sp_y")) {
        if (spr_is_dink(a0) && g_pl != NULL) {
            if (nargs < 2 || a1 == -1) {
                if (ret != NULL) {
                    *ret = g_pl->y;
                }
            } else {
                g_pl->y = a1;
            }
        }
        return 1;
    }
    if (is_cmd(name, "sp_dir")) {
        if (spr_is_dink(a0) && g_pl != NULL) {
            if (nargs < 2 || a1 == -1) {
                if (ret != NULL) {
                    *ret = g_pl->dir;
                }
            } else {
                g_pl->dir = a1;
            }
        }
        return 1;
    }
    if (is_cmd(name, "sp_seq")) {
        if (spr_is_dink(a0) && g_pl != NULL && nargs >= 2 && a1 != -1) {
            g_pl->seq = a1;
            g_pl->frame = 1;
        }
        return 1;
    }
    if (is_cmd(name, "sp_frame")) {
        if (spr_is_dink(a0) && g_pl != NULL && nargs >= 2 && a1 != -1) {
            g_pl->frame = a1;
        }
        return 1;
    }
    if (is_cmd(name, "sp_base_attack")) {
        if (spr_is_dink(a0) && g_pl != NULL && nargs >= 2 && a1 != -1) {
            g_pl->base_attack = a1;
        }
        return 1;
    }
    if (is_cmd(name, "sp_base_walk") || is_cmd(name, "sp_base_idle") ||
        is_cmd(name, "sp_speed") || is_cmd(name, "sp_timing") ||
        is_cmd(name, "sp_pseq") || is_cmd(name, "sp_pframe") ||
        is_cmd(name, "sp_brain") || is_cmd(name, "sp_script") ||
        is_cmd(name, "sp_active") || is_cmd(name, "sp_kill") ||
        is_cmd(name, "move") || is_cmd(name, "create_sprite") ||
        is_cmd(name, "script_attach") || is_cmd(name, "external") ||
        is_cmd(name, "set_callback_random")) {
        return 1;
    }
    if (is_cmd(name, "force_vision")) {
        dinkc_var_set("&vision", a0, DINKC_GLOBAL_SCOPE, 1);
        return 1;
    }
    if (is_cmd(name, "stop")) {
        if (yield != NULL) {
            *yield = 3;
        }
        return 1;
    }
    if (is_cmd(name, "wait_for_button")) {
        printf("wait_for_button\n");
        if (yield != NULL) {
            *yield = 1;
        }
        return 1;
    }
    if (is_cmd(name, "sp_hitpoints")) {
        int s = spr_slot(a0);

        if (s != 0) {
            change_i(&g_hp[s], nargs, a1, ret);
        }
        return 1;
    }
    if (is_cmd(name, "sp_defense")) {
        int s = spr_slot(a0);

        if (s != 0) {
            change_i(&g_def[s], nargs, a1, ret);
        }
        return 1;
    }
    if (is_cmd(name, "sp_touch_damage")) {
        int s = spr_slot(a0);

        if (s != 0) {
            /* FreeDink change_sprite_noreturn: -1 is a real set. */
            if (nargs >= 2) {
                g_touch[s] = a1;
                if (ret != NULL) {
                    *ret = a1;
                }
            } else if (ret != NULL) {
                *ret = g_touch[s];
            }
        }
        return 1;
    }
    if (is_cmd(name, "hurt")) {
        if (spr_is_dink(a0)) {
            int life = dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1);

            if (a1 > 0) {
                life -= a1;
                if (life < 0) {
                    life = 0;
                }
                dinkc_var_set("&life", life, DINKC_GLOBAL_SCOPE, 1);
            }
        }
        return 1;
    }
    if (is_cmd(name, "add_exp")) {
        int exp = dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1);

        exp += a0;
        if (exp > 99999) {
            exp = 99999;
        }
        dinkc_var_set("&exp", exp, DINKC_GLOBAL_SCOPE, 1);
        return 1;
    }
    if (is_cmd(name, "add_item")) {
        store_name(g_item, &g_nitem, 16, str);
        printf("add_item %s\n", str != NULL ? str : "");
        return 1;
    }
    if (is_cmd(name, "add_magic")) {
        store_name(g_magic, &g_nmagic, 8, str);
        printf("add_magic %s\n", str != NULL ? str : "");
        return 1;
    }
    if (is_cmd(name, "initfont") ||
        is_cmd(name, "get_next_sprite_with_this_brain")) {
        return 1;
    }
    if (is_cmd(name, "sp_nohit")) {
        return 1;
    }
    /* 11.8 wave 3: dispatch so stock scripts do not log unimplemented. */
    if (is_cmd(name, "playmidi")) {
        if (str != NULL && str[0] != '\0') {
            g_midi = atoi(str);
        } else {
            g_midi = a0;
        }
        printf("playmidi stub %s\n", str != NULL ? str : "");
        return 1;
    }
    if (is_cmd(name, "stopcd")) {
        return 1;
    }
    if (is_cmd(name, "draw_status") || is_cmd(name, "update_status")) {
        return 1;
    }
    if (is_cmd(name, "preload_seq") || is_cmd(name, "kill_shadow") ||
        is_cmd(name, "arm_weapon") || is_cmd(name, "arm_magic") ||
        is_cmd(name, "fade_up") || is_cmd(name, "fade_down") ||
        is_cmd(name, "fill_screen") || is_cmd(name, "load_screen")) {
        return 1;
    }
    if (is_cmd(name, "sp_range")) {
        int s = spr_slot(a0);

        if (s != 0) {
            change_i(&g_range[s], nargs, a1, ret);
        }
        return 1;
    }
    if (is_cmd(name, "sp_target")) {
        int s = spr_slot(a0);

        if (s != 0) {
            change_i(&g_target[s], nargs, a1, ret);
        }
        return 1;
    }
    if (is_cmd(name, "sp_attack_wait")) {
        int s = spr_slot(a0);

        if (s != 0) {
            change_i(&g_atkwait[s], nargs, a1, ret);
        }
        return 1;
    }
    if (is_cmd(name, "sp_editor_num") || is_cmd(name, "sp")) {
        /* Live sprite == editor slot until 15. */
        if (ret != NULL) {
            *ret = spr_slot(a0);
        }
        return 1;
    }
    if (is_cmd(name, "compare_weapon")) {
        int cur = dinkc_var_get("&cur_weapon", DINKC_GLOBAL_SCOPE, 1);
        int ok = 0;

        if (cur >= 1 && cur <= g_nitem) {
            ok = name_eq(g_item[cur - 1], str);
        }
        if (ret != NULL) {
            *ret = ok;
        }
        return 1;
    }
    if (is_cmd(name, "compare_magic")) {
        int cur = dinkc_var_get("&cur_magic", DINKC_GLOBAL_SCOPE, 1);
        int ok = 0;

        if (cur >= 1 && cur <= g_nmagic) {
            ok = name_eq(g_magic[cur - 1], str);
        }
        if (ret != NULL) {
            *ret = ok;
        }
        return 1;
    }
    if (is_cmd(name, "sp_custom")) {
        /* parse_args: str=key, args 0 (string), sprite, val */
        int spr = nargs > 1 ? args[1] : a0;
        int val = nargs > 2 ? args[2] : a1;
        int i, empty = -1;

        if (str == NULL) {
            str = "";
        }
        for (i = 0; i < 32; i++) {
            if (g_custom[i].key[0] == '\0') {
                if (empty < 0) {
                    empty = i;
                }
                continue;
            }
            if (g_custom[i].spr == spr && name_eq(g_custom[i].key, str)) {
                if (val != -1) {
                    g_custom[i].val = val;
                }
                if (ret != NULL) {
                    *ret = g_custom[i].val;
                }
                return 1;
            }
        }
        if (empty >= 0) {
            g_custom[empty].spr = spr;
            g_custom[empty].val = (val == -1) ? 0 : val;
            strncpy(g_custom[empty].key, str, 19);
            g_custom[empty].key[19] = '\0';
            if (ret != NULL) {
                *ret = g_custom[empty].val;
            }
        }
        return 1;
    }
    return 1; /* in k_fn: default stub */
}
