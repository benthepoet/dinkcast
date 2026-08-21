/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"

#include "dinkc_cmd.h"
#include "hurt.h"
#include "player.h"
#include "tiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DINK_BRAIN_NONE 0
#define DINK_BRAIN_PLAYER 1
#define DINK_BRAIN_BOUNCE 2
#define DINK_BRAIN_DUCK 3
#define DINK_BRAIN_PIG 4
#define DINK_BRAIN_ONETIME 5
#define DINK_BRAIN_REPEAT 6
#define DINK_BRAIN_ONETIME_STAY 7
#define DINK_BRAIN_TEXT 8
#define DINK_BRAIN_PILL 9
#define DINK_BRAIN_DRAGON 10
#define DINK_BRAIN_MISSILE 11
#define DINK_BRAIN_SCALE 12
#define DINK_BRAIN_MOUSE 13
#define DINK_BRAIN_BUTTON 14
#define DINK_BRAIN_SHADOW 15
#define DINK_BRAIN_PEOPLE 16
#define DINK_BRAIN_MISS_EXPIRE 17
#define DINK_GFX_W 640

#define DINK_PLAYX (DINK_PLAY_LEFT + DINK_PLAY_W)
#define DINK_PLAYY DINK_PLAY_H

/* FreeDink game_compute_speed at 60 Hz: fps_final clamped to 12 → 4. */
#define DINK_BASE_TIMING 4

struct BrainSpr {
    int live;
    int x, y;
    int mx, my;
    int seq, frame;
    int seq_orig;
    int pseq, pframe;
    int delay;
    int wait;
    int freeze;
    int dir;
    int action;
    int move_wait;
    int speed;
    int base_walk;
    int timing;
    int size;
    int brain_parm;
    int brain_parm2;
    int que;
    int flying;
    int brain;
    int logged;
    int created;
    int hidden;
    int base_idle;
    int base_attack;
    int move_active;
    int move_dir;
    int move_num;
    int move_nohard;
    int kill_ttl;
    int kill_start;
    int hitpoints, defense, strength, exp, base_die, nohit, range;
    int touch_damage, damage, last_hit, target, just_hit;
    int hard, notouch, bloodseq, bloodnum;
    char script[16];
};

static struct BrainSpr g_b[101];
static const struct MapScreen *g_map;
static int g_unimpl;
static void (*g_on_kill)(int slot, const char *proc);
static void (*g_on_exp)(int num);
static struct Player *g_pl;

static int seq_on(const struct SeqInfo *seqs, int seq)
{
    return seqs != NULL && seq >= 1 && seq < DINK_MAX_SEQ &&
           seqs[seq].prefix[0] != '\0';
}

static int seq_delay(const struct SeqInfo *seqs, int seq, int frame)
{
    int d = 50;

    if (seqs != NULL && seq >= 1 && seq < DINK_MAX_SEQ) {
        d = ini_frame_delay(seq, frame, seqs[seq].delay);
    }
    if (d < 1) {
        d = 50;
    }
    return d;
}

static int seq_len(const struct SeqInfo *seqs, int seq)
{
    int n = 1;

    if (seqs != NULL && seq >= 1 && seq < DINK_MAX_SEQ) {
        n = ini_seq_len(seq, seqs[seq].nframes);
    }
    if (n < 1) {
        n = 1;
    }
    return n;
}

static int spr_i(const struct BrainSpr *s)
{
    return (int)(s - g_b);
}

static int in_this_base(int seq, int base)
{
    return (seq / 10) * 10 == base;
}

static void change_dir_to_diag(int *dir)
{
    if (dir == NULL) {
        return;
    }
    if (*dir == 8) {
        *dir = 7;
    }
    if (*dir == 4) {
        *dir = 1;
    }
    if (*dir == 2) {
        *dir = 3;
    }
    if (*dir == 6) {
        *dir = 9;
    }
}

static void add_exp_if_player(struct BrainSpr *s)
{
    if (s->last_hit == 1 && g_on_exp != NULL && s->exp > 0) {
        g_on_exp(s->exp);
    }
}

/* FreeDink add_kill_sprite. */
static void add_kill_sprite(struct BrainSpr *s, const struct SeqInfo *seqs)
{
    int dir = s->dir;
    int base = s->base_die;
    int slot;

    if (dir > 9 || dir < 1) {
        dir = 3;
    }
    if (base < 1) {
        if (seq_on(seqs, s->base_walk + 5)) {
            add_exp_if_player(s);
            slot = brains_create(s->x, s->y, 5, s->base_walk + 5, 1);
            if (slot > 0) {
                g_b[slot].speed = 0;
                g_b[slot].seq = s->base_walk + 5;
                g_b[slot].size = s->size;
            }
            return;
        }
        dir = 0;
        base = 164;
    }
    if (!seq_on(seqs, base + dir)) {
        if (dir == 1) {
            dir = 9;
        } else if (dir == 3) {
            dir = 7;
        } else if (dir == 7) {
            dir = 3;
        } else if (dir == 9) {
            dir = 1;
        } else if (dir == 4) {
            dir = 6;
        } else if (dir == 6) {
            dir = 4;
        } else if (dir == 8) {
            dir = 2;
        } else if (dir == 2) {
            dir = 8;
        }
    }
    slot = brains_create(s->x, s->y, base == 164 ? 7 : 5, base + dir, 1);
    if (slot > 0) {
        g_b[slot].speed = 0;
        g_b[slot].base_walk = 0;
        g_b[slot].seq = base + dir;
        g_b[slot].size = s->size;
        if (base == 164) {
            g_b[slot].brain = 7;
        }
    }
    add_exp_if_player(s);
}

static void draw_damage(struct BrainSpr *s, const struct SeqInfo *seqs)
{
    int slot, x, y, seq, fr, cx = 0, cy = 0, hl = 0, ht = 0, hr = 50, hb = 50;

    if (s == NULL || s->damage <= 0) {
        return;
    }
    x = s->x;
    y = s->y;
    seq = s->pseq > 0 ? s->pseq : s->seq;
    fr = s->pframe > 0 ? s->pframe : (s->frame > 0 ? s->frame : 1);
    if (seqs != NULL && seq > 0 && seq < DINK_MAX_SEQ) {
        ini_frame_geom(&seqs[seq], seq, fr, 50, 50, &cx, &cy, &hl, &ht, &hr,
                       &hb);
        y -= cy;
        x -= cx;
        y -= hb / 3;
        x += hr / 5;
    } else {
        y -= 40;
    }
    slot = brains_create(x, y, DINK_BRAIN_TEXT, 0, 0);
    if (slot < 1) {
        return;
    }
    g_b[slot].speed = 1;
    g_b[slot].hard = 1;
    g_b[slot].brain_parm = spr_i(s);
    g_b[slot].my = -1;
    g_b[slot].mx = 0;
    g_b[slot].kill_ttl = 1000;
    g_b[slot].dir = 8;
    g_b[slot].damage = s->damage;
    g_b[slot].seq = 0;
    g_b[slot].pseq = 0;
}

