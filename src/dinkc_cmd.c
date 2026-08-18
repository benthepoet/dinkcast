/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"

#include "dinkc_var.h"
#include "player.h"

#include <ctype.h>
#include <stdio.h>
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

void dinkc_cmd_bind_player(struct Player *p)
{
    g_pl = p;
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
    if (is_cmd(name, "wait") || is_cmd(name, "move_stop") ||
        is_cmd(name, "choice_start") || is_cmd(name, "choice_end")) {
        return 0; /* VM handles yield */
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
        if (g_nitem < 16) {
            g_nitem++;
        }
        printf("add_item %s\n", str != NULL ? str : "");
        return 1;
    }
    if (is_cmd(name, "add_magic")) {
        if (g_nmagic < 8) {
            g_nmagic++;
        }
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
    return 0;
}
