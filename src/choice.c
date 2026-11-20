/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "choice.h"

#include "font.h"

#include <stdio.h>
#include <string.h>

static struct SpriteFrame g_box[5];
static struct SpriteFrame g_arowl[DINK_CHOICE_ARROW_MAX + 1];
static struct SpriteFrame g_arowr[DINK_CHOICE_ARROW_MAX + 1];
static int g_curf;
static int g_timer;

void choice_wrap(char *s, int max_px)
{
    int i = 0, start = 0, last_sp = -1;
    int cell = font_advance('A');

    if (s == NULL) {
        return;
    }
    if (cell < 1) {
        cell = DINK_FONT_CELL;
    }
    while (s[i] != '\0') {
        if (s[i] == ' ') {
            last_sp = i;
        }
        if (s[i] == '\n') {
            start = i + 1;
            last_sp = -1;
            i++;
            continue;
        }
        if ((i - start + 1) * cell > max_px) {
            if (last_sp > start) {
                s[last_sp] = '\n';
                start = last_sp + 1;
                last_sp = -1;
            } else {
                start = i;
            }
        }
        i++;
    }
}

int choice_wrap_height(const char *s)
{
    char buf[256];
    int i, nlines = 1;

    if (s == NULL) {
        s = "";
    }
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    choice_wrap(buf, DINK_CHOICE_BOX_R - DINK_CHOICE_BOX_L);
    for (i = 0; buf[i] != '\0'; i++) {
        if (buf[i] == '\n') {
            nlines++;
        }
    }
    return nlines * DINK_FONT_CELL;
}

int choice_center_x(const char *line)
{
    int cell = font_advance('A');
    int w, boxw, n = 0;
    const char *p = line != NULL ? line : "";

    if (cell < 1) {
        cell = DINK_FONT_CELL;
    }
    while (p[n] != '\0' && p[n] != '\n') {
        n++;
    }
    w = n * cell;
    boxw = DINK_CHOICE_BOX_R - DINK_CHOICE_BOX_L;
    return DINK_CHOICE_BOX_L + boxw / 2 - w / 2;
}

void choice_layout(int last, int cur, const int *h, int newy,
                   struct ChoiceLayout *out)
{
    int view = 1, guard = 0;

    if (out == NULL) {
        return;
    }
    out->choices_y = DINK_CHOICE_Y0;
    out->view_lo = 1;
    out->view_hi = last < 1 ? 0 : last;
    if (last < 1 || h == NULL) {
        return;
    }
    if (cur < 1) {
        cur = 1;
    }
    if (cur > last) {
        cur = last;
    }
    for (;;) {
        int choices_y = DINK_CHOICE_Y0;
        int sy_hold, i, view_end;

        if (newy != DINK_CHOICE_NEWY_NONE) {
            choices_y = newy;
        }
        sy_hold = choices_y;
        i = view;
        for (; i < last; i++) {
            sy_hold += h[i];
            if (sy_hold > DINK_CHOICE_X_DEPTH) {
                view_end = i - 1;
                goto death;
            }
        }
        view_end = i;
        if (view == 1 && view_end == last) {
            choices_y += ((DINK_CHOICE_X_DEPTH - sy_hold) / 2) - 20;
        }
    death:
        if (view_end < view) {
            view_end = view;
        }
        if (cur > view_end) {
            view = cur;
            if (++guard > 32) {
                break;
            }
            continue;
        }
        if (cur < view) {
            view = 1;
            if (++guard > 32) {
                break;
            }
            continue;
        }
        out->choices_y = choices_y;
        out->view_lo = view;
        out->view_hi = view_end;
        return;
    }
    out->choices_y = DINK_CHOICE_Y0;
    out->view_lo = 1;
    out->view_hi = last;
}

/* Loaded: CPU pixels before upload, texture after (CPU dropped). */
static int frame_has(const struct SpriteFrame *f)
{
    return sprite_has_cpu(f) || f->tex != NULL;
}

static int load_fr(struct SeqInfo *seqs, int seq, int frame,
                   struct SpriteFrame *dst)
{
    if (seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ || dst == NULL) {
        return -1;
    }
    sprite_frame_free(dst);
    if (sprite_load_seq_frame(&seqs[seq], seq, frame, dst) != 0) {
        printf("choice skip seq=%d fr=%d\n", seq, frame);
        return -1;
    }
    return 0;
}

static struct SeqInfo *g_seqs;

/* Arrows (seq 456/457, small) stay resident; the box (seq 30 frames 2..4,
 * ~0.7 MB) is VRAM only while a choice is up (overlay). */