void brains_draw_damage(int slot, const struct SeqInfo *seqs)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return;
    }
    draw_damage(&g_b[slot], seqs);
}

void brains_random_blood(int mx, int my, int sprite)
{
    int myseq = 187;
    int randy = 3;
    int slot;

    if (sprite >= 1 && sprite <= 100 && g_b[sprite].live &&
        g_b[sprite].bloodseq > 0 && g_b[sprite].bloodnum > 0) {
        myseq = g_b[sprite].bloodseq;
        randy = g_b[sprite].bloodnum;
    }
    if (randy < 1) {
        randy = 1;
    }
    myseq += (rand() % randy);
    slot = brains_create(mx, my, DINK_BRAIN_ONETIME, myseq, 1);
    if (slot < 1) {
        return;
    }
    g_b[slot].speed = 0;
    g_b[slot].base_walk = -1;
    g_b[slot].nohit = 1;
    g_b[slot].seq = myseq;
    if (sprite >= 1 && sprite <= 100 && g_b[sprite].live) {
        g_b[slot].que = g_b[sprite].y + 1;
    }
}

/* people/pill/dragon: draw_damage then hp. 1 if killed this tick. */
static int apply_hp_kill(struct BrainSpr *s, const struct SeqInfo *seqs)
{
    if (s->damage <= 0) {
        return 0;
    }
    if (s->hitpoints > 0) {
        draw_damage(s, seqs);
        if (s->damage > s->hitpoints) {
            s->damage = s->hitpoints;
        }
        s->hitpoints -= s->damage;
        if (s->hitpoints < 1) {
            if (g_on_kill != NULL) {
                g_on_kill(spr_i(s), "die");
            }
            return 1;
        }
    }
    s->damage = 0;
    return 0;
}

static void kill_sprite(struct BrainSpr *s)
{
    s->live = 0;
    s->hidden = 1;
}

/* FreeDink live_sprite_animate (forward only). */
static int brain_animate(struct BrainSpr *s, const struct SeqInfo *seqs,
                         int now_ms)
{
    int dseq, dfr, nfr, hit = 0;

    if (s->seq < 1) {
        return 0;
    }
    if (s->frame < 1) {
        s->pseq = s->seq;
        s->pframe = 1;
        s->frame = 1;
        s->delay = now_ms + seq_delay(seqs, s->seq, 1);
        return 0;
    }
    if (s->delay == 0) {
        s->delay = now_ms + seq_delay(seqs, s->seq, s->frame);
        s->pseq = s->seq;
        s->pframe = s->frame;
        return 0;
    }
    if (now_ms <= s->delay) {
        return 0;
    }
    s->frame++;
    nfr = seq_len(seqs, s->seq);
    s->delay = now_ms + seq_delay(seqs, s->seq, s->frame);
    s->pseq = s->seq;
    s->pframe = s->frame;
    if (ini_resolve_frame(s->seq, s->frame, &dseq, &dfr) != 0 ||
        s->frame > nfr) {
        s->pseq = s->seq;
        s->pframe = s->frame - 1;
        if (s->pframe < 1) {
            s->pframe = 1;
        }
        s->frame = 0;
        s->seq_orig = s->seq;
        s->seq = 0;
        return 0;
    }
    if (ini_frame_special(s->seq, s->frame)) {
        hit = 1;
    }
    (void)dseq;
    (void)dfr;
    return hit;
}

/* FreeDink changedir. base_timing/4 is 1 at 60 Hz. */
static void changedir(int dir1, struct BrainSpr *s, int base,
                      const struct SeqInfo *seqs)
{
    int old_seq, hold, hspeed;

    if (s == NULL) {
        return;
    }
    hold = s->speed;
    if (s->brain != DINK_BRAIN_PILL && s->brain != DINK_BRAIN_DRAGON) {
        hspeed = s->speed * (DINK_BASE_TIMING / 4);
        if (hspeed > 49) {
            s->speed = 49;
        } else {
            s->speed = hspeed;
        }
    }
    old_seq = s->seq;
    s->dir = dir1;
    if (dir1 == 1) {
        s->mx = (0 - s->speed) + (s->speed / 3);
        s->my = s->speed - (s->speed / 3);
        if (base != -1) {
            s->seq = base + 1;
            if (!seq_on(seqs, s->seq)) {
                s->seq = base + 9;
            }
        }
    } else if (dir1 == 2) {
        s->mx = 0;
        s->my = s->speed;
        if (base != -1) {
            s->seq = base + 2;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 3)) {
            s->seq = base + 3;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 1)) {
            s->seq = base + 1;
        }
    } else if (dir1 == 3) {
        s->mx = s->speed - (s->speed / 3);
        s->my = s->speed - (s->speed / 3);
        if (base != -1) {
            s->seq = base + 3;
            if (!seq_on(seqs, s->seq)) {
                s->seq = base + 7;
            }
        }
    } else if (dir1 == 4) {
        s->mx = 0 - s->speed;
        s->my = 0;
        if (base != -1) {
            s->seq = base + 4;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 7)) {
            s->seq = base + 7;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 1)) {
            s->seq = base + 1;
        }
    } else if (dir1 == 6) {
        s->mx = s->speed;
        s->my = 0;
        if (base != -1) {
            s->seq = base + 6;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 3)) {
            s->seq = base + 3;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 9)) {
            s->seq = base + 9;
        }
    } else if (dir1 == 7) {
        s->mx = (0 - s->speed) + (s->speed / 3);
        s->my = (0 - s->speed) + (s->speed / 3);
        if (base != -1) {
            s->seq = base + 7;
            if (!seq_on(seqs, s->seq)) {
                s->seq = base + 3;
            }
        }
    } else if (dir1 == 8) {
        s->mx = 0;
        s->my = 0 - s->speed;
        if (base != -1) {
            s->seq = base + 8;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 7)) {
            s->seq = base + 7;
        }
        if (!seq_on(seqs, s->seq) && seq_on(seqs, base + 9)) {
            s->seq = base + 9;
        }
    } else if (dir1 == 9) {
        s->mx = s->speed - (s->speed / 3);
        s->my = (0 - s->speed) + (s->speed / 3);
        if (base != -1) {
            s->seq = base + 9;
            if (!seq_on(seqs, s->seq)) {
                s->seq = base + 1;
            }
        }
    }
    if (old_seq != s->seq) {
        s->frame = 0;
        s->delay = 0;
    }
    if (!seq_on(seqs, s->seq)) {
        s->seq = old_seq;
    }
    s->speed = hold;
}

