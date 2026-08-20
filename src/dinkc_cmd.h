/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_CMD_H
#define DINKCAST_DINKC_CMD_H

struct Player;

void dinkc_cmd_bind_player(struct Player *p);
void dinkc_cmd_bind_sprite_freeze(void (*fn)(int slot, int on));
/* prop: change_sprite. val -1 = read (except kill). */
#define DINKC_SP_BRAIN 1
#define DINKC_SP_SPEED 2
#define DINKC_SP_BASE_WALK 3
#define DINKC_SP_TIMING 4
#define DINKC_SP_X 5
#define DINKC_SP_Y 6
#define DINKC_SP_DIR 7
#define DINKC_SP_SEQ 8
#define DINKC_SP_FRAME 9
#define DINKC_SP_BASE_ATTACK 10
#define DINKC_SP_BASE_IDLE 11
#define DINKC_SP_PSEQ 12
#define DINKC_SP_PFRAME 13
#define DINKC_SP_ACTIVE 14
#define DINKC_SP_KILL 15
#define DINKC_SP_HITPOINTS 16
#define DINKC_SP_DEFENSE 17
#define DINKC_SP_EXP 18
#define DINKC_SP_BASE_DIE 19
#define DINKC_SP_NOHIT 20
#define DINKC_SP_STRENGTH 21
#define DINKC_SP_RANGE 22
#define DINKC_SP_TOUCH 23
void dinkc_cmd_bind_sprite_change(int (*fn)(int slot, int prop, int val));
void dinkc_cmd_bind_create(int (*fn)(int x, int y, int brain, int seq, int fr));
void dinkc_cmd_bind_move(int (*fn)(int slot, int dir, int dest, int nohard));
void dinkc_cmd_bind_moving(int (*fn)(int slot));
void dinkc_cmd_bind_sp_script(int (*fn)(int slot, const char *name));
void dinkc_cmd_bind_external(int (*fn)(int sprite, const char *file,
                                       const char *proc, const int *args,
                                       int nargs));
void dinkc_cmd_bind_callback(int (*fn)(const char *proc, int base, int range,
                                       int fiber, int sprite));
void dinkc_cmd_bind_fiber(int fiber, int sprite);
void dinkc_cmd_bind_hurt(int (*fn)(int slot, int damage));
void dinkc_cmd_bind_restart(void (*fn)(void));
void dinkc_cmd_set_dink_base_push(int seq);
int dinkc_cmd_dink_base_push(void);
/* 1 if move()/move_stop still running. */
int dinkc_cmd_move_busy(int slot);
/* 0 = unknown, 1 = ran. *yield: 0 continue, 1 say_stop, 3 kill, 5 external. */
int dinkc_cmd(const char *name, int *args, int nargs, const char *str,
              const char *str2, int *yield, int *ret);
/* 11.9: one table. DINKC_DUMP_FNS=1 prints implemented + missing. */
void dinkc_cmd_dump(void);
int dinkc_cmd_implemented_count(void);
int dinkc_cmd_missing_count(void);
/* If freeze>0 and no live fibers, clear (script died before unfreeze). */
void dinkc_cmd_thaw_if_idle(void);

#endif
