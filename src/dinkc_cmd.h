/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_CMD_H
#define DINKCAST_DINKC_CMD_H

struct Player;
struct SeqInfo;
struct MapScreen;

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
#define DINKC_SP_NOCONTROL 24
#define DINKC_SP_MX 25
#define DINKC_SP_MY 26
#define DINKC_SP_FLYING 27
#define DINKC_SP_BRAIN_PARM 28
#define DINKC_SP_BRAIN_PARM2 29
#define DINKC_SP_QUE 30
#define DINKC_SP_DISTANCE 31
#define DINKC_SP_SIZE 32
#define DINKC_SP_HARD 33
#define DINKC_SP_NOTOUCH 34
#define DINKC_SP_FOLLOW 35
#define DINKC_SP_TARGET 36
#define DINKC_SP_ATTACK_WAIT 37
void dinkc_cmd_bind_sprite_change(int (*fn)(int slot, int prop, int val));
void dinkc_cmd_bind_create(int (*fn)(int x, int y, int brain, int seq, int fr));
void dinkc_cmd_bind_move(int (*fn)(int slot, int dir, int dest, int nohard));
void dinkc_cmd_bind_moving(int (*fn)(int slot));
void dinkc_cmd_bind_sp_script(int (*fn)(int slot, const char *name));
void dinkc_cmd_bind_external(int (*fn)(int sprite, const char *file,
                                       const char *proc, const int *args,
                                       int nargs));
void dinkc_cmd_bind_spawn(int (*fn)(const char *file));
void dinkc_cmd_bind_load_screen(int (*fn)(int player_map));
void dinkc_cmd_bind_draw_screen(int (*fn)(int sprite));
void dinkc_cmd_bind_callback(int (*fn)(const char *proc, int base, int range,
                                       int fiber, int sprite));
void dinkc_cmd_bind_fiber(int fiber, int sprite);
void dinkc_cmd_bind_hurt(int (*fn)(int slot, int damage));
void dinkc_cmd_bind_restart(void (*fn)(void));
void dinkc_cmd_bind_brain_lookup(int (*first)(int brain, int ignore, int start),
                                 int (*rnd)(int brain, int ignore));
void dinkc_cmd_set_now(int now_ms);
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
void dinkc_cmd_bind_seqs(struct SeqInfo *seqs);
void dinkc_cmd_bind_preload(void (*fn)(int seq));
void dinkc_cmd_bind_load_frame(void (*fn)(int seq, int frame));
void dinkc_cmd_bind_item(int (*arm)(const char *name),
                         int (*locate)(int slot, const char *proc),
                         int (*pickup)(const char *name));
void dinkc_cmd_bind_inv(void (*show)(int on));
void dinkc_cmd_bind_status(void (*draw)(void),
                           int (*show_bmp)(const char *rel, int showdot,
                                           int fiber));
int dinkc_cmd_inv_active(int magic, int idx0);
int dinkc_cmd_inv_seq(int magic, int idx0);
int dinkc_cmd_inv_frame(int magic, int idx0);
int dinkc_cmd_weapon_armed(void);
int dinkc_cmd_magic_armed(void);
int dinkc_cmd_weapon_use(void);
int dinkc_cmd_magic_use(void);
void dinkc_cmd_reset_inv(void);
void dinkc_cmd_apply_spmap(struct MapScreen *scr, int player_map);
void dinkc_cmd_bind_blood(void (*fn)(int slot));
void dinkc_cmd_bind_hard_redraw(void (*fn)(void));
int dinkc_cmd_hard_redraw_take(void);

#endif
