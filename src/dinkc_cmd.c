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
        is_cmd(name, "choice_start")) {
        return 0; /* VM handles yield */
    }
    return 0;
}
