/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"

#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "hurt.h"
#include "player.h"
#include "saybox.h"

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
static void (*g_spr_freeze)(int slot, int on);
static int (*g_spr_change)(int slot, int prop, int val);
static int (*g_create)(int x, int y, int brain, int seq, int fr);
static int (*g_move)(int slot, int dir, int dest, int nohard);
static int (*g_moving)(int slot);
static int (*g_sp_script)(int slot, const char *name);
static int (*g_external)(int sprite, const char *file, const char *proc,
                         const int *args, int nargs);
static int (*g_callback)(const char *proc, int base, int range, int fiber,
                         int sprite);
static int (*g_hurt)(int slot, int damage);
static void (*g_restart)(void);
static int g_fiber;
static int g_cmd_sprite;
static int g_touch[100];
static int g_nitem;
static int g_nmagic;
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
    {"sp_exp", 0},
    {"sp_base_death", 0},
    {"sp_base_die", 0},
    {"sp_editor_num", 0},
    {"sp_custom", 0},
    {"sp", 0},
    {"random", 0},
    {"move", 0},
    {"create_sprite", 0},
    {"script_attach", 0},
    {"external", 0},
    {"set_callback_random", 0},
    {"force_vision", 0},
    {"hurt", 0},
    {"add_exp", 0},
    {"set_dink_base_push", 0},
    {"restart_game", 0},
    {"kill_game", 0},
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

