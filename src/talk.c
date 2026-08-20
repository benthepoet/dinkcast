/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "talk.h"

#include <string.h>

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

int talk_probe(const struct MapScreen *scr, struct EdGfx *edg, int ned,
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
        if (es->type != 1 || es->brain == 8 || es->script[0] == '\0') {
            continue;
        }
        sprite_hardbox(es, edg, ned, seqs, &hl, &ht, &hr, &hb);
        l = (int)es->x + hl - 10;
        t = (int)es->y + ht - 10;
        r = (int)es->x + hr + 10;
        b = (int)es->y + hb + 10;
        if (dir == 6) {
            l -= 50;
        }
        if (dir == 4) {
            r += 50;
        }
        if (dir == 2) {
            t -= 35;
        }
        if (dir == 8) {
            b += 35;
        }
        if (inside_box(dx, dy, l, t, r, b)) {
            return i;
        }
    }
    return 0;
}

/* human_brain talk miss / magic_script==0. Official data has no
 * dnotalk.c / dnomagic.c so the 1.08 script hook never runs. */
static const char *k_notalk[6] = {
    "`$I don't see anything here.",
    "`$Huh?",
    "`$I'm fairly sure I can't talk to or use that.",
    "`$What?",
    "`$I'm bored.",
    "`$Not much happening here.",
};

static const char *k_nomagic[6] = {
    "`$I don't know any magic.",
    "`$I'm no wizard!",
    "`$I need to learn magic before trying this.",
    "`$I'm gesturing wildly to no avail!",
    "`$Nothing happened.",
    "`$Hocus pocus!",
};

const char *talk_miss_line(int r)
{
    if (r < 1 || r > 6) {
        return "";
    }
    return k_notalk[r - 1];
}

const char *magic_miss_line(int r)
{
    if (r < 1 || r > 6) {
        return "";
    }
    return k_nomagic[r - 1];
}
