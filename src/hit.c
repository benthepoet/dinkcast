/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hit.h"

static int inside_box(int x, int y, int l, int t, int r, int b)
{
    if (x > r || x < l || y > b || y < t) {
        return 0;
    }
    return 1;
}

static void sprite_hardbox(const struct EditorSprite *es, struct EdGfx *edg,
                           int ned, struct SeqInfo *seqs, int *hl, int *ht,
                           int *hr, int *hb)
{
    int fr, cx, cy;
    struct SpriteFrame *ef;

    fr = (int)es->frame < 1 ? 1 : (int)es->frame;
    ef = edraw_find(edg, ned, (int)es->seq, fr);
    if (ef != NULL) {
        *hl = ef->hl;
        *ht = ef->ht;
        *hr = ef->hr;
        *hb = ef->hb;
        return;
    }
    if (seqs != NULL && es->seq > 0 && es->seq < DINK_MAX_SEQ) {
        ini_frame_geom(&seqs[es->seq], (int)es->seq, fr, 50, 50, &cx, &cy, hl,
                       ht, hr, hb);
        return;
    }
    *hl = -10;
    *ht = -10;
    *hr = 10;
    *hb = 10;
}

int hit_probe(const struct MapScreen *scr, struct EdGfx *edg, int ned,
              struct SeqInfo *seqs, int dx, int dy, int dir)
{
    int i;

    if (scr == NULL) {
        return 0;
    }
    for (i = 1; i <= 99; i++) {
        const struct EditorSprite *es = &scr->sprite[i];
        int hl, ht, hr, hb, l, t, r, b;

        if (!editor_sprite_on_vision(es, DINK_VISION_DEFAULT)) {
            continue;
        }
        if (es->type != 1) {
            continue;
        }
        sprite_hardbox(es, edg, ned, seqs, &hl, &ht, &hr, &hb);
        /* run_through_tag_list: +5/-5/-5/+10 then dir range. */
        l = (int)es->x + hl - 5;
        t = (int)es->y + ht - 5;
        r = (int)es->x + hr + 5;
        b = (int)es->y + hb + 10;
        if (dir == 6) {
            t -= 10;
            b += 10;
            l -= 28;
        }
        if (dir == 4) {
            r += 28;
            t -= 10;
            b += 10;
        }
        if (dir == 2) {
            r += 10;
            l -= 10;
            t -= 36;
        }
        if (dir == 8) {
            r += 10;
            l -= 10;
            b += 36;
        }
        if (inside_box(dx, dy, l, t, r, b)) {
            return i;
        }
    }
    return 0;
}