void dinkc_cmd_thaw_if_idle(void)
{
    if (g_pl != NULL && g_pl->freeze > 0 && dinkc_vm_live() == 0) {
        printf("freeze orphan %d -> 0\n", g_pl->freeze);
        g_pl->freeze = 0;
    }
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

void dinkc_cmd_bind_sprite_freeze(void (*fn)(int slot, int on))
{
    g_spr_freeze = fn;
}

void dinkc_cmd_bind_sprite_change(int (*fn)(int slot, int prop, int val))
{
    g_spr_change = fn;
}

void dinkc_cmd_bind_create(int (*fn)(int x, int y, int brain, int seq, int fr))
{
    g_create = fn;
}

void dinkc_cmd_bind_move(int (*fn)(int slot, int dir, int dest, int nohard))
{
    g_move = fn;
}

void dinkc_cmd_bind_moving(int (*fn)(int slot))
{
    g_moving = fn;
}

void dinkc_cmd_bind_sp_script(int (*fn)(int slot, const char *name))
{
    g_sp_script = fn;
}

void dinkc_cmd_bind_external(int (*fn)(int sprite, const char *file,
                                       const char *proc, const int *args,
                                       int nargs))
{
    g_external = fn;
}

void dinkc_cmd_bind_callback(int (*fn)(const char *proc, int base, int range,
                                       int fiber, int sprite))
{
    g_callback = fn;
}

void dinkc_cmd_bind_fiber(int fiber, int sprite)
{
    g_fiber = fiber;
    g_cmd_sprite = sprite;
}

void dinkc_cmd_bind_hurt(int (*fn)(int slot, int damage))
{
    g_hurt = fn;
}

void dinkc_cmd_bind_restart(void (*fn)(void))
{
    g_restart = fn;
}

void dinkc_cmd_set_dink_base_push(int seq)
{
    if (g_pl != NULL) {
        g_pl->base_push = seq;
    }
}

int dinkc_cmd_dink_base_push(void)
{
    if (g_pl != NULL) {
        return g_pl->base_push;
    }
    return DINK_BASE_PUSH;
}

int dinkc_cmd_move_busy(int slot)
{
    if (slot == 1 && g_pl != NULL) {
        return g_pl->move_active;
    }
    if (g_moving != NULL) {
        return g_moving(slot);
    }
    return 0;
}

static int spr_is_dink(int id)
{
    return id == 1;
}

static int change_sp(int slot, int prop, int nargs, int setv, int *ret)
{
    int val = nargs < 2 ? -1 : setv;
    int *p = NULL;
    int v;

    if (spr_is_dink(slot) && g_pl != NULL) {
        if (prop == DINKC_SP_X) {
            p = &g_pl->x;
        } else if (prop == DINKC_SP_Y) {
            p = &g_pl->y;
        } else if (prop == DINKC_SP_DIR) {
            p = &g_pl->dir;
        } else if (prop == DINKC_SP_SEQ) {
            p = &g_pl->seq;
        } else if (prop == DINKC_SP_FRAME) {
            p = &g_pl->frame;
        } else if (prop == DINKC_SP_BASE_ATTACK) {
            p = &g_pl->base_attack;
            } else if (prop == DINKC_SP_BASE_IDLE) {
            p = &g_pl->base_idle;
        } else if (prop == DINKC_SP_PSEQ) {
            p = &g_pl->seq;
        } else if (prop == DINKC_SP_PFRAME) {
            p = &g_pl->frame;
        } else if (prop == DINKC_SP_HITPOINTS) {
            p = &g_pl->hitpoints;
        } else if (prop == DINKC_SP_DEFENSE) {
            p = &g_pl->defense;
        } else if (prop == DINKC_SP_NOHIT) {
            p = &g_pl->nohit;
        } else if (prop == DINKC_SP_STRENGTH) {
            p = &g_pl->strength;
        } else if (prop == DINKC_SP_RANGE) {
            p = &g_pl->range;
        }
        if (p != NULL) {
            if (val != -1) {
                *p = val;
                if (prop == DINKC_SP_SEQ) {
                    g_pl->frame = 1;
                }
            }
            if (ret != NULL) {
                *ret = *p;
            }
            return 1;
        }
    }
    if (slot >= 1 && slot <= 99 && g_spr_change != NULL) {
        v = g_spr_change(slot, prop, val);
        if (ret != NULL) {
            *ret = v;
        }
    }
    return 1;
}

int dinkc_cmd(const char *name, int *args, int nargs, const char *str,
              const char *str2, int *yield, int *ret)
{
    int a0 = nargs > 0 ? args[0] : 0;
    int a1 = nargs > 1 ? args[1] : 0;
    int a2 = nargs > 2 ? args[2] : 0;
    int a3 = nargs > 3 ? args[3] : 0;
    int a4 = nargs > 4 ? args[4] : 0;

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
        int spr = nargs >= 2 ? a1 : a0;

        printf("say %s\n", str != NULL ? str : "");
        saybox_set(str, spr);
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
        /* FreeDink: spr[h].freeze = script (any non-zero). Not a nest. */
        if (spr_is_dink(a0) && g_pl != NULL) {
            g_pl->freeze = 1;
            printf("freeze 1\n");
        } else if (a0 >= 2 && a0 <= 99 && g_spr_freeze != NULL) {
            g_spr_freeze(a0, 1);
            printf("freeze %d\n", a0);
        }
        return 1;
    }
    if (is_cmd(name, "unfreeze")) {
        if (spr_is_dink(a0) && g_pl != NULL) {
            g_pl->freeze = 0;
            printf("unfreeze 1\n");
        } else if (a0 >= 2 && a0 <= 99 && g_spr_freeze != NULL) {
            g_spr_freeze(a0, 0);
            printf("unfreeze %d\n", a0);
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
        return change_sp(a0, DINKC_SP_X, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_y")) {
        return change_sp(a0, DINKC_SP_Y, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_dir")) {
        return change_sp(a0, DINKC_SP_DIR, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_seq")) {
        return change_sp(a0, DINKC_SP_SEQ, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_frame")) {
        return change_sp(a0, DINKC_SP_FRAME, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_base_attack")) {
        return change_sp(a0, DINKC_SP_BASE_ATTACK, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_brain") || is_cmd(name, "sp_speed") ||
        is_cmd(name, "sp_base_walk") || is_cmd(name, "sp_timing")) {
        int prop = DINKC_SP_BRAIN;

        if (is_cmd(name, "sp_speed")) {
            prop = DINKC_SP_SPEED;
        } else if (is_cmd(name, "sp_base_walk")) {
            prop = DINKC_SP_BASE_WALK;
        } else if (is_cmd(name, "sp_timing")) {
            prop = DINKC_SP_TIMING;
        }
        return change_sp(a0, prop, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_base_idle")) {
        return change_sp(a0, DINKC_SP_BASE_IDLE, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_pseq")) {
        return change_sp(a0, DINKC_SP_PSEQ, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_pframe")) {
        return change_sp(a0, DINKC_SP_PFRAME, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_active")) {
        return change_sp(a0, DINKC_SP_ACTIVE, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_kill")) {
        return change_sp(a0, DINKC_SP_KILL, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_script")) {
        if (g_sp_script != NULL && str != NULL && str[0] != '\0') {
            int ok = g_sp_script(a0, str);

            if (ret != NULL) {
                *ret = ok;
            }
        }
        return 1;
    }
    if (is_cmd(name, "move")) {
        if (spr_is_dink(a0) && g_pl != NULL) {
            g_pl->move_active = 1;
            g_pl->move_dir = a1;
            g_pl->move_num = a2;
            g_pl->move_nohard = a3 ? 1 : 0;
        } else if (g_move != NULL) {
            g_move(a0, a1, a2, a3);
        }
        return 1;
    }
    if (is_cmd(name, "create_sprite")) {
        int slot = 0;

        if (g_create != NULL) {
            slot = g_create(a0, a1, a2, a3, a4);
        }
        if (ret != NULL) {
            *ret = slot;
        }
        return 1;
    }
    if (is_cmd(name, "script_attach")) {
        if (ret != NULL) {
            *ret = a0;
        }
        return 1;
    }
    if (is_cmd(name, "external")) {
        if (g_external != NULL && str != NULL && str2 != NULL &&
            str[0] != '\0' && str2[0] != '\0') {
            int child = g_external(g_cmd_sprite, str, str2, args, nargs);

            if (ret != NULL) {
                *ret = child;
            }
            if (child > 0 && yield != NULL) {
                *yield = 5;
            }
        }
        return 1;
    }
    if (is_cmd(name, "set_callback_random")) {
        if (g_callback != NULL && str != NULL && str[0] != '\0') {
            int cb = g_callback(str, a1, a2, g_fiber, g_cmd_sprite);

            if (ret != NULL) {
                *ret = cb;
            }
        }
        return 1;
    }
    if (is_cmd(name, "random")) {
        int range = a0 > 0 ? a0 : 1;

        if (ret != NULL) {
            *ret = (rand() % range) + a1;
        }
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
        return change_sp(a0, DINKC_SP_HITPOINTS, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_defense")) {
        return change_sp(a0, DINKC_SP_DEFENSE, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_touch_damage")) {
        int s = spr_slot(a0);

        if (s != 0) {
            /* FreeDink change_sprite_noreturn: -1 is a real set. */
            if (nargs >= 2) {
                if (g_spr_change != NULL) {
                    (void)g_spr_change(s, DINKC_SP_TOUCH, a1);
                } else {
                    g_touch[s] = a1;
                }
                if (ret != NULL) {
                    *ret = a1;
                }
            } else if (ret != NULL) {
                if (g_spr_change != NULL) {
                    *ret = g_spr_change(s, DINKC_SP_TOUCH, -1);
                } else {
                    *ret = g_touch[s];
                }
            }
        }
        return 1;
    }
    if (is_cmd(name, "hurt")) {
        /* dc_hurt 1.08: damage < 0 is a no-op. */
        if (a1 < 0) {
            return 1;
        }
        if (spr_is_dink(a0)) {
            int life = dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1);

            if (g_pl != NULL) {
                g_pl->defense = dinkc_var_get("&defense", DINKC_GLOBAL_SCOPE, 1);
                (void)player_hurt(g_pl, a1);
                (void)player_apply_life(g_pl, &life);
            } else if (a1 > 0) {
                life -= a1;
                if (life < 0) {
                    life = 0;
                }
            }
            dinkc_var_set("&life", life, DINKC_GLOBAL_SCOPE, 1);
        } else if (g_hurt != NULL) {
            (void)g_hurt(a0, a1);
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
    if (is_cmd(name, "sp_exp")) {
        return change_sp(a0, DINKC_SP_EXP, nargs, a1, ret);
    }
    if (is_cmd(name, "sp_base_die") || is_cmd(name, "sp_base_death")) {
        return change_sp(a0, DINKC_SP_BASE_DIE, nargs, a1, ret);
    }
    if (is_cmd(name, "set_dink_base_push")) {
        dinkc_cmd_set_dink_base_push(a0);
        return 1;
    }
    if (is_cmd(name, "restart_game")) {
        if (g_restart != NULL) {
            g_restart();
        }
        if (yield != NULL) {
            *yield = 1;
        }
        return 1;
    }
    if (is_cmd(name, "kill_game")) {
        printf("kill_game\n");
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
        return change_sp(a0, DINKC_SP_NOHIT, nargs, a1, ret);
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
        return change_sp(a0, DINKC_SP_RANGE, nargs, a1, ret);
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