int choice_arrows_load(struct SeqInfo *seqs)
{
    int f, ok = 0;

    g_seqs = seqs;
    if (seqs == NULL) {
        return -1;
    }
    for (f = 1; f <= DINK_CHOICE_ARROW_MAX; f++) {
        if (load_fr(seqs, DINK_CHOICE_AROWL, f, &g_arowl[f]) == 0 &&
            load_fr(seqs, DINK_CHOICE_AROWR, f, &g_arowr[f]) == 0) {
            ok++;
        }
    }
    printf("choice gfx arrows=%d/%d\n", ok, DINK_CHOICE_ARROW_MAX);
    return ok == DINK_CHOICE_ARROW_MAX ? 0 : -1;
}

int choice_box_load(void)
{
    int f, ok = 0;

    if (g_seqs == NULL) {
        return -1;
    }
    for (f = 2; f <= 4; f++) {
        if (g_box[f].tex != NULL || sprite_has_cpu(&g_box[f]) ||
            load_fr(g_seqs, DINK_CHOICE_SEQ, f, &g_box[f]) == 0) {
            ok++;
        }
    }
    return ok == 3 ? 0 : -1;
}

void choice_box_release(void)
{
    int f;

    for (f = 0; f < 5; f++) {
        sprite_frame_free(&g_box[f]);
    }
}

int choice_box_nframes(void)
{
    return 3;
}

struct SpriteFrame *choice_box_frame(int i)
{
    return i >= 0 && i < 3 ? &g_box[i + 2] : NULL;
}

/* Start menu Continue: everything, outside the play loop. */
int choice_load(struct SeqInfo *seqs)
{
    int ok;

    g_curf = 0;
    g_timer = 0;
    if (seqs == NULL) {
        return -1;
    }
    g_seqs = seqs;
    if (!frame_has(&g_arowl[1])) {
        (void)choice_arrows_load(seqs);
    }
    ok = choice_box_load();
    printf("choice gfx box=%s\n", ok == 0 ? "ok" : "partial");
    return ok;
}

void choice_free(void)
{
    int f;

    for (f = 0; f < 5; f++) {
        sprite_frame_free(&g_box[f]);
    }
    for (f = 0; f <= DINK_CHOICE_ARROW_MAX; f++) {
        sprite_frame_free(&g_arowl[f]);
        sprite_frame_free(&g_arowr[f]);
    }
}

struct SpriteFrame *choice_frame(int seq, int frame)
{
    if (seq == DINK_CHOICE_SEQ && frame >= 2 && frame <= 4) {
        return frame_has(&g_box[frame]) ? &g_box[frame] : NULL;
    }
    if (seq == DINK_CHOICE_AROWL && frame >= 1 && frame <= DINK_CHOICE_ARROW_MAX) {
        return frame_has(&g_arowl[frame]) ? &g_arowl[frame] : NULL;
    }
    if (seq == DINK_CHOICE_AROWR && frame >= 1 && frame <= DINK_CHOICE_ARROW_MAX) {
        return frame_has(&g_arowr[frame]) ? &g_arowr[frame] : NULL;
    }
    return NULL;
}


void choice_tick(int now_ms)
{
    if (now_ms >= g_timer) {
        g_curf++;
        g_timer = now_ms + 100;
    }
    if (g_curf < 1) {
        g_curf = 1;
    }
    if (g_curf > DINK_CHOICE_ARROW_MAX) {
        g_curf = 1;
    }
}

int choice_curf(void)
{
    return g_curf;
}

#ifdef _arch_dreamcast
int choice_upload_pvr(void)
{
    int f, n = 0;

    for (f = 2; f <= 4; f++) {
        if (g_box[f].tex != NULL ||
            (sprite_has_cpu(&g_box[f]) && sprite_upload_pvr(&g_box[f]) == 0)) {
            sprite_drop_cpu(&g_box[f]);
            n++;
        }
    }
    for (f = 1; f <= DINK_CHOICE_ARROW_MAX; f++) {
        if (sprite_has_cpu(&g_arowl[f]) &&
            sprite_upload_pvr(&g_arowl[f]) == 0) {
            sprite_drop_cpu(&g_arowl[f]);
        }
        if (sprite_has_cpu(&g_arowr[f]) &&
            sprite_upload_pvr(&g_arowr[f]) == 0) {
            sprite_drop_cpu(&g_arowr[f]);
        }
    }
    return n == 3 ? 0 : -1;
}
#endif
