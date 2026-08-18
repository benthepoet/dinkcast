/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"

#include <stdio.h>
#include <stdlib.h>
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

int edraw_load_screen(const struct EditorSprite *spr, struct SeqInfo *seqs,
                      struct EdGfx *g, int *n)
{
    /* Own BSS: copy BEFORE touching g[] (memset g_edg was eating g_scr). */
    static struct EditorSprite sp[101];
    int i, got = 0;

    if (spr == NULL || seqs == NULL || g == NULL || n == NULL) {
        return -1;
    }
    memcpy(sp, spr, sizeof(sp));
    printf("edraw in sprite1 seq=%d y=%d act=%d\n", (int)sp[1].seq,
           (int)sp[1].y, (int)sp[1].active);
    for (i = 0; i < DINK_EDGFX_MAX; i++) {
        sprite_frame_free(&g[i].fr);
    }
    *n = 0;
    memset(g, 0, sizeof(*g) * (size_t)DINK_EDGFX_MAX);
    for (i = 1; i <= 8; i++) {
        printf("edraw slot %d act=%d seq=%d fr=%d xy=%d,%d vis=%d\n", i,
               (int)sp[i].active, (int)sp[i].seq, (int)sp[i].frame,
               (int)sp[i].x, (int)sp[i].y, (int)sp[i].vision);
    }
    for (i = 1; i <= 100; i++) {
        int seq, fr;

        if (!editor_sprite_draw(&sp[i], DINK_VISION_DEFAULT)) {
            continue;
        }
        seq = (int)sp[i].seq;
        fr = (int)sp[i].frame;
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