static int automove(struct BrainSpr *s, const struct HardMask *mask)
{
    int ox, oy, i, n, dx, dy, hit = 0;

    if (s == NULL) {
        return 0;
    }
    ox = s->x;
    oy = s->y;
    n = s->mx < 0 ? -s->mx : s->mx;
    i = s->my < 0 ? -s->my : s->my;
    if (i > n) {
        n = i;
    }
    if (n < 1) {
        return 0;
    }
    dx = s->mx < 0 ? -1 : (s->mx > 0 ? 1 : 0);
    dy = s->my < 0 ? -1 : (s->my > 0 ? 1 : 0);
    for (i = 0; i < n; i++) {
        int nx = s->x, ny = s->y;

        if (dx != 0) {
            nx = s->x + dx;
        }
        if (dy != 0) {
            ny = s->y + dy;
        }
        if (mask != NULL && !s->flying && hard_get(mask, nx, ny) != 0) {
            s->x = ox;
            s->y = oy;
            hit = 1;
            break;
        }
        s->x = nx;
        s->y = ny;
        ox = s->x;
        oy = s->y;
    }
    return hit;
}

static void done_moving(struct BrainSpr *s)
{
    s->move_active = 0;
    s->move_nohard = 0;
}

/* FreeDink process_move. destination_limit is x (dirs 1/3/4/6/7/9) or y (2/8). */
static void process_move(struct BrainSpr *s, const struct SeqInfo *seqs,
                         const struct HardMask *mask)
{
    int d = s->move_dir;
    const struct HardMask *m = s->move_nohard ? NULL : mask;

    if ((d == 4 || d == 1 || d == 7) && s->x <= s->move_num) {
        done_moving(s);
        return;
    }
    if ((d == 6 || d == 9 || d == 3) && s->x >= s->move_num) {
        done_moving(s);
        return;
    }
    if (d == 2 && s->y >= s->move_num) {
        done_moving(s);
        return;
    }
    if (d == 8 && s->y <= s->move_num) {
        done_moving(s);
        return;
    }
    changedir(d, s, s->base_walk, seqs);
    (void)automove(s, m);
}

static int autoreverse(struct BrainSpr *s)
{
    int r = (rand() % 2) + 1;
    int d = s->dir;

    if (d == 1 || d == 2) {
        return r == 1 ? 8 : 6;
    }
    if (d == 3 || d == 6) {
        return r == 1 ? 2 : 4;
    }
    if (d == 9 || d == 8) {
        return r == 1 ? 2 : 6;
    }
    if (d == 7 || d == 4) {
        return r == 1 ? 8 : 6;
    }
    return 0;
}

static int autoreverse_diag(struct BrainSpr *s)
{
    int r = (rand() % 2) + 1;
    int d = s->dir;

    if (d == 0) {
        d = 7;
    }
    if (d == 1 || d == 3) {
        return r == 1 ? 9 : 7;
    }
    if (d == 3 || d == 6) {
        return r == 1 ? 7 : 1;
    }
    if (d == 9 || d == 8) {
        return r == 1 ? 1 : 7;
    }
    if (d == 7 || d == 4) {
        return r == 1 ? 3 : 9;
    }
    return 0;
}

static void no_brain(struct BrainSpr *s)
{
    if (s->freeze) {
        return;
    }
}

/* FreeDink repeat_brain. */
static void repeat_brain(struct BrainSpr *s, const struct EditorSprite *es)
{
    if (s->seq_orig == 0 && es != NULL) {
        s->seq_orig = (int)es->seq;
        s->frame = (int)es->frame;
        s->wait = 0;
    }
    if (s->seq == 0) {
        s->seq = s->seq_orig;
    }
}

static void find_action(struct BrainSpr *s, const struct SeqInfo *seqs,
                        int now_ms)
{
    int dir;

    s->action = (rand() % 2) + 1;
    if (s->action == 1) {
        s->move_wait = now_ms + (rand() % 3000) + 400;
        if (s->base_walk != -1) {
            dir = (rand() % 4) + 1;
            s->pframe = 1;
            if (dir == 1) {
                s->pseq = s->base_walk + 1;
            } else if (dir == 2) {
                s->pseq = s->base_walk + 3;
            } else if (dir == 3) {
                s->pseq = s->base_walk + 7;
            } else {
                s->pseq = s->base_walk + 9;
            }
        }
        return;
    }
    s->move_wait = now_ms + (rand() % 3000) + 500;
    dir = (rand() % 4) + 1;
    s->pframe = 1;
    if (dir == 1) {
        changedir(1, s, s->base_walk, seqs);
    } else if (dir == 2) {
        changedir(3, s, s->base_walk, seqs);
    } else if (dir == 3) {
        changedir(7, s, s->base_walk, seqs);
    } else {
        changedir(9, s, s->base_walk, seqs);
    }
}

