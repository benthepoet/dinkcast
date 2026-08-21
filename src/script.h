/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_SCRIPT_H
#define DINKCAST_SCRIPT_H

#include "mapscr.h"

/* Bind editor screen so talk/hit stubs can print script[16]. */
void script_bind_screen(const struct MapScreen *scr);
void script_bind_note_script(void (*fn)(int slot, const char *name));

/* Bite 10.3 stubs: log only. DinkC is 11. */
void script_on_main(int script_id);
/* Load unique sprite + screen story files (11.0). No run. */
int script_preload_screen(void);
/* 11.6: screen MAIN then type-1 sprite main() in rank order. */
int script_attach_screen(void);
/* draw_screen_game: *pvision=0, run screen MAIN. */
void script_enter_vision(void);
/* game_place_sprites / init_scripts for current &vision. */
int script_attach_live(void);
int script_play_vision(void);
void script_on_talk(int sprite);
void script_on_hit(int sprite);
void script_on_hit_from(int sprite, int attacker);
void script_on_kill(int sprite, const char *proc);
void script_on_push(int sprite);
void script_on_touch(int sprite);
/* update_status: flife < 1 → dinfo DIE once. */
int script_on_dink_die(void);
void script_clear_dink_die(void);
int script_on_raise(void);
int script_on_button(int n);
int script_item_arm(const char *name);
int script_item_locate(int slot, const char *proc);
int script_item_pickup(const char *name);

/* Last printf line (no newline). Host tests. */
const char *script_stub_log(void);

#endif
