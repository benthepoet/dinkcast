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
    *n = 0;
    memset(g, 0, sizeof(*g) * DINK_EDGFX_MAX);
    for (i = 1; i <= 99; i++) {
        int seq, fr;

        if (!scr->sprite[i].active) {
            continue;
        }
        seq = (int)scr->sprite[i].seq;
        fr = (int)scr->sprite[i].frame;
        if (fr < 1) {
            fr = 1;
        }
        if (seq < 1 || seq >= DINK_MAX_SEQ || seqs[seq].prefix[0] == '\0') {
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
        if (sprite_load_seq_frame(&seqs[seq], fr, &g[got].fr) != 0) {
            printf("edraw skip seq=%d frame=%d\n", seq, fr);
            continue;
        }
#ifdef _arch_dreamcast
        if (sprite_upload_pvr(&g[got].fr) != 0) {
            sprite_frame_free(&g[got].fr);
            printf("edraw upload fail seq=%d\n", seq);
            continue;
        }
#endif
        g[got].seq = seq;
        g[got].frame = fr;
        got++;
    }
    *n = got;
    return 0;
}
