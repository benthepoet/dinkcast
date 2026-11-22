/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PLAYER_H
#define DINKCAST_PLAYER_H

#include "hard.h"
#include "ini.h"

struct MapScreen;

#define DINK_BASE_WALK 70
#define DINK_BASE_IDLE 10
#define DINK_BASE_ATTACK 100 /* fists; START-1.c / item-fst */
#define DINK_BASE_PUSH 310 /* human_brain dink_base_push default */
#define DINK_SPEED 3

struct Player {
    int x, y, dir, seq, frame, acc;
    int pseq, pframe; /* getpic: seq==0 holds last anim (S1-HOLE crawl) */
    int freeze; /* FreeDink spr[1].freeze nest */
    int brain; /* spr[1].brain; S1-HOLE 0 skips freeze: idle */
    int nocontrol; /* attack lock (item-fst / spr.nocontrol) */
    int just_hit; /* 1 after landing on SET_FRAME_SPECIAL */
    int just_push; /* 1 the frame push seq starts (tag_list_push) */
    int base_attack;
    int base_idle;
    int base_push;
    int base_hit; /* FreeDink spr[1].base_hit; START-1 / item-fst 100 */
    int distance;
    int warp_hit; /* editor slot if last step hit hardness 100+is_warp */
    int move_active, move_dir, move_num, move_nohard;
    int hitpoints, defense, strength, nohit, range, damage, last_hit;
    int push_active, push_dir, push_timer;
    int notouch; /* run_through_touch_damage_list: skip +damage while set */
    int notouch_timer;
    int frame_delay; /* FreeDink spr[1].frame_delay; 0 = seq delay */
    int attack_hit_sound, attack_hit_sound_speed;
};

void player_init(struct Player *p);
/* Snap diag like item-fst; start base_attack+dir. No-op if freeze/nocontrol. */
void player_attack(struct Player *p, const struct SeqInfo *seqs);
void player_seq_for_input(const struct Player *p, int pad_dir, int *seq,
                          int *nframes_hint);
void player_step(struct Player *p, int pad_dir, const struct HardMask *mask,
                 const struct SeqInfo *seqs, int now_ms,
                 const struct MapScreen *scr);
/* FreeDink human_brain: freeze or game_choice.active → no walk pad. */
int player_walk_pad(int pad_dir, int freeze, int choice_active);
/* FreeDink hurt_thing on spr[1]. Adds to .damage. */
int player_hurt(struct Player *p, int damage);
/* FreeDink dinkspeed: set_dink_speed (0 ignored), load_game resets to 3.
 * player_walk_speed is the game_compute_speed pixels per tick. */
void player_set_dink_speed(int speed);
void player_reset_dink_speed(void);
int player_dink_speed(void);
int player_walk_speed(void);

/* FreeDink game_compute_speed: the frame delta fps_final is clamped to
 * 12..68 ms and base_timing = max(4, fps_final / 3). Speeds scale by
 * base_timing / 4 (1 at 60 Hz, 2 at 30 FPS). */
#define DINK_FRAME_MS_MIN 12
#define DINK_FRAME_MS_MAX 68
struct GameClock {
    unsigned last_real; /* real ms at the previous tick */
    int now;            /* game ms: what wait(), delays and timers see */
    int frame_ms;       /* clamped fps_final of the last tick */
    int started;
};
/* One frame: advance the game clock by the real time since the last tick
 * (thisTickCount = game_GetTicks()). Dreamcast exception: one frame adds at
 * most DINK_FRAME_MS_MAX, so a GD-ROM load or a blocking menu inside a
 * frame does not expire waits and talk lines. Sets base_timing. Returns
 * the new game time. */
int gameclock_tick(struct GameClock *c, unsigned real_ms);
int player_base_timing(void);
void player_set_base_timing(int base_timing);
/* human_brain: *plife -= damage, clamp 0. Writes *life. Returns 1 if life < 1. */
int player_apply_life(struct Player *p, int *life);
/* FreeDink getpic(1): seq if >0 else pseq / pframe. */
int player_pic_seq(const struct Player *p);
/* FreeDink human_brain draw_damage(1): called by player_apply_life with the
 * damage about to come off &life, before it is cleared. */
void player_bind_damage_draw(void (*fn)(const struct Player *p, int damage));
int player_pic_frame(const struct Player *p);

#endif
