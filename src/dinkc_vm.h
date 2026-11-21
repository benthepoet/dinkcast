/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_VM_H
#define DINKCAST_DINKC_VM_H

#include <stddef.h>

/* FreeDink dinkc.h MAX_SCRIPTS 200 with slot 0 unused: 199 live (slots
 * 1..199). Instances share one copy of each script's source and tokens
 * (refcounted), so the cap costs ~100 B per slot. */
#define DINKC_MAX_LIVE 199
#define DINKC_TICK_MS 16 /* 60 Hz */
#define DINKC_OPS_CAP 2000

enum DinkcState {
    DINKC_DEAD = 0,
    DINKC_RUN,
    DINKC_WAIT_MS,
    DINKC_WAIT_SAY,
    DINKC_WAIT_MOVE,
    DINKC_WAIT_CHOICE,
    DINKC_WAIT_EXT,
    DINKC_WAIT_BMP,
    DINKC_WAIT_FADE,
    DINKC_WAIT_BUTTON, /* wait_for_button */
    DINKC_IDLE /* armed item script: ARM finished, USE/DISARM still locate */
};

/* Copy src, locate void proc (default main), run until yield. */
int dinkc_vm_start(const char *src, size_t n, int sprite);
int dinkc_vm_start_proc(const char *src, size_t n, int sprite, const char *proc);
/* Like start_proc but the fiber stays after ARM so USE can locate. */
int dinkc_vm_start_keep(const char *src, size_t n, int sprite, const char *proc);
/* Run proc on a keep fiber (FreeDink locate). */
int dinkc_vm_locate(int slot, const char *proc);
/* FreeDink load_script + spr.script: the sprite's resident script. Runs
 * MAIN if present; with no MAIN it stays attached IDLE so procs locate on
 * it and compare_sprite_script sees it. */
int dinkc_vm_attach(const char *src, size_t n, int sprite);
/* FreeDink spr[sprite].script: the attached (keep) script only, 0 if none. */
int dinkc_vm_sprite_keep(int sprite);
/* 1 while the slot's fiber is executing somewhere up the C stack. */
int dinkc_vm_running(int slot);
/* FreeDink kill_scripts_with_inactive_sprites: each tick, scripts whose
 * sprite (> 0, != 1000) the callback reports dead are killed. */
void dinkc_vm_bind_sprite_alive(int (*fn)(int sprite));
/* A dead sprite slot was reused (FreeDink add_sprite zeroes spr.script). */
void dinkc_vm_sprite_reused(int sprite);
/* Fibers not IDLE (running or waiting): live minus attached-idle. */
int dinkc_vm_busy(void);
void dinkc_vm_set_args(int slot, const int *args, int n);
int dinkc_vm_arg(int slot, int n1);
int dinkc_vm_used(int slot);
/* Keep fiber for this sprite, else any live instance. 0 if none. */
int dinkc_vm_sprite_fiber(int sprite);
/* FreeDink kill_returning_stuff: drop wait callbacks, keep the script. */
void dinkc_vm_kill_returning(int slot);
void dinkc_vm_resume_move(void);
void dinkc_vm_tick_callbacks(int now_ms);
int dinkc_vm_waiting_choice(void);
void dinkc_vm_choice_pick(int result);
int dinkc_vm_choice_n(void);
int dinkc_vm_choice_cur(void);
const char *dinkc_vm_choice_line(int vis1);
const char *dinkc_vm_choice_title(void);
int dinkc_vm_choice_newy(void);
int dinkc_vm_choice_color(void);
void dinkc_vm_choice_move(int delta);
/* start-2.c load() lines without a fiber (title Continue). */
void dinkc_vm_choice_open_saves(void);
void dinkc_vm_choice_close_saves(void);
/* V0.3 Start pause: Continue / Title on the seq 30 overlay. cur1 is 1 or 2. */
void dinkc_vm_choice_open_pause(int cur1);
void dinkc_vm_kill(int slot);
/* FreeDink: one instance per sprite; locate talk() on it. */
void dinkc_vm_kill_sprite(int sprite);
void dinkc_vm_kill_all(void);
void dinkc_vm_set_now(int now_ms);
void dinkc_vm_reset(void);
/* Resume wait(ms) / move_stop stubs. now_ms is elapsed clock. */
void dinkc_vm_tick(int now_ms);
/* A while say_stop / say_stop_npc. */
void dinkc_vm_note_last_talk(int fiber);
int dinkc_vm_last_talk(void);
void dinkc_vm_advance_say(void);
/* Resume one say_stop fiber if it is still waiting on a line. */
void dinkc_vm_advance_say_fiber(int slot);
/* saybox_tick then resume only the fibers whose lines expired. */
void dinkc_vm_pump_say(int now_ms);
/* ACTION_TALK: mark that script's lines expired. pump_say runs them. */
void dinkc_vm_press_talk(int now_ms);
/* 1 if play.last_talk's script is still alive (not only while WAIT_SAY). */
int dinkc_vm_talk_script(void);
/* FreeDink wait4b button id from a pad edge, or 0. */
int dinkc_vm_button_code(unsigned prev, unsigned now);
/* Set &result and resume wait_for_button. button 0 does nothing. */
void dinkc_vm_advance_button(int button);
/* If a script is in wait_for_button and a pad edge is new, resume it
 * and return 1 so the same edge is not also talk or attack. */
int dinkc_vm_eat_button(unsigned prev, unsigned now);
int dinkc_vm_waiting_say(void);
void dinkc_vm_advance_bmp(void);
int dinkc_vm_waiting_bmp(void);
void dinkc_vm_choice_done(void);
int dinkc_vm_live(void);
/* FreeDink rinfo->name: name the next started script after its file
 * (script.c calls this before each start). Same-file procs and timer
 * callbacks inherit their parent's name. */
void dinkc_vm_name_next(const char *file);
/* File name of a live script, NULL if the slot is dead. */
const char *dinkc_vm_name(int slot);
/* Distinct scripts held (shared by their instances) and their bytes. */
int dinkc_vm_code_count(void);
size_t dinkc_vm_code_bytes(void);
int dinkc_vm_state(int slot);

#endif
