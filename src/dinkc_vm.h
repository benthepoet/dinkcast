/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_VM_H
#define DINKCAST_DINKC_VM_H

#include <stddef.h>

#define DINKC_MAX_LIVE 20
#define DINKC_LOCALS 256
#define DINKC_TICK_MS 16 /* 60 Hz */
#define DINKC_OPS_CAP 2000

enum DinkcState {
    DINKC_DEAD = 0,
    DINKC_RUN,
    DINKC_WAIT_MS,
    DINKC_WAIT_SAY,
    DINKC_WAIT_MOVE,
    DINKC_WAIT_CHOICE
};

/* Copy src, locate void proc (default main), run until yield. */
int dinkc_vm_start(const char *src, size_t n, int sprite);
int dinkc_vm_start_proc(const char *src, size_t n, int sprite, const char *proc);
int dinkc_vm_waiting_choice(void);
void dinkc_vm_choice_pick(int result);
int dinkc_vm_choice_n(void);
int dinkc_vm_choice_cur(void);
const char *dinkc_vm_choice_line(int vis1);
void dinkc_vm_choice_move(int delta);
void dinkc_vm_kill(int slot);
/* FreeDink: one instance per sprite; locate talk() on it. */
void dinkc_vm_kill_sprite(int sprite);
void dinkc_vm_kill_all(void);
void dinkc_vm_set_now(int now_ms);
void dinkc_vm_reset(void);
/* Resume wait(ms) / move_stop stubs. now_ms is elapsed clock. */
void dinkc_vm_tick(int now_ms);
/* A while say_stop / say_stop_npc. */
void dinkc_vm_advance_say(void);
int dinkc_vm_waiting_say(void);
void dinkc_vm_choice_done(void);
int dinkc_vm_live(void);
int dinkc_vm_state(int slot);

#endif
