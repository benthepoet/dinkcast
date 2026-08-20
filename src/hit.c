/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hit.h"

#include "brains.h"
#include "hurt.h"

#include <stdlib.h>

static struct Player *g_pl;
static void (*g_on_hit)(int slot, int attacker);
static void (*g_on_push)(int slot);

void hit_bind_player(struct Player *p)
{
    g_pl = p;
}

void hit_bind_hit(void (*fn)(int slot, int attacker))
{
    g_on_hit = fn;
}

void hit_bind_push(void (*fn)(int slot))
{
    g_on_push = fn;
}

static int inside_box(int x, int y, int l, int t, int r, int b)
{
    if (x > r || x < l || y > b || y < t) {
        return 0;
    }
    return 1;
}

static void seq_hardbox(int seq, int fr, struct EdGfx *edg, int ned,
                        struct SeqInfo *seqs, int *hl, int *ht, int *hr,
                        int *hb)
{
    struct SpriteFrame *ef;
    int cx, cy;

    if (fr < 1) {
        fr = 1;
    }
    ef = edraw_find(edg, ned, seq, fr);
    if (ef != NULL) {
        *hl = ef->hl;
        *ht = ef->ht;
        *hr = ef->hr;
        *hb = ef->hb;
        return;
    }
    if (seqs != NULL && seq > 0 && seq < DINK_MAX_SEQ) {
        ini_frame_geom(&seqs[seq], seq, fr, 50, 50, &cx, &cy, hl, ht, hr, hb);
        return;
    }
    *hl = -10;
    *ht = -10;
    *hr = 10;
    *hb = 10;
}

static void sprite_hardbox(const struct EditorSprite *es, struct EdGfx *edg,
                           int ned, struct SeqInfo *seqs, int *hl, int *ht,
                           int *hr, int *hb)
{
    int fr = (int)es->frame < 1 ? 1 : (int)es->frame;

    seq_hardbox((int)es->seq, fr, edg, ned, seqs, hl, ht, hr, hb);
}

static void inflate_hit(int *l, int *t, int *r, int *b, int dir, int range)
{
    int amount, amounty, range_amount;

    *r += 5;
    *l -= 5;
    *t -= 5;
    *b += 10;
    if (range == 0) {
        amount = 28;
        amounty = 36;
    } else {
        amount = range;
        amounty = range + range / 6;
    }
    range_amount = range / 8;
    if (dir == 6) {
        *t -= 10;
        *b += 10;
        if (range != 0) {
            *t -= range_amount;
            *b += range_amount;
        }
        *l -= amount;
    }
    if (dir == 4) {
        *r += amount;
        *t -= 10;
        *b += 10;
        if (range != 0) {
            *t -= range_amount;
            *b += range_amount;
        }
    }
    if (dir == 2) {
        *r += 10;
        *l -= 10;
        *t -= amounty;
        if (range != 0) {
            *r += range_amount;
            *l -= range_amount;
        }
    }
    if (dir == 8) {
        *r += 10;
        *l -= 10;
        *b += amounty;
        if (range != 0) {
            *r += range_amount;
            *r -= range_amount;
        }
    }
}

int hit_probe(const struct MapScreen *scr, struct EdGfx *edg, int ned,
              struct SeqInfo *seqs, int dx, int dy, int dir, int vision)
{
    int i;

    if (scr == NULL) {
        return 0;
    }
    for (i = 1; i <= 99; i++) {
        const struct EditorSprite *es = &scr->sprite[i];
        int hl, ht, hr, hb, l, t, r, b;

        if (!editor_sprite_on_vision(es, vision)) {
            continue;
        }
        if (es->type != 1) {
            continue;
        }
        sprite_hardbox(es, edg, ned, seqs, &hl, &ht, &hr, &hb);
        l = (int)es->x + hl;
        t = (int)es->y + ht;
        r = (int)es->x + hr;
        b = (int)es->y + hb;
        inflate_hit(&l, &t, &r, &b, dir, 0);
        if (inside_box(dx, dy, l, t, r, b)) {
            return i;
        }
    }
    return 0;
}

static int punch_damage(int strength)
{
    int half;

    if (strength < 1) {
        return 0;
    }
    half = (strength + 1) / 2;
    if (half < 1) {
        half = 1;
    }
    return (strength / 2) + ((rand() % half) + 1);
}

static void live_box(int slot, struct EdGfx *edg, int ned, struct SeqInfo *seqs,
                     int *l, int *t, int *r, int *b)
{
    int x, y, seq, fr, hl, ht, hr, hb;

    if (!brains_live_xy(slot, &x, &y)) {
        *l = *t = *r = *b = 0;
        return;
    }
    (void)brains_seq_frame(slot, &seq, &fr);
    seq_hardbox(seq, fr, edg, ned, seqs, &hl, &ht, &hr, &hb);
    *l = x + hl;
    *t = y + ht;
    *r = x + hr;
    *b = y + hb;
}

void hit_tag_list(int attacker, int ax, int ay, int dir, int strength,
                  int range, struct EdGfx *edg, int ned, struct SeqInfo *seqs)
{
    int i;

    for (i = 1; i <= 99; i++) {
        int l, t, r, b, nohit;

        if (i == 1) {
            if (g_pl != NULL && attacker != 1 && g_pl->nohit != 1) {
                int hl, ht, hr, hb;

                seq_hardbox(g_pl->seq, g_pl->frame, edg, ned, seqs, &hl, &ht,
                            &hr, &hb);
                l = g_pl->x + hl;
                t = g_pl->y + ht;
                r = g_pl->x + hr;
                b = g_pl->y + hb;
                inflate_hit(&l, &t, &r, &b, dir, range);
                if (inside_box(ax, ay, l, t, r, b)) {
                    g_pl->last_hit = attacker;
                    (void)player_hurt(g_pl, punch_damage(strength));
                }
            }
            /* Editor slot 1 is not Dink (g_b[1] / en-pill). Fall through. */
        }
        if (i == attacker && i != 1) {
            continue;
        }
        if (!brains_slot_live(i)) {
            continue;
        }
        nohit = brains_nohit(i);
        live_box(i, edg, ned, seqs, &l, &t, &r, &b);
        inflate_hit(&l, &t, &r, &b, dir, range);
        if (!inside_box(ax, ay, l, t, r, b)) {
            continue;
        }
        if (nohit == 1) {
            if (g_on_hit != NULL) {
                g_on_hit(i, attacker);
            }
            continue;
        }
        if (brains_base_attack(i) != -1 || brains_touch_damage(i) > 0) {
            brains_set_target(i, attacker);
        }
        if (strength != 0 && brains_hitpoints(i) > 0) {
            brains_set_last_hit(i, attacker);
            (void)brains_hurt(i, punch_damage(strength));
        }
        if (g_on_hit != NULL) {
            g_on_hit(i, attacker);
        }
    }
}

void hit_tag_list_push(int ax, int ay, struct EdGfx *edg, int ned,
                       struct SeqInfo *seqs)
{
    int i;

    for (i = 1; i <= 99; i++) {
        int l, t, r, b;

        if (!brains_slot_live(i)) {
            continue;
        }
        live_box(i, edg, ned, seqs, &l, &t, &r, &b);
        l -= 2;
        t -= 2;
        r += 2;
        b += 2;
        if (inside_box(ax, ay, l, t, r, b) && g_on_push != NULL) {
            g_on_push(i);
        }
    }
}