/* FreeDink people_brain (damage 15.2). */
static void people_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                         const struct HardMask *mask, int now_ms)
{
    if (apply_hp_kill(s, seqs)) {
        if (s->brain == DINK_BRAIN_PEOPLE) {
            if (s->dir == 0) {
                s->dir = 3;
            }
            s->brain = 0;
            change_dir_to_diag(&s->dir);
            add_kill_sprite(s, seqs);
            kill_sprite(s);
        }
        return;
    }
    if (s->freeze) {
        return;
    }
    if (s->move_wait < now_ms && s->seq == 0) {
        s->action = 0;
    }
    if (s->action == 0) {
        find_action(s, seqs, now_ms);
    }
    if (s->action != 2) {
        s->seq = 0;
        return;
    }
    if (s->seq_orig != 0 && s->seq == 0) {
        s->seq = s->seq_orig;
    }
    if (s->y > DINK_PLAYY) {
        changedir(((rand() % 2) + 1) == 1 ? 9 : 7, s, s->base_walk, seqs);
    }
    if (s->x > DINK_PLAYX) {
        changedir(((rand() % 2) + 1) == 1 ? 1 : 7, s, s->base_walk, seqs);
    }
    if (s->y < 20) {
        changedir(((rand() % 2) + 1) == 1 ? 1 : 3, s, s->base_walk, seqs);
    }
    if (s->x < 30) {
        changedir(((rand() % 2) + 1) == 1 ? 3 : 9, s, s->base_walk, seqs);
    }
    if (automove(s, mask) != 0) {
        if ((rand() % 3) == 2) {
            changedir(autoreverse_diag(s), s, s->base_walk, seqs);
        } else {
            s->move_wait = 0;
            s->pframe = 1;
            s->seq = 0;
        }
    }
}

/* FreeDink bounce_brain. getpic box is 0 (off-screen a little). */
static void bounce_brain(struct BrainSpr *s)
{
    if (s->y > DINK_PLAYY) {
        s->my -= s->my * 2;
    }
    if (s->x > DINK_GFX_W) {
        s->mx -= s->mx * 2;
    }
    if (s->y < 0) {
        s->my -= s->my * 2;
    }
    if (s->x < 0) {
        s->mx -= s->mx * 2;
    }
    s->x += s->mx;
    s->y += s->my;
}

/* FreeDink duck_brain (damage 15.2; no sfx). */
static void duck_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                       const struct HardMask *mask, int now_ms)
{
    int hold;

    if (s->damage > 0 && in_this_base(s->pseq, 110)) {
        if (g_on_kill != NULL) {
            g_on_kill(spr_i(s), "duckdie");
        }
        (void)brains_create(s->x, s->y, 7, 164, 1);
        draw_damage(s, seqs);
        add_exp_if_player(s);
        s->damage = 0;
        kill_sprite(s);
        return;
    }
    if (s->damage > 0 && in_this_base(s->pseq, s->base_walk)) {
        int head;

        draw_damage(s, seqs);
        add_exp_if_player(s);
        s->damage = 0;
        if (g_on_kill != NULL) {
            g_on_kill(spr_i(s), "die");
        }
        /* FreeDink: flying-head sprite brain 5, base_walk 120. */
        head = brains_create(s->x, s->y, DINK_BRAIN_ONETIME, 1, 1);
        s->base_walk = 110;
        s->speed = 1;
        s->timing = 0;
        s->frame = 0;
        if (s->dir == 0) {
            s->dir = 1;
        }
        if (s->dir == 4) {
            s->dir = 7;
        }
        if (s->dir == 6) {
            s->dir = 3;
        }
        changedir(s->dir, s, s->base_walk, seqs);
        if (head > 0) {
            g_b[head].speed = (rand() % 3) + 1;
            g_b[head].size = s->size;
            g_b[head].dir = s->dir;
            g_b[head].base_walk = 120;
            changedir(g_b[head].dir, &g_b[head], 120, seqs);
        }
        (void)automove(s, mask);
        return;
    }
    if (s->freeze) {
        return;
    }
    if (s->seq == 0) {
        if ((rand() % 12) + 1 == 1) {
            hold = (rand() % 9) + 1;
            if (hold != 2 && hold != 8 && hold != 5) {
                changedir(hold, s, s->base_walk, seqs);
            } else {
                s->mx = 0;
                s->my = 0;
                s->wait = now_ms + (rand() % 300) + 200;
            }
            return;
        }
        if (s->mx != 0 || s->my != 0) {
            s->seq = s->seq_orig;
        }
    }
    if (s->y > DINK_PLAYY) {
        changedir(9, s, s->base_walk, seqs);
    }
    if (s->x > DINK_PLAYX - 30) {
        changedir(7, s, s->base_walk, seqs);
    }
    if (s->y < 10) {
        changedir(1, s, s->base_walk, seqs);
    }
    if (s->x < 30) {
        changedir(3, s, s->base_walk, seqs);
    }
    if (automove(s, mask) != 0 && s->dir != 0) {
        changedir(autoreverse_diag(s), s, s->base_walk, seqs);
    }
}

/* FreeDink pig_brain (damage 15.2; no sfx). */
static void pig_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                      const struct HardMask *mask, int now_ms)
{
    int hold;

    if (s->damage > 0) {
        draw_damage(s, seqs);
        s->hitpoints -= s->damage;
        s->damage = 0;
        if (s->hitpoints < 1) {
            add_exp_if_player(s);
            if (g_on_kill != NULL) {
                g_on_kill(spr_i(s), "die");
            }
            s->speed = 0;
            s->base_walk = -1;
            s->seq = 164;
            s->brain = DINK_BRAIN_ONETIME_STAY;
        }
        return;
    }
    if (s->freeze) {
        return;
    }
    if (s->seq == 0) {
        if ((rand() % 12) + 1 == 1) {
            hold = (rand() % 9) + 1;
            if (hold != 4 && hold != 6 && hold != 2 && hold != 8 &&
                hold != 5) {
                changedir(hold, s, s->base_walk, seqs);
            } else {
                s->mx = 0;
                s->my = 0;
                s->wait = now_ms + (rand() % 300) + 200;
            }
        } else if (s->mx != 0 || s->my != 0) {
            s->seq = s->seq_orig;
        }
    }
    if (s->y > DINK_PLAYY) {
        changedir(9, s, s->base_walk, seqs);
    }
    if (s->x > DINK_GFX_W - 10) {
        changedir(1, s, s->base_walk, seqs);
    }
    if (s->y < 10) {
        changedir(1, s, s->base_walk, seqs);
    }
    if (s->x < 10) {
        changedir(3, s, s->base_walk, seqs);
    }
    if (automove(s, mask) != 0) {
        changedir(autoreverse_diag(s), s, s->base_walk, seqs);
    }
}

/* 5: play once then keep last frame (bg blit). 7: play once then remove. */
static void one_time_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                           const struct HardMask *mask, int stay)
{
    if (s->seq == 0 && s->seq_orig == 0 && s->pseq > 0) {
        s->seq = s->pseq;
        return;
    }
    if (s->seq == 0) {
        if (!stay) {
            s->live = 0;
        }
        return;
    }
    if (s->dir > 0) {
        changedir(s->dir, s, -1, seqs);
        (void)automove(s, mask);
    }
}

