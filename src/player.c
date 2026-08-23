/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "player.h"

#include "hurt.h"
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
    p->freeze = 0;
    p->nocontrol = 0;
    p->just_hit = 0;
    p->just_push = 0;
    p->base_attack = DINK_BASE_ATTACK;
    p->base_idle = DINK_BASE_IDLE;
    p->base_push = DINK_BASE_PUSH;
    p->base_hit = 100;
    p->distance = 0;
    p->warp_hit = 0;
    p->move_active = 0;
    p->move_dir = 0;
    p->move_num = 0;
    p->move_nohard = 0;
    p->hitpoints = 0;
    p->defense = 0;
    p->strength = 0;
    p->nohit = 0;
    p->range = 0;
    p->damage = 0;
    p->last_hit = 0;
    p->push_active = 0;
    p->push_dir = 0;
    p->push_timer = 0;
    p->notouch = 0;
    p->notouch_timer = 0;
}

int player_hurt(struct Player *p, int damage)
{
    int num;

    if (p == NULL) {
        return 0;
    }
    num = hurt_roll(damage, p->defense);
    p->damage += num;
    return num;
}

int player_apply_life(struct Player *p, int *life)
{
    if (p == NULL || life == NULL) {
        return 0;
    }
    if (p->damage > 0) {
        *life -= p->damage;
        p->damage = 0;
        if (*life < 0) {
            *life = 0;
        }
    }
    return *life < 1 ? 1 : 0;
}

