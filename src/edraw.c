/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"

#include "ff.h"

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

struct EdGfx *edraw_gfx_alloc(void)
{
    return (struct EdGfx *)calloc((size_t)DINK_EDGFX_MAX, sizeof(struct EdGfx));
}

void edraw_gfx_release(struct EdGfx *g)
{
    edraw_free(g, DINK_EDGFX_MAX);
    free(g);
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

int edraw_load_screen(struct EditorSprite *spr, struct SeqInfo *seqs,
                      struct EdGfx *g, int *n)
{
    struct EditorSprite *sp;
    int i, got = 0;

    if (spr == NULL || seqs == NULL || g == NULL || n == NULL) {
        return -1;
    }
    /* Heap copy FIRST. BSS memset(g_edg) sits next to g_scr / g_spr_ok. */
    sp = (struct EditorSprite *)malloc(101u * sizeof(*sp));
    if (sp == NULL) {
        return -1;
    }
    memcpy(sp, spr, 101u * sizeof(*sp));
    printf("edraw in sprite1 seq=%d y=%d act=%d keep=%d\n", (int)sp[1].seq,
           (int)sp[1].y, (int)sp[1].active, *n);
    for (i = 1; i <= 8; i++) {
        printf("edraw slot %d act=%d seq=%d fr=%d xy=%d,%d vis=%d\n", i,
               (int)sp[i].active, (int)sp[i].seq, (int)sp[i].frame,
               (int)sp[i].x, (int)sp[i].y, (int)sp[i].vision);
    }
    {
        int need_s[DINK_EDGFX_MAX], need_f[DINK_EDGFX_MAX], nneed = 0, old, k;

        for (i = 1; i <= 100 && nneed < DINK_EDGFX_MAX; i++) {
            int seq, fr, d;

            if (!editor_sprite_draw(&sp[i], DINK_VISION_DEFAULT)) {
                continue;
            }
            seq = (int)sp[i].seq;
            fr = (int)sp[i].frame < 1 ? 1 : (int)sp[i].frame;
            if (seq < 1 || seq >= DINK_MAX_SEQ || seqs[seq].prefix[0] == '\0') {
                continue;
            }
            for (d = 0; d < nneed; d++) {
                if (need_s[d] == seq && need_f[d] == fr) {
                    break;
                }
            }
            if (d < nneed) {
                continue;
            }
            need_s[nneed] = seq;
            need_f[nneed] = fr;
            nneed++;
        }
        old = *n;
        if (old < 0) {
            old = 0;
        }
        if (old > DINK_EDGFX_MAX) {
            old = DINK_EDGFX_MAX;
        }
        for (i = 0; i < old; i++) {
            int keep = 0;

            for (k = 0; k < nneed; k++) {
                if (g[i].seq == need_s[k] && g[i].frame == need_f[k]) {
                    keep = 1;
                    break;
                }
            }
            g[i].live = keep;
        }
        got = old;
        for (k = 0; k < nneed; k++) {
            if (edraw_find(g, got, need_s[k], need_f[k]) != NULL) {
                continue;
            }
            while (got >= DINK_EDGFX_MAX) {
                int drop = -1;

                for (i = 0; i < got; i++) {
                    if (!g[i].live) {
                        drop = i;
                        break;
                    }
                }
                if (drop < 0) {
                    break;
                }
                sprite_frame_free(&g[drop].fr);
                got--;
                if (drop < got) {
                    g[drop] = g[got];
                    memset(&g[got], 0, sizeof(g[got]));
                } else {
                    memset(&g[drop], 0, sizeof(g[drop]));
                }
            }
            if (got >= DINK_EDGFX_MAX) {
                printf("edraw full skip seq=%d fr=%d\n", need_s[k], need_f[k]);
                continue;
            }
            printf("edraw load seq=%d fr=%d\n", need_s[k], need_f[k]);
            if (sprite_load_seq_frame(&seqs[need_s[k]], need_s[k], need_f[k],
                                      &g[got].fr) != 0) {
                printf("edraw skip seq=%d frame=%d\n", need_s[k], need_f[k]);
                continue;
            }
            g[got].seq = need_s[k];
            g[got].frame = need_f[k];
            g[got].live = 1;
            got++;
            /* Neighbor screens reuse this seq with other frames (yard
             * home- 1/2/11, east 3/4). Decode the rest while dir.ff is
             * open; reopening the pack hangs /cd. */
            {
                int nfr = seqs[need_s[k]].nframes;
                int fi;

                if (nfr > 24) {
                    nfr = 0;
                }
                for (fi = 1; fi <= nfr; fi++) {
                    int live = 0, d;

                    if (edraw_find(g, got, need_s[k], fi) != NULL) {
                        continue;
                    }
                    if (got >= DINK_EDGFX_MAX) {
                        break;
                    }
                    if (sprite_load_seq_frame(&seqs[need_s[k]], need_s[k], fi,
                                              &g[got].fr) != 0) {
                        continue;
                    }
                    for (d = 0; d < nneed; d++) {
                        if (need_s[d] == need_s[k] && need_f[d] == fi) {
                            live = 1;
                            break;
                        }
                    }
                    g[got].seq = need_s[k];
                    g[got].frame = fi;
                    g[got].live = live;
                    got++;
                }
            }
        }
        for (i = 0; i < got; i++) {
            if (!g[i].live) {
                sprite_evict_pvr(&g[i].fr);
            }
        }
    }
    ff_cache_drop_unpinned();
    memcpy(spr, sp, 101u * sizeof(*sp));
    free(sp);
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
        if (!g[i].live) {
            continue;
        }
        if (sprite_upload_pvr(&g[i].fr) != 0) {
            printf("edraw upload fail seq=%d\n", g[i].seq);
            continue;
        }
        ok++;
    }
    return ok > 0 || n == 0 ? 0 : -1;
}
#endif
