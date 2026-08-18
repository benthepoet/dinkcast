/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_var.h"

#include <ctype.h>
#include <string.h>

struct Slot {
    int active;
    int scope;
    int value;
    char name[DINKC_VAR_NAME];
};

static struct Slot g_var[DINKC_MAX_VARS];
static int g_returnint;

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

static int lookup_scope(const char *name, int scope)
{
    int i;

    for (i = 1; i < DINKC_MAX_VARS; i++) {
        if (g_var[i].active && g_var[i].scope == scope &&
            name_eq(g_var[i].name, name)) {
            return i;
        }
    }
    return 0;
}

/* FreeDink 1.08: local then global. */
static int lookup_108(const char *name, int scope)
{
    int i;

    if (scope != DINKC_GLOBAL_SCOPE) {
        i = lookup_scope(name, scope);
        if (i != 0) {
            return i;
        }
    }
    return lookup_scope(name, DINKC_GLOBAL_SCOPE);
}

static int is_engine(const char *name)
{
    return name_eq(name, "&current_sprite") || name_eq(name, "&current_script") ||
           name_eq(name, "&return") ||
           (name[0] == '&' && name[1] == 'a' && name[2] == 'r' && name[3] == 'g' &&
            name[4] >= '1' && name[4] <= '9' && name[5] == '\0');
}

void dinkc_var_init(void)
{
    static const struct {
        const char *n;
        int v;
    } boot[] = {
        /* story/MAIN.c required by the engine */
        {"&exp", 0},
        {"&strength", 3},
        {"&defense", 0},
        {"&cur_weapon", 0},
        {"&cur_magic", 0},
        {"&gold", 0},
        {"&magic", 0},
        {"&magic_level", 0},
        {"&vision", 0},
        {"&result", 0},
        {"&speed", 1},
        {"&timing", 0},
        {"&lifemax", 10},
        {"&life", 10},
        {"&level", 1},
        {"&player_map", 1},
        {"&last_text", 0},
        {"&update_status", 0},
        {"&missile_target", 0},
        {"&enemy_sprite", 0},
        {"&magic_cost", 0},
        {"&missle_source", 0},
        /* official campaign extras from MAIN.c */
        {"&story", 0},
        {"&old_womans_duck", 0},
        {"&nuttree", 0},
        {"&letter", 0},
        {"&little_girl", 0},
        {"&farmer_quest", 0},
        {"&save_x", 0},
        {"&save_y", 0},
        {"&safe", 0},
        {"&pig_story", 0},
        {"&wizard_see", 0},
        {"&mlibby", 0},
        {"&wizard_again", 0},
        {"&snowc", 0},
        {"&duckgame", 0},
        {"&gossip", 0},
        {"&robbed", 0},
        {"&dinklogo", 0},
        {"&rock_placement", 0},
        {"&temphold", 0},
        {"&temp1hold", 0},
        {"&temp2hold", 0},
        {"&temp3hold", 0},
        {"&temp4hold", 0},
        {"&temp5hold", 0},
        {"&temp6hold", 0},
        {"&town1", 0},
        {"&s2-milder", 0},
        {"&thief", 0},
        {"&caveguy", 0},
        {"&s2-aunt", 0},
        {"&tombob", 0},
        {"&mayor", 0},
        {"&hero", 0},
        {"&s2-nad", 0},
        {"&gobpass", 0},
        {"&bowlore", 0},
        {"&s4-duck", 0},
        {"&s5-jop", 0},
        {"&s7-boat", 0},
        {"&s2-map", 0},
    };
    size_t i;

    memset(g_var, 0, sizeof(g_var));
    g_returnint = 0;
    for (i = 0; i < sizeof(boot) / sizeof(boot[0]); i++) {
        dinkc_var_make_global(boot[i].n, boot[i].v);
    }
}

int dinkc_var_make(const char *name, int value, int scope)
{
    int i, slot = 0;

    if (name == NULL || name[0] != '&' || strlen(name) > 19) {
        return 0;
    }
    i = lookup_scope(name, scope);
    if (i != 0) {
        /* FreeDink make_int: existing slot stays. */
        return i;
    }
    for (i = 1; i < DINKC_MAX_VARS; i++) {
        if (!g_var[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == 0) {
        return 0;
    }
    g_var[slot].active = 1;
    g_var[slot].scope = scope;
    g_var[slot].value = value;
    strncpy(g_var[slot].name, name, DINKC_VAR_NAME - 1);
    g_var[slot].name[DINKC_VAR_NAME - 1] = '\0';
    return slot;
}

int dinkc_var_make_global(const char *name, int value)
{
    return dinkc_var_make(name, value, DINKC_GLOBAL_SCOPE);
}

int dinkc_var_get(const char *name, int scope, int sprite)
{
    int i;

    if (name == NULL) {
        return 0;
    }
    if (name_eq(name, "&current_sprite")) {
        return sprite;
    }
    if (name_eq(name, "&current_script")) {
        return scope;
    }
    if (name_eq(name, "&return")) {
        return g_returnint;
    }
    if (name_eq(name, "&arg1") || name_eq(name, "&arg2") ||
        name_eq(name, "&arg3") || name_eq(name, "&arg4") ||
        name_eq(name, "&arg5") || name_eq(name, "&arg6") ||
        name_eq(name, "&arg7") || name_eq(name, "&arg8") ||
        name_eq(name, "&arg9")) {
        return 0;
    }
    i = lookup_108(name, scope);
    if (i == 0) {
        return 0;
    }
    return g_var[i].value;
}

void dinkc_var_kill_scope(int scope)
{
    int i;

    if (scope == DINKC_GLOBAL_SCOPE) {
        return;
    }
    for (i = 1; i < DINKC_MAX_VARS; i++) {
        if (g_var[i].active && g_var[i].scope == scope) {
            g_var[i].active = 0;
        }
    }
}

void dinkc_var_set(const char *name, int value, int scope, int sprite)
{
    int i;

    (void)sprite;
    if (name == NULL) {
        return;
    }
    if (is_engine(name)) {
        if (name_eq(name, "&return")) {
            g_returnint = value;
        }
        return;
    }
    i = lookup_108(name, scope);
    if (i == 0) {
        /* FreeDink assign to missing var is often a no-op / 0. */
        return;
    }
    g_var[i].value = value;
}
