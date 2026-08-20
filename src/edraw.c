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

static int need_has(const int *ns, const int *nf, int n, int seq, int fr)
{
    int d;

    for (d = 0; d < n; d++) {
        if (ns[d] == seq && nf[d] == fr) {
            return 1;
        }
    }
    return 0;
}

static void need_push(int *ns, int *nf, int *n, int seq, int fr)
{
    if (*n >= DINK_EDGFX_MAX || need_has(ns, nf, *n, seq, fr)) {
        return;
    }
    ns[*n] = seq;
    nf[*n] = fr;
    (*n)++;
}

static int load_one(struct EdGfx *g, int *got, struct SeqInfo *seqs, int seq,
                    int frame)
{
    if (edraw_find(g, *got, seq, frame) != NULL) {
        return 0;
    }
    if (*got >= DINK_EDGFX_MAX) {
        printf("edraw full skip seq=%d fr=%d\n", seq, frame);
        return -1;
    }
    printf("edraw load seq=%d fr=%d\n", seq, frame);
    if (sprite_load_seq_frame(&seqs[seq], seq, frame, &g[*got].fr) != 0) {
        printf("edraw skip seq=%d frame=%d\n", seq, frame);
        return -1;
    }
    g[*got].seq = seq;
    g[*got].frame = frame;
    g[*got].live = 1;
    (*got)++;
    return 0;
}

static void load_seq_frames(struct EdGfx *g, int *got, struct SeqInfo *seqs,
                            int seq)
{
    int nfr, f2;

    if (seq < 1 || seq >= DINK_MAX_SEQ || seqs[seq].prefix[0] == '\0') {
        return;
    }
    nfr = ini_seq_len(seq, seqs[seq].nframes);
    for (f2 = 1; f2 <= nfr && *got < DINK_EDGFX_MAX; f2++) {
        (void)load_one(g, got, seqs, seq, f2);
    }
}

/* people/pig/pill: +1,3,7,9. duck: those plus 4,6. dragon: 2,4,6,8. */
static void walk_seqs_for_brain(int brain, int base, int *out, int *n)
{
    static const int diag[4] = {1, 3, 7, 9};
    static const int card[4] = {2, 4, 6, 8};
    static const int duck[6] = {1, 3, 4, 6, 7, 9};
    const int *d = diag;
    int nd = 4, i;

    *n = 0;
    if (base < 0) {
        return;
    }
    if (brain == 10) {
        d = card;
        nd = 4;
    } else if (brain == 3) {
        d = duck;
        nd = 6;
    }
    for (i = 0; i < nd; i++) {
        out[i] = base + d[i];
        (*n)++;
    }
}

static void pack_dir(const struct SeqInfo *seq, char *dir, size_t n)
{
    const char *sl;

    dir[0] = '\0';
    if (seq == NULL || seq->prefix[0] == '\0') {
        return;
    }
    sl = strrchr(seq->prefix, '/');
    if (sl == NULL) {
        return;
    }
    snprintf(dir, n, "%.*s/dir.ff", (int)(sl - seq->prefix), seq->prefix);
}