/* FreeDink pill_brain (damage 15.2). */
static void pill_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                       const struct HardMask *mask, int now_ms)
{
    int hold;

    if (apply_hp_kill(s, seqs)) {
        if (s->brain == DINK_BRAIN_PILL) {
            if (s->dir == 0) {
                s->dir = 3;
            }
            change_dir_to_diag(&s->dir);
            add_kill_sprite(s, seqs);
            kill_sprite(s);
        }
        return;
    }
    if (s->freeze) {
        return;
    }
    if (s->base_walk != -1 && s->seq == 0) {
        goto recal;
    }
    if (s->seq == 0 && s->move_wait < now_ms) {
    recal:
        if ((rand() % 12) + 1 == 1) {
            hold = (rand() % 9) + 1;
            if (hold != 4 && hold != 6 && hold != 2 && hold != 8 &&
                hold != 5) {
                changedir(hold, s, s->base_walk, seqs);
                s->move_wait = now_ms + (rand() % 2000) + 200;
            }
        } else {
            s->seq = s->seq_orig;
            if (s->seq_orig == 0) {
                goto recal;
            }
        }
    }
    if (s->y > DINK_PLAYY - 15) {
        changedir(9, s, s->base_walk, seqs);
    }
    if (s->x > DINK_PLAYX - 15) {
        changedir(1, s, s->base_walk, seqs);
    }
    if (s->y < 18) {
        changedir(1, s, s->base_walk, seqs);
    }
    if (s->x < 18) {
        changedir(3, s, s->base_walk, seqs);
    }
    if (automove(s, mask) != 0) {
        s->move_wait = now_ms + 400;
        changedir(autoreverse_diag(s), s, s->base_walk, seqs);
    }
}

/* FreeDink dragon_brain (damage 15.2; no ATTACK script). */
static void dragon_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                         const struct HardMask *mask)
{
    int hold;

    if (apply_hp_kill(s, seqs)) {
        if (s->brain == DINK_BRAIN_DRAGON) {
            add_kill_sprite(s, seqs);
            kill_sprite(s);
        }
        return;
    }
    if (s->freeze) {
        return;
    }
    if (s->seq == 0) {
    recal:
        if ((rand() % 12) + 1 == 1) {
            hold = (rand() % 9) + 1;
            if (hold != 1 && hold != 3 && hold != 7 && hold != 9 &&
                hold != 5) {
                changedir(hold, s, s->base_walk, seqs);
            }
        } else {
            s->seq = s->seq_orig;
            if (s->seq_orig == 0) {
                goto recal;
            }
        }
    }
    if (s->y > DINK_PLAYY) {
        changedir(8, s, s->base_walk, seqs);
    }
    if (s->x > DINK_GFX_W) {
        changedir(4, s, s->base_walk, seqs);
    }
    if (s->y < 0) {
        changedir(2, s, s->base_walk, seqs);
    }
    if (s->x < 0) {
        changedir(6, s, s->base_walk, seqs);
    }
    if (automove(s, mask) != 0) {
        changedir(autoreverse(s), s, s->base_walk, seqs);
    }
}

/* FreeDink missile_brain: k[getpic(j)].hardbox + offset, then range inflate.
 * ini_frame_geom is get_box without a decoded frame (same fallback as hit.c). */
static void geom_hardbox(const struct SeqInfo *seqs, int seq, int fr, int x,
                         int y, int range, int *l, int *t, int *r, int *b)
{
    int hl, ht, hr, hb, cx, cy;

    if (fr < 1) {
        fr = 1;
    }
    if (seqs != NULL && seq > 0 && seq < DINK_MAX_SEQ) {
        ini_frame_geom(&seqs[seq], seq, fr, 50, 50, &cx, &cy, &hl, &ht, &hr,
                       &hb);
    } else {
        hl = ht = -10;
        hr = hb = 10;
    }
    *l = x + hl;
    *t = y + ht;
    *r = x + hr;
    *b = y + hb;
    if (range != 0) {
        *l -= range;
        *t -= range;
        *r += range;
        *b += range;
    }
}

static int spr_draw_seq(const struct BrainSpr *s)
{
    if (s->seq > 0) {
        return s->seq;
    }
    return s->pseq;
}

static int spr_draw_frame(const struct BrainSpr *s)
{
    if (s->frame > 0) {
        return s->frame;
    }
    return s->pframe;
}

static void missile_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                          const struct HardMask *mask, int repeat)
{
    int hit, j, h;

    hit = automove(s, mask);
    if (repeat && s->seq == 0) {
        s->seq = s->seq_orig;
    }
    if (hit) {
        s->live = 0;
        return;
    }
    if (s->x > 1000 || s->y > 700 || s->y < -500 || s->x < -500) {
        s->live = 0;
        return;
    }
    h = spr_i(s);
    if (g_pl != NULL && g_pl->nohit != 1) {
        int l, t, r, b;

        geom_hardbox(seqs, g_pl->seq, g_pl->frame, g_pl->x, g_pl->y, s->range,
                     &l, &t, &r, &b);
        if (s->x >= l && s->x <= r && s->y >= t && s->y <= b &&
            s->strength != 0) {
            g_pl->last_hit = h;
            (void)player_hurt(g_pl, s->strength);
            s->live = 0;
            return;
        }
    }
    for (j = 1; j <= 99; j++) {
        int l, t, r, b;

        if (j == h || !g_b[j].live || g_b[j].nohit == 1) {
            continue;
        }
        if (s->brain_parm == j) {
            continue;
        }
        geom_hardbox(seqs, spr_draw_seq(&g_b[j]), spr_draw_frame(&g_b[j]),
                     g_b[j].x, g_b[j].y, s->range, &l, &t, &r, &b);
        if (s->x < l || s->x > r || s->y < t || s->y > b) {
            continue;
        }
        if (g_b[j].hitpoints > 0 && s->strength != 0) {
            int hit, half;

            /* FreeDink missile_brain, not hurt_thing. last_hit is Dink. */
            if ((rand() % 2) + 1 == 1) {
                hit = s->strength - g_b[j].defense;
            } else {
                half = s->strength / 2;
                if (half < 1) {
                    half = 1;
                }
                hit = half + ((rand() % half) + 1) - g_b[j].defense;
            }
            if (hit < 0) {
                hit = 0;
            }
            g_b[j].last_hit = 1;
            g_b[j].damage += hit;
        }
        s->live = 0;
        break;
    }
}

