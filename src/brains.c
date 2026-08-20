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
#define DINK_BRAIN_PEOPLE 16

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
    if (s->brain != DINK_BRAIN_PILL && s->brain != 10) {
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

static void one_time_brain(struct BrainSpr *s)
{
    if (s->seq == 0) {
        s->live = 0;
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
    if (b == DINK_BRAIN_REPEAT) {
        repeat_brain(s, es);
        return;
    }
    if (b == DINK_BRAIN_ONETIME || b == DINK_BRAIN_ONETIME_STAY) {
        one_time_brain(s);
        return;
    }
    if (b == DINK_BRAIN_TEXT) {
        return;
    }
    if (b == DINK_BRAIN_PEOPLE) {
        people_brain(s, seqs, mask, now_ms);
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

int brains_unimpl_count(void)
{
    return g_unimpl;
}
