/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "player.h"

#include "start_map.h"

static void dir_delta(int dir, int *dx, int *dy)
{
    *dx = 0;
    *dy = 0;
    if (dir == 1 || dir == 4 || dir == 7) {
        *dx = -1;
    }
    if (dir == 3 || dir == 6 || dir == 9) {
        *dx = 1;
    }
    if (dir == 1 || dir == 2 || dir == 3) {
        *dy = 1;
    }
    if (dir == 7 || dir == 8 || dir == 9) {
        *dy = -1;
    }
}

void player_init(struct Player *p)
{
    if (p == NULL) {
        return;
    }
    p->x = DINK_START_X;
    p->y = DINK_START_Y;
    p->dir = DINK_START_DIR;
    p->seq = DINK_BASE_IDLE + DINK_START_DIR;
    p->frame = 1;
    p->acc = 0;
}

void player_seq_for_input(const struct Player *p, int pad_dir, int *seq,
                          int *nframes_hint)
{
    (void)nframes_hint;
    if (p == NULL || seq == NULL) {
        return;
    }
    if (pad_dir == 0) {
        *seq = DINK_BASE_IDLE + p->dir;
    } else {
        *seq = DINK_BASE_WALK + pad_dir;
    }
}

void player_step(struct Player *p, int pad_dir, const struct HardMask *mask,
                 const struct SeqInfo *seqs)
{
    int seq, delay, nfr, nx, ny, dx, dy;
    int hl, ht, hr, hb;

    if (p == NULL || seqs == NULL) {
        return;
    }
    if (pad_dir != 0) {
        p->dir = pad_dir;
    }
    player_seq_for_input(p, pad_dir, &seq, NULL);
    if (seq != p->seq) {
        p->seq = seq;
        p->frame = 1;
        p->acc = 0;
    }
    if (seq < 1 || seq >= DINK_MAX_SEQ) {
        return;
    }
    hl = seqs[seq].hl;
    ht = seqs[seq].ht;
    hr = seqs[seq].hr;
    hb = seqs[seq].hb;
    if (hr <= hl) {
        hl = -11;
        hr = 11;
        ht = -9;
        hb = 9;
    }
    if (pad_dir != 0) {
        dir_delta(pad_dir, &dx, &dy);
        nx = p->x + dx * DINK_SPEED;
        ny = p->y + dy * DINK_SPEED;
        if (!hard_box_blocked(mask, nx, p->y, hl, ht, hr, hb)) {
            p->x = nx;
        }
        if (!hard_box_blocked(mask, p->x, ny, hl, ht, hr, hb)) {
            p->y = ny;
        }
    }
    delay = seqs[seq].delay > 0 ? seqs[seq].delay : 50;
    nfr = seqs[seq].nframes > 0 ? seqs[seq].nframes : 1;
    p->acc += 16;
    if (p->acc >= delay) {
        p->acc = 0;
        p->frame++;
        if (p->frame > nfr) {
            p->frame = 1;
        }
    }
}