static void scale_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                        const struct HardMask *mask)
{
    int num = 5 * (DINK_BASE_TIMING / 4);

    if (s->size == s->brain_parm) {
        s->live = 0;
        s->hidden = 1;
        return;
    }
    if (s->size > s->brain_parm) {
        if (s->size - num < s->brain_parm) {
            num = s->size - s->brain_parm;
        }
        s->size -= num;
    }
    if (s->size < s->brain_parm) {
        if (s->size + num > s->brain_parm) {
            num = s->brain_parm - s->size;
        }
        s->size += num;
    }
    if (s->dir > 0) {
        changedir(s->dir, s, -1, seqs);
        (void)automove(s, mask);
    }
}

static void shadow_brain(struct BrainSpr *s)
{
    struct BrainSpr *o;

    if (s->brain_parm < 1 || s->brain_parm > 100) {
        s->live = 0;
        return;
    }
    o = &g_b[s->brain_parm];
    if (!o->live) {
        s->live = 0;
        return;
    }
    s->x = o->x;
    s->y = o->y;
    s->size = o->size;
    if (s->seq == 0 && s->seq_orig != 0) {
        s->seq = s->seq_orig;
    }
}

static void log_unimpl(struct BrainSpr *s)
{
    if (s->logged) {
        return;
    }
    s->logged = 1;
    g_unimpl++;
    printf("brain unimplemented: %d\n", s->brain);
}

static void brain_switch(struct BrainSpr *s, const struct EditorSprite *es,
                         const struct SeqInfo *seqs, const struct HardMask *mask,
                         int now_ms)
{
    int b = s->brain;

    if (b == DINK_BRAIN_PLAYER) {
        return;
    }
    if (b == DINK_BRAIN_NONE) {
        no_brain(s);
        return;
    }
    if (b == DINK_BRAIN_BOUNCE) {
        bounce_brain(s);
        return;
    }
    if (b == DINK_BRAIN_DUCK) {
        duck_brain(s, seqs, mask, now_ms);
        return;
    }
    if (b == DINK_BRAIN_PIG) {
        pig_brain(s, seqs, mask, now_ms);
        return;
    }
    if (b == DINK_BRAIN_ONETIME) {
        one_time_brain(s, seqs, mask, 1);
        return;
    }
    if (b == DINK_BRAIN_REPEAT) {
        repeat_brain(s, es);
        return;
    }
    if (b == DINK_BRAIN_ONETIME_STAY) {
        one_time_brain(s, seqs, mask, 0);
        return;
    }
    if (b == DINK_BRAIN_TEXT) {
        /* FreeDink text_brain: say follow is saybox; damage/exp float up. */
        if (s->damage != -1) {
            s->x += s->mx;
            s->y += s->my;
            if (s->y < 0) {
                s->y = 0;
            }
        }
        return;
    }
    if (b == DINK_BRAIN_PILL) {
        pill_brain(s, seqs, mask, now_ms);
        return;
    }
    if (b == DINK_BRAIN_DRAGON) {
        dragon_brain(s, seqs, mask);
        return;
    }
    if (b == DINK_BRAIN_MISSILE) {
        missile_brain(s, seqs, mask, 1);
        return;
    }
    if (b == DINK_BRAIN_SCALE) {
        scale_brain(s, seqs, mask);
        return;
    }
    if (b == DINK_BRAIN_SHADOW) {
        shadow_brain(s);
        return;
    }
    if (b == DINK_BRAIN_PEOPLE) {
        people_brain(s, seqs, mask, now_ms);
        return;
    }
    if (b == DINK_BRAIN_MISS_EXPIRE) {
        missile_brain(s, seqs, mask, 0);
        if (s->seq == 0) {
            s->live = 0;
        }
        return;
    }
    if (b == DINK_BRAIN_MOUSE || b == DINK_BRAIN_BUTTON) {
        log_unimpl(s);
        return;
    }
    log_unimpl(s);
}

void brains_bind_screen(const struct MapScreen *scr)
{
    g_map = scr;
}

void brains_reset(void)
{
    memset(g_b, 0, sizeof(g_b));
    g_unimpl = 0;
}

int brains_live_xy(int slot, int *x, int *y)
{
    if (slot < 1 || slot > 99 || !g_b[slot].live || x == NULL || y == NULL) {
        return 0;
    }
    *x = g_b[slot].x;
    *y = g_b[slot].y;
    return 1;
}

int brains_slot_live(int slot)
{
    return slot >= 1 && slot <= 100 && g_b[slot].live;
}

int brains_slot_seq(int slot)
{
    if (!brains_slot_live(slot)) {
        return 0;
    }
    return g_b[slot].seq;
}

int brains_slot_created(int slot)
{
    return slot >= 2 && slot <= 99 && g_b[slot].live && g_b[slot].created;
}

int brains_slot_pseq(int slot)
{
    if (!brains_slot_created(slot)) {
        return 0;
    }
    return g_b[slot].pseq;
}

int brains_slot_base_walk(int slot)
{
    if (!brains_slot_created(slot)) {
        return -1;
    }
    return g_b[slot].base_walk;
}

int brains_slot_brain(int slot)
{
    if (!brains_slot_created(slot)) {
        return -1;
    }
    return g_b[slot].brain;
}

void brains_set_script(int slot, const char *name)
{
    if (slot < 1 || slot > 99 || name == NULL) {
        return;
    }
    strncpy(g_b[slot].script, name, sizeof(g_b[slot].script) - 1u);
    g_b[slot].script[sizeof(g_b[slot].script) - 1u] = '\0';
}

