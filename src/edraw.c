/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"

#include <stdio.h>
#include <string.h>

void edraw_free(struct EdGfx *g, int n)
{
    int i;

    if (g == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        sprite_frame_free(&g[i].fr);
        g[i].seq = g[i].frame = 0;
    }
}

struct SpriteFrame *edraw_find(struct EdGfx *g, int n, int seq, int frame)
{
    int i;

    if (g == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        if (g[i].seq == seq && g[i].frame == frame) {
            return &g[i].fr;
        }
    }
    return NULL;
}

int edraw_load_screen(const struct MapScreen *scr, struct SeqInfo *seqs,
                      struct EdGfx *g, int *n)
{
    int i, got = 0;

    if (scr == NULL || seqs == NULL || g == NULL || n == NULL) {
        return -1;
    }
    for (i = 0; i < DINK_EDGFX_MAX; i++) {
        sprite_frame_free(&g[i].fr);
    }
    *n = 0;
    memset(g, 0, sizeof(*g) * DINK_EDGFX_MAX);
    for (i = 1; i <= 100; i++) {
        int seq, fr;

        if (!editor_sprite_draw(&scr->sprite[i], DINK_VISION_DEFAULT)) {
            continue;
        }
        seq = (int)scr->sprite[i].seq;
        fr = (int)scr->sprite[i].frame;
        if (fr < 1) {
            fr = 1;
        }
        if (seq < 1 || seq >= DINK_MAX_SEQ) {
            printf("edraw skip slot=%d seq=%d\n", i, seq);
            continue;
        }
        if (seqs[seq].prefix[0] == '\0') {
            printf("edraw skip seq=%d no prefix\n", seq);
            continue;
        }
        if (edraw_find(g, got, seq, fr) != NULL) {
            continue;
        }
        if (got >= DINK_EDGFX_MAX) {
            break;
        }
        if (seqs[seq].nframes <= 0) {
            seqs[seq].nframes = ini_count_ff_frames(seqs[seq].prefix);
        }
        if (sprite_load_seq_frame(&seqs[seq], seq, fr, &g[got].fr) != 0) {
            printf("edraw skip seq=%d frame=%d\n", seq, fr);
            continue;
        }
        g[got].seq = seq;
        g[got].frame = fr;
        got++;
    }
    *n = got;
    return 0;
}

#ifdef _arch_dreamcast
int edraw_upload_pvr(struct EdGfx *g, int n)
{
    int i, ok = 0;

    if (g == NULL) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (sprite_upload_pvr(&g[i].fr) != 0) {
            printf("edraw upload fail seq=%d\n", g[i].seq);
            continue;
        }
        ok++;
    }
    return ok > 0 || n == 0 ? 0 : -1;
}
#endif