int edraw_load_screen(struct EditorSprite *spr, struct SeqInfo *seqs,
                      struct EdGfx *g, int *n, int vision)
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

        for (i = 1; i <= 100; i++) {
            int seq, fr;

            if (!editor_sprite_draw(&sp[i], vision)) {
                continue;
            }
            seq = (int)sp[i].seq;
            fr = (int)sp[i].frame < 1 ? 1 : (int)sp[i].frame;
            if (seq < 1 || seq >= DINK_MAX_SEQ || seqs[seq].prefix[0] == '\0') {
                continue;
            }
            need_push(need_s, need_f, &nneed, seq, fr);
            /* Repeat brains cycle the whole seq (fireplace). */
            if ((int)sp[i].type == 1 && (int)sp[i].brain == 6) {
                int nfr, f2;

                nfr = ini_seq_len(seq, seqs[seq].nframes);
                for (f2 = 1; f2 <= nfr; f2++) {
                    need_push(need_s, need_f, &nneed, seq, f2);
                }
            }
            /* Walk brains share that sprite's dir.ff. */
            if ((int)sp[i].type == 1) {
                int br = (int)sp[i].brain;
                int ws[6], nw, w, f2, nfr;

                if (br == 3 || br == 4 || br == 9 || br == 10 || br == 16) {
                    walk_seqs_for_brain(br, (int)sp[i].base_walk, ws, &nw);
                    for (w = 0; w < nw; w++) {
                        if (ws[w] < 1 || ws[w] >= DINK_MAX_SEQ ||
                            seqs[ws[w]].prefix[0] == '\0') {
                            continue;
                        }
                        need_push(need_s, need_f, &nneed, ws[w], 1);
                        nfr = ini_seq_len(ws[w], seqs[ws[w]].nframes);
                        for (f2 = 1; f2 <= nfr; f2++) {
                            need_push(need_s, need_f, &nneed, ws[w], f2);
                        }
                    }
                }
            }
        }
        old = *n;
        if (old < 0) {
            old = 0;
        }
        if (old > DINK_EDGFX_MAX) {
            old = DINK_EDGFX_MAX;
        }
        /* Free unused pixels. Pinned dir.ff still supplies other frames
         * without /cd. Keeping 96 decoded frames OOMs the 16 MB heap. */
        i = 0;
        while (i < old) {
            int keep = 0;

            for (k = 0; k < nneed; k++) {
                if (g[i].seq == need_s[k] && g[i].frame == need_f[k]) {
                    keep = 1;
                    break;
                }
            }
            g[i].live = keep;
            if (keep) {
                i++;
            } else {
                sprite_frame_free(&g[i].fr);
                old--;
                if (i < old) {
                    g[i] = g[old];
                    memset(&g[old], 0, sizeof(g[old]));
                } else {
                    memset(&g[i], 0, sizeof(g[i]));
                }
            }
        }
        got = old;
        for (k = 0; k < nneed; k++) {
            if (edraw_find(g, got, need_s[k], need_f[k]) != NULL) {
                continue;
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
        }
        /* After frame 1 decode, seq.nframes is known. Load remaining
         * brain-6 seqs and walk dirs while packs stay open. */
        for (i = 1; i <= 100; i++) {
            int br, ws[6], nw, w;

            if ((int)sp[i].type != 1 ||
                !editor_sprite_on_vision(&sp[i], vision)) {
                continue;
            }
            br = (int)sp[i].brain;
            if (br == 6) {
                load_seq_frames(g, &got, seqs, (int)sp[i].seq);
            }
            if (br == 3 || br == 4 || br == 9 || br == 10 || br == 16) {
                walk_seqs_for_brain(br, (int)sp[i].base_walk, ws, &nw);
                for (w = 0; w < nw; w++) {
                    load_seq_frames(g, &got, seqs, ws[w]);
                }
            }
        }
    }
    ff_cache_drop_unpinned();
    memcpy(spr, sp, 101u * sizeof(*sp));
    free(sp);
    *n = got;
    return 0;
}

int edraw_ensure_frame(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq,
                       int frame)
{
    char dir[160];
    int got;

    if (g == NULL || n == NULL || seqs == NULL || seq < 1 ||
        seq >= DINK_MAX_SEQ || frame < 1) {
        return -1;
    }
    got = *n;
    if (got < 0) {
        got = 0;
    }
    if (edraw_find(g, got, seq, frame) != NULL) {
        return 0;
    }
    /* Play path: decode only from a pack already opened at screen load. */
    pack_dir(&seqs[seq], dir, sizeof(dir));
    if (dir[0] == '\0' || !ff_is_cached(dir)) {
        printf("edraw skip seq=%d fr=%d (pack not cached)\n", seq, frame);
        return -1;
    }
    if (load_one(g, &got, seqs, seq, frame) != 0) {
        return -1;
    }
#ifdef _arch_dreamcast
    (void)sprite_upload_pvr(&g[got - 1].fr);
#endif
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