void player_attack(struct Player *p, const struct SeqInfo *seqs)
{
    int seq;

    if (p == NULL || p->freeze > 0 || p->nocontrol) {
        return;
    }
    if (p->dir == 1 || p->dir == 3) {
        p->dir = 2;
    }
    if (p->dir == 7 || p->dir == 9) {
        p->dir = 8;
    }
    seq = p->base_attack + p->dir;
    if (seqs != NULL && (seq < 1 || seq >= DINK_MAX_SEQ)) {
        return;
    }
    p->seq = seq;
    p->frame = 1;
    p->acc = 0;
    p->nocontrol = 1;
    p->just_hit = 0;
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

int player_walk_pad(int pad_dir, int freeze, int choice_active)
{
    /* FreeDink human_brain: game_choice.active goto freeze. SAVEBOT
     * unfreeze(1) before the 10-slot menu; Down must not walk. */
    if (freeze > 0 || choice_active) {
        return 0;
    }
    return pad_dir;
}

void player_step(struct Player *p, int pad_dir, const struct HardMask *mask,
                 const struct SeqInfo *seqs, int now_ms,
                 const struct MapScreen *scr)
{
    int seq, delay, nfr, nx, ny, dx, dy;
    int blocked = 0;

    if (p == NULL || seqs == NULL) {
        return;
    }
    p->just_hit = 0;
    p->just_push = 0;
    p->warp_hit = 0;
    if (p->move_active) {
        int d = p->move_dir;
        int done = 0;

        if ((d == 4 || d == 1 || d == 7) && p->x <= p->move_num) {
            done = 1;
        }
        if ((d == 6 || d == 9 || d == 3) && p->x >= p->move_num) {
            done = 1;
        }
        if (d == 2 && p->y >= p->move_num) {
            done = 1;
        }
        if (d == 8 && p->y <= p->move_num) {
            done = 1;
        }
        if (done) {
            p->move_active = 0;
            p->move_nohard = 0;
            return;
        }
        pad_dir = d;
        p->dir = d;
        /* Scripted move: ignore freeze for motion (FreeDink process_move). */
    } else if (p->freeze > 0) {
        pad_dir = 0;
    }
    if (p->nocontrol) {
        delay = p->frame_delay != 0
                    ? p->frame_delay
                    : ini_frame_delay(p->seq, p->frame, seqs[p->seq].delay);
        if (delay < 1) {
            delay = 50;
        }
        nfr = ini_seq_len(p->seq, seqs[p->seq].nframes);
        if (nfr < 1) {
            nfr = 1;
        }
        p->acc += 16;
        if (p->acc >= delay) {
            int dseq, dfr;

            p->acc = 0;
            p->frame++;
            if (p->frame > nfr ||
                ini_resolve_frame(p->seq, p->frame, &dseq, &dfr) != 0) {
                p->nocontrol = 0;
                if (p->dir == 1 || p->dir == 3) {
                    p->dir = 2;
                }
                if (p->dir == 7 || p->dir == 9) {
                    p->dir = 8;
                }
                p->seq = DINK_BASE_IDLE + p->dir;
                p->frame = 1;
            } else if (ini_frame_special(p->seq, p->frame)) {
                p->just_hit = 1;
            }
        }
        return;
    }
    if (p->push_active) {
        if (pad_dir == 0 || pad_dir != p->push_dir) {
            p->push_active = 0;
        }
    }
    if (p->push_active && now_ms > p->push_timer + 600) {
        p->seq = p->base_push + p->dir;
        p->frame = 1;
        p->acc = 0;
        p->nocontrol = 1;
        p->just_push = 1;
        return;
    }
    if (pad_dir != 0) {
        p->dir = pad_dir;
    } else {
        /* human_brain idle: no ds-i1/3/7/9 — snap to down/up. */
        if (p->dir == 1 || p->dir == 3) {
            p->dir = 2;
        }
        if (p->dir == 7 || p->dir == 9) {
            p->dir = 8;
        }
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
    if (pad_dir != 0) {
        int i;

        dir_delta(pad_dir, &dx, &dy);
        /* FreeDink changedir: diag mx/my = speed - speed/3; then 1px move(). */
        {
            int steps = DINK_SPEED;

            if (dx != 0 && dy != 0) {
                steps = DINK_SPEED - DINK_SPEED / 3;
            }
            for (i = 0; i < steps; i++) {
                nx = p->x + dx;
                ny = p->y + dy;
                if (dx != 0) {
                    int h = 0, wed = 0;

                    if (!p->move_nohard) {
                        h = hard_get_play(mask, scr, nx, p->y, &wed);
                    }
                    if (wed > 0) {
                        p->warp_hit = wed;
                    }
                    if (h == 0) {
                        p->x = nx;
                    } else if (h != 2) {
                        blocked = 1;
                    }
                }
                if (dy != 0) {
                    int h = 0, wed = 0;

                    if (!p->move_nohard) {
                        h = hard_get_play(mask, scr, p->x, ny, &wed);
                    }
                    if (wed > 0) {
                        p->warp_hit = wed;
                    }
                    if (h == 0) {
                        p->y = ny;
                    } else if (h != 2) {
                        blocked = 1;
                    }
                }
            }
        }
        /* check_if_move_is_legal: hardness starts push; clear on free walk. */
        if (blocked && (p->dir == 2 || p->dir == 4 || p->dir == 6 ||
                        p->dir == 8)) {
            if (!p->push_active) {
                p->push_active = 1;
                p->push_dir = p->dir;
                p->push_timer = now_ms;
            } else if (p->push_dir != p->dir) {
                p->push_active = 0;
            }
        } else if (!blocked) {
            p->push_active = 0;
        }
    }
    delay = p->frame_delay != 0
                ? p->frame_delay
                : ini_frame_delay(seq, p->frame, seqs[seq].delay);
    if (delay < 1) {
        delay = 50;
    }
    nfr = ini_seq_len(seq, seqs[seq].nframes);
    if (nfr < 1) {
        nfr = 1;
    }
    p->acc += 16;
    if (p->acc >= delay) {
        int dseq, dfr;

        p->acc = 0;
        p->frame++;
        if (p->frame > nfr ||
            ini_resolve_frame(seq, p->frame, &dseq, &dfr) != 0) {
            p->frame = 1;
        }
    }
}
