/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "brains.h"

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
    int brain;
    int logged;
};

static struct BrainSpr g_b[101];
static int g_unimpl;

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
        if (mask != NULL && hard_get(mask, nx, ny) != 0) {
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

/* FreeDink people_brain without damage (15.2). */
static void people_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                         const struct HardMask *mask, int now_ms)
{
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

/* FreeDink duck_brain without damage / sfx / follow (15.2 / 12). */
static void duck_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                       const struct HardMask *mask, int now_ms)
{
    int hold;

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

/* FreeDink pig_brain without damage / sfx. */
static void pig_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                      const struct HardMask *mask, int now_ms)
{
    int hold;

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

/* FreeDink pill_brain walk_normal only (target/hit is 15.2). */
static void pill_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                       const struct HardMask *mask, int now_ms)
{
    int hold;

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

/* FreeDink dragon_brain without damage / ATTACK script. */
static void dragon_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                         const struct HardMask *mask)
{
    int hold;

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

static void missile_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                          const struct HardMask *mask, int repeat)
{
    int hit;

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
    }
    (void)seqs;
}

static void scale_brain(struct BrainSpr *s, const struct SeqInfo *seqs,
                        const struct HardMask *mask)
{
    int num = 5 * (DINK_BASE_TIMING / 4);

    if (s->size == s->brain_parm) {
        s->live = 0;
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

void brains_enter(const struct MapScreen *scr, int vision)
{
    int i;

    memset(g_b, 0, sizeof(g_b));
    g_unimpl = 0;
    if (scr == NULL) {
        return;
    }
    for (i = 1; i <= 100; i++) {
        const struct EditorSprite *es = &scr->sprite[i];

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
        g_b[i].pseq = (int)es->seq;
        g_b[i].pframe = (int)es->frame < 1 ? 1 : (int)es->frame;
        /* add_sprite_dumb: seq=0, frame=0 */
        g_b[i].seq = 0;
        g_b[i].frame = 0;
        g_b[i].seq_orig = 0;
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

        if (!s->live) {
            continue;
        }
        scr->sprite[i].x = s->x;
        scr->sprite[i].y = s->y;
        if (s->pseq > 0) {
            scr->sprite[i].seq = s->pseq;
        }
        if (s->pframe > 0) {
            scr->sprite[i].frame = s->pframe;
        }
    }
}

void brains_tick(struct MapScreen *scr, const struct SeqInfo *seqs,
                 const struct HardMask *mask, int now_ms, int vision)
{
    int i;

    if (scr == NULL) {
        return;
    }
    for (i = 1; i <= 100; i++) {
        struct BrainSpr *s = &g_b[i];
        const struct EditorSprite *es = &scr->sprite[i];

        if (!s->live) {
            continue;
        }
        if (!editor_sprite_on_vision(es, vision) && es->type == 1) {
            /* vision can hide a live sprite; keep state */
        }
        /* update_frame timing: skip AI, still animate */
        if (s->timing > 0) {
            if (now_ms > s->wait || s->wait == 0) {
                s->wait = now_ms + s->timing;
            } else {
                (void)brain_animate(s, seqs, now_ms);
                continue;
            }
        }
        brain_switch(s, es, seqs, mask, now_ms);
        (void)brain_animate(s, seqs, now_ms);
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
    if (prop == 1) {
        p = &s->brain;
    } else if (prop == 2) {
        p = &s->speed;
    } else if (prop == 3) {
        p = &s->base_walk;
    } else if (prop == 4) {
        p = &s->timing;
    }
    if (p == NULL) {
        return -1;
    }
    if (val != -1) {
        *p = val;
        s->live = 1;
    }
    return *p;
}

int brains_unimpl_count(void)
{
    return g_unimpl;
}