void brains_enter(const struct MapScreen *scr, int vision)
{
    int i;

    g_unimpl = 0;
    if (scr != NULL) {
        g_map = scr;
    }
    /* Keep create_sprite from screen MAIN (draw_screen_game order). */
    for (i = 1; i <= 100; i++) {
        if (g_b[i].live && g_b[i].created) {
            continue;
        }
        memset(&g_b[i], 0, sizeof(g_b[i]));
    }
    if (scr == NULL) {
        return;
    }
    for (i = 1; i <= 100; i++) {
        const struct EditorSprite *es = &scr->sprite[i];

        if (g_b[i].live) {
            continue;
        }
        if (es->type != 1 || !editor_sprite_on_vision(es, vision)) {
            continue;
        }
        g_b[i].live = 1;
        g_b[i].x = (int)es->x;
        g_b[i].y = (int)es->y;
        g_b[i].brain = (int)es->brain;
        g_b[i].speed = (int)es->speed;
        g_b[i].base_walk = (int)es->base_walk;
        g_b[i].timing = (int)es->timing;
        g_b[i].size = (int)es->size;
        if (g_b[i].size < 1) {
            g_b[i].size = 100;
        }
        g_b[i].pseq = (int)es->seq;
        g_b[i].pframe = (int)es->frame < 1 ? 1 : (int)es->frame;
        /* add_sprite_dumb: seq=0, frame=0; base_* = -1 */
        g_b[i].seq = 0;
        g_b[i].frame = 0;
        g_b[i].seq_orig = 0;
        g_b[i].base_idle = -1;
        g_b[i].base_attack = -1;
        g_b[i].base_die = (int)es->base_die;
        if (g_b[i].base_die == 0) {
            g_b[i].base_die = -1;
        }
        g_b[i].hitpoints = (int)es->hitpoints;
        g_b[i].defense = (int)es->defense;
        g_b[i].strength = (int)es->strength;
        g_b[i].exp = (int)es->exp;
        g_b[i].nohit = (int)es->nohit;
        g_b[i].touch_damage = (int)es->touch_damage;
        g_b[i].hard = (int)es->hard;
        g_b[i].created = 0;
        g_b[i].hidden = 0;
    }
}

void brains_apply(struct MapScreen *scr)
{
    int i;

    if (scr == NULL) {
        return;
    }
    for (i = 1; i <= 100; i++) {
        struct BrainSpr *s = &g_b[i];

        if (s->hidden && !s->live) {
            scr->sprite[i].active = 0;
            continue;
        }
        if (!s->live) {
            continue;
        }
        if (s->created) {
            scr->sprite[i].active = 1;
            scr->sprite[i].type = 1;
            scr->sprite[i].vision = 0;
            scr->sprite[i].brain = s->brain;
            if (s->script[0] != '\0') {
                strncpy(scr->sprite[i].script, s->script,
                        sizeof(scr->sprite[i].script) - 1u);
                scr->sprite[i].script[sizeof(scr->sprite[i].script) - 1u] =
                    '\0';
            }
        }
        scr->sprite[i].x = s->x;
        scr->sprite[i].y = s->y;
        if (s->pseq > 0) {
            scr->sprite[i].seq = s->pseq;
        }
        if (s->pframe > 0) {
            scr->sprite[i].frame = s->pframe;
        }
        scr->sprite[i].size = s->size;
        scr->sprite[i].hard = s->hard;
    }
}

void brains_tick(struct MapScreen *scr, const struct SeqInfo *seqs,
                 const struct HardMask *mask, int now_ms, int vision)
{
    int i;

    (void)vision;
    if (scr == NULL) {
        return;
    }
    for (i = 1; i <= 100; i++) {
        struct BrainSpr *s = &g_b[i];
        const struct EditorSprite *es = &scr->sprite[i];

        if (!s->live) {
            continue;
        }
        if (s->kill_ttl > 0) {
            if (s->kill_start == 0) {
                s->kill_start = now_ms;
            }
            if (now_ms > s->kill_start + s->kill_ttl) {
                s->live = 0;
                s->hidden = 1;
                continue;
            }
        }
        if (s->move_active) {
            process_move(s, seqs, mask);
            (void)brain_animate(s, seqs, now_ms);
            continue;
        }
        /* update_frame timing: skip AI, still animate */
        if (s->timing > 0) {
            if (now_ms > s->wait || s->wait == 0) {
                s->wait = now_ms + s->timing;
            } else {
                s->just_hit = brain_animate(s, seqs, now_ms);
                continue;
            }
        }
        brain_switch(s, es, seqs, mask, now_ms);
        s->just_hit = brain_animate(s, seqs, now_ms);
    }
    brains_apply(scr);
}

void brains_set_freeze(int slot, int on)
{
    if (slot < 1 || slot > 100) {
        return;
    }
    g_b[slot].freeze = on ? 1 : 0;
}

int brains_freeze(int slot)
{
    if (slot < 1 || slot > 100) {
        return 0;
    }
    return g_b[slot].freeze;
}

int brains_change_prop(int slot, int prop, int val)
{
    struct BrainSpr *s;
    int *p = NULL;

    if (slot < 1 || slot > 100) {
        return -1;
    }
    s = &g_b[slot];
    if (prop == DINKC_SP_KILL) {
        s->kill_ttl = val;
        s->kill_start = 0;
        return val;
    }
    if (!s->live && val != -1 && prop != DINKC_SP_ACTIVE) {
        /* change_sprite on inactive: -1 */
        return -1;
    }
    if (prop == 1) {
        p = &s->brain;
    } else if (prop == 2) {
        p = &s->speed;
    } else if (prop == 3) {
        p = &s->base_walk;
    } else if (prop == 4) {
        p = &s->timing;
    } else if (prop == DINKC_SP_X) {
        p = &s->x;
    } else if (prop == DINKC_SP_Y) {
        p = &s->y;
    } else if (prop == DINKC_SP_DIR) {
        p = &s->dir;
    } else if (prop == DINKC_SP_SEQ) {
        p = &s->seq;
    } else if (prop == DINKC_SP_FRAME) {
        p = &s->frame;
    } else if (prop == DINKC_SP_BASE_ATTACK) {
        p = &s->base_attack;
    } else if (prop == DINKC_SP_BASE_IDLE) {
        p = &s->base_idle;
    } else if (prop == DINKC_SP_PSEQ) {
        p = &s->pseq;
    } else if (prop == DINKC_SP_PFRAME) {
        p = &s->pframe;
    } else if (prop == DINKC_SP_ACTIVE) {
        if (val == -1) {
            return s->live ? 1 : 0;
        }
        if (val == 0) {
            s->live = 0;
            s->hidden = 1;
            return 0;
        }
        s->live = 1;
        s->hidden = 0;
        return 1;
    } else if (prop == DINKC_SP_HITPOINTS) {
        p = &s->hitpoints;
    } else if (prop == DINKC_SP_DEFENSE) {
        p = &s->defense;
    } else if (prop == DINKC_SP_EXP) {
        p = &s->exp;
    } else if (prop == DINKC_SP_BASE_DIE) {
        p = &s->base_die;
    } else if (prop == DINKC_SP_NOHIT) {
        p = &s->nohit;
    } else if (prop == DINKC_SP_STRENGTH) {
        p = &s->strength;
    } else if (prop == DINKC_SP_RANGE) {
        p = &s->range;
    } else if (prop == DINKC_SP_TOUCH) {
        p = &s->touch_damage;
    } else if (prop == DINKC_SP_MX) {
        p = &s->mx;
    } else if (prop == DINKC_SP_MY) {
        p = &s->my;
    } else if (prop == DINKC_SP_FLYING) {
        p = &s->flying;
    } else if (prop == DINKC_SP_BRAIN_PARM) {
        p = &s->brain_parm;
    } else if (prop == DINKC_SP_BRAIN_PARM2) {
        p = &s->brain_parm2;
    } else if (prop == DINKC_SP_QUE) {
        p = &s->que;
    } else if (prop == DINKC_SP_SIZE) {
        p = &s->size;
    } else if (prop == DINKC_SP_HARD) {
        p = &s->hard;
    } else if (prop == DINKC_SP_NOTOUCH) {
        p = &s->notouch;
    }
    if (p == NULL) {
        return -1;
    }
    /* change_sprite_noreturn: touch/mx/my may be -1 (pickup, missile). */
    if (prop == DINKC_SP_TOUCH || prop == DINKC_SP_MX || prop == DINKC_SP_MY) {
        *p = val;
        return *p;
    }
    if (val != -1) {
        *p = val;
        s->live = 1;
        if (prop == DINKC_SP_SEQ && val > 0) {
            s->frame = 0;
        }
        if (prop == DINKC_SP_DIR && s->base_walk > 0) {
            /* changedir needs seqs; seq = base_walk+dir is applied on tick. */
            s->seq = s->base_walk + val;
            s->frame = 0;
        }
        if ((prop == DINKC_SP_DIR || prop == DINKC_SP_SPEED) &&
            s->brain == DINK_BRAIN_MISSILE && s->speed != 0 && s->mx == 0 &&
            s->my == 0) {
            changedir(s->dir < 1 ? 6 : s->dir, s, -1, NULL);
        }
    }
    return *p;
}

int brains_create(int x, int y, int brain, int pseq, int pframe)
{
    int i;

    for (i = 2; i <= 99; i++) {
        if (g_b[i].live) {
            continue;
        }
        /* add_sprite_dumb skips spr[].active. Type 0 stays in g_scr. */
        if (g_map != NULL && g_map->sprite[i].active) {
            continue;
        }
        memset(&g_b[i], 0, sizeof(g_b[i]));
        g_b[i].live = 1;
        g_b[i].created = 1;
        g_b[i].hidden = 0;
        g_b[i].x = x;
        g_b[i].y = y;
        g_b[i].brain = brain;
        g_b[i].pseq = pseq;
        g_b[i].pframe = pframe < 1 ? 1 : pframe;
        g_b[i].seq = 0;
        g_b[i].frame = 0;
        g_b[i].size = 100;
        g_b[i].base_walk = -1;
        g_b[i].base_idle = -1;
        g_b[i].base_attack = -1;
        g_b[i].base_die = -1;
        return i;
    }
    return 0;
}

int brains_move(int slot, int dir, int dest, int nohard)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return -1;
    }
    g_b[slot].move_active = 1;
    g_b[slot].move_dir = dir;
    g_b[slot].move_num = dest;
    g_b[slot].move_nohard = nohard ? 1 : 0;
    return 1;
}

int brains_moving(int slot)
{
    if (slot < 1 || slot > 100) {
        return 0;
    }
    return g_b[slot].move_active;
}

int brains_unimpl_count(void)
{
    return g_unimpl;
}

void brains_bind_kill(void (*fn)(int slot, const char *proc))
{
    g_on_kill = fn;
}

void brains_bind_exp(void (*fn)(int num))
{
    g_on_exp = fn;
}

void brains_bind_player(struct Player *p)
{
    g_pl = p;
}

int brains_hurt(int slot, int damage)
{
    int num;

    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    num = hurt_roll(damage, g_b[slot].defense);
    g_b[slot].damage += num;
    return num;
}

int brains_hitpoints(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].hitpoints;
}

int brains_defense(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].defense;
}

int brains_strength(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].strength;
}

int brains_range(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].range;
}

int brains_nohit(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].nohit;
}

int brains_exp(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].exp;
}

int brains_base_attack(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return -1;
    }
    return g_b[slot].base_attack;
}

int brains_touch_damage(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].touch_damage;
}

void brains_set_last_hit(int slot, int who)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return;
    }
    g_b[slot].last_hit = who;
}

void brains_set_target(int slot, int who)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return;
    }
    g_b[slot].target = who;
}

int brains_take_just_hit(int slot)
{
    int hit;

    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    hit = g_b[slot].just_hit;
    g_b[slot].just_hit = 0;
    return hit;
}

int brains_seq_frame(int slot, int *seq, int *frame)
{
    int sq, fr;

    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    sq = g_b[slot].seq > 0 ? g_b[slot].seq : g_b[slot].pseq;
    fr = g_b[slot].frame > 0 ? g_b[slot].frame : g_b[slot].pframe;
    if (fr < 1) {
        fr = 1;
    }
    if (seq != NULL) {
        *seq = sq;
    }
    if (frame != NULL) {
        *frame = fr;
    }
    return 1;
}

int brains_slot_size(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 100;
    }
    if (g_b[slot].size < 1) {
        return 100;
    }
    return g_b[slot].size;
}

int brains_slot_hard(int slot)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    return g_b[slot].hard;
}

int brains_floater_num(int slot, int *x, int *y, int *num)
{
    if (slot < 1 || slot > 100 || !g_b[slot].live) {
        return 0;
    }
    if (g_b[slot].brain != DINK_BRAIN_TEXT || g_b[slot].damage < 0) {
        return 0;
    }
    if (x != NULL) {
        *x = g_b[slot].x;
    }
    if (y != NULL) {
        *y = g_b[slot].y;
    }
    if (num != NULL) {
        *num = g_b[slot].damage;
    }
    return 1;
}
