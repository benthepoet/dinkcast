/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"

#include "ff.h"
#include "fs.h"
#include "mem.h"
#include "residency.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

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

size_t edraw_cpu_bytes(const struct EdGfx *g, int n)
{
    size_t t = 0;
    int i;

    if (g == NULL || n < 1) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (g[i].fr.argb1555 != NULL && g[i].fr.tw > 0 && g[i].fr.th > 0) {
            t += (size_t)g[i].fr.tw * (size_t)g[i].fr.th * 2u;
        }
    }
    return t;
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

enum {
    PIX_SCREEN = 0,
    PIX_ALWAYS = 1,
    PIX_STICKY = 2
};

static int pixel_class(const struct SeqInfo *seqs, int seq)
{
    char dir[160];

    if (seq < 1 || seq >= DINK_MAX_SEQ) {
        return PIX_SCREEN;
    }
    if (residency_is_sticky_seq(seq)) {
        return PIX_STICKY;
    }
    pack_dir(&seqs[seq], dir, sizeof(dir));
    if (dir[0] != '\0' && residency_is_always(dir)) {
        return PIX_ALWAYS;
    }
    return PIX_SCREEN;
}

static const char *pixel_class_name(int cls)
{
    if (cls == PIX_ALWAYS) {
        return "always";
    }
    if (cls == PIX_STICKY) {
        return "sticky";
    }
    return "screen";
}

/* Screen pixels only. Prefer unused; pack still cached. unused_only skips
 * live frames so a cpu_pixels miss cannot thrash two live seqs each tick. */
static int evict_slot(struct EdGfx *g, int *got, struct SeqInfo *seqs,
                      int keep_seq, int gpu_wait, int unused_only)
{
    int i, pick = -1, pick_cached = 0, pick_unused = 0;
    char dir[160];

    for (i = 0; i < *got; i++) {
        int s = g[i].seq;
        int cls, cached, unused;

        if (s == keep_seq) {
            continue;
        }
        cls = pixel_class(seqs, s);
        if (cls == PIX_ALWAYS || cls == PIX_STICKY) {
            continue;
        }
        pack_dir(&seqs[s], dir, sizeof(dir));
        cached = dir[0] != '\0' && ff_is_cached(dir);
        unused = !g[i].live;
        if (unused_only && !unused) {
            continue;
        }
        if (pick < 0 || (unused && !pick_unused) ||
            (unused == pick_unused && cached && !pick_cached)) {
            pick = i;
            pick_cached = cached;
            pick_unused = unused;
        }
    }
    if (pick < 0) {
        return -1;
    }
#ifdef _arch_dreamcast
    if (gpu_wait) {
        pvr_wait_ready();
    }
#else
    (void)gpu_wait;
#endif
    printf("edraw evict class=%s seq=%d fr=%d for seq=%d\n",
           pixel_class_name(pixel_class(seqs, g[pick].seq)), g[pick].seq,
           g[pick].frame, keep_seq);
    sprite_frame_free(&g[pick].fr);
    (*got)--;
    if (pick < *got) {
        g[pick] = g[*got];
        memset(&g[*got], 0, sizeof(g[0]));
    } else {
        memset(&g[pick], 0, sizeof(g[0]));
    }
    return 0;
}

/* A cached pack that lacks this BMP will not grow it mid-screen. */
#define DINK_BMP_MISS 64
static int g_miss_seq[DINK_BMP_MISS];
static int g_miss_fr[DINK_BMP_MISS];
static int g_nmiss;

static void miss_clear(void)
{
    g_nmiss = 0;
}

#define DINK_EDRAW_MARK_MAX 32
static int g_mark_s[DINK_EDRAW_MARK_MAX];
static int g_mark_f[DINK_EDRAW_MARK_MAX];
static int g_nmark;

void edraw_mark_need(int seq, int frame)
{
    int i;

    if (seq < 1 || frame < 1 || g_nmark >= DINK_EDRAW_MARK_MAX) {
        return;
    }
    for (i = 0; i < g_nmark; i++) {
        if (g_mark_s[i] == seq && g_mark_f[i] == frame) {
            return;
        }
    }
    g_mark_s[g_nmark] = seq;
    g_mark_f[g_nmark] = frame;
    g_nmark++;
}

static int miss_has(int seq, int frame)
{
    int i;

    for (i = 0; i < g_nmiss; i++) {
        if (g_miss_seq[i] == seq && g_miss_fr[i] == frame) {
            return 1;
        }
    }
    return 0;
}

static void miss_note(int seq, int frame)
{
    if (miss_has(seq, frame) || g_nmiss >= DINK_BMP_MISS) {
        return;
    }
    g_miss_seq[g_nmiss] = seq;
    g_miss_fr[g_nmiss] = frame;
    g_nmiss++;
}

/* Cache dir.ff even when EdGfx is full so play-path ensure can fopen-not. */
static int open_seq_pack(struct SeqInfo *seqs, int seq)
{
    char dir[160];
    struct FfFile *ff = NULL;

    if (seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ) {
        return -1;
    }
    pack_dir(&seqs[seq], dir, sizeof(dir));
    if (dir[0] == '\0') {
        return -1;
    }
    return ff_cached(dir, &ff);
}

static int load_one(struct EdGfx *g, int *got, struct SeqInfo *seqs, int seq,
                    int frame, int may_evict)
{
    static uint8_t full_noted[DINK_MAX_SEQ];

    if (edraw_find(g, *got, seq, frame) != NULL) {
        return 0;
    }
    if (miss_has(seq, frame)) {
        return -1;
    }
    (void)open_seq_pack(seqs, seq);
    if (*got >= DINK_EDGFX_MAX) {
        if (!may_evict || evict_slot(g, got, seqs, seq, 0, 0) != 0) {
            if (full_noted[seq] == 0) {
                full_noted[seq] = 1;
                printf("edraw full skip seq=%d fr=%d\n", seq, frame);
            }
            return -1;
        }
    }
    printf("edraw load seq=%d fr=%d\n", seq, frame);
    if (sprite_load_seq_frame(&seqs[seq], seq, frame, &g[*got].fr) != 0) {
        miss_note(seq, frame);
        printf("edraw skip seq=%d frame=%d\n", seq, frame);
        return -1;
    }
    g[*got].seq = seq;
    g[*got].frame = frame;
    g[*got].live = 1;
    (*got)++;
    {
        size_t need = edraw_cpu_bytes(g, *got);

        if (need > (size_t)DINK_MEM_CPU_PIXELS) {
            (*got)--;
            sprite_frame_free(&g[*got].fr);
            memset(&g[*got], 0, sizeof(g[0]));
            printf("mem refuse pool=cpu_pixels need=%u have=%u cap=%u\n",
                   (unsigned)need, (unsigned)edraw_cpu_bytes(g, *got),
                   (unsigned)DINK_MEM_CPU_PIXELS);
            return -2;
        }
    }
    return 0;
}

static void load_seq_frames(struct EdGfx *g, int *got, struct SeqInfo *seqs,
                            int seq)
{
    int nfr, f2;

    if (seq < 1 || seq >= DINK_MAX_SEQ || seqs[seq].prefix[0] == '\0') {
        return;
    }
    (void)open_seq_pack(seqs, seq);
    /* nframes is 0 until frame 1 is decoded (create_sprite seqs). */
    if (load_one(g, got, seqs, seq, 1, 1) != 0) {
        return;
    }
    nfr = ini_seq_len(seq, seqs[seq].nframes);
    for (f2 = 2; f2 <= nfr; f2++) {
        if (load_one(g, got, seqs, seq, f2, 0) != 0) {
            break;
        }
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

static int brain_needs_death_walk(int br)
{
    /* duck/pig/pill/dragon: punch changes base_walk on play-path. */
    return br == 3 || br == 4 || br == 9 || br == 10;
}

static void push_walk_frames(int *ns, int *nf, int *n, struct SeqInfo *seqs,
                             int br, int base, int all_frames)
{
    int ws[6], nw, w, f2, nfr;

    walk_seqs_for_brain(br, base, ws, &nw);
    for (w = 0; w < nw; w++) {
        if (ws[w] < 1 || ws[w] >= DINK_MAX_SEQ ||
            seqs[ws[w]].prefix[0] == '\0') {
            continue;
        }
        need_push(ns, nf, n, ws[w], 1);
        if (!all_frames) {
            continue;
        }
        nfr = ini_seq_len(ws[w], seqs[ws[w]].nframes);
        for (f2 = 1; f2 <= nfr; f2++) {
            need_push(ns, nf, n, ws[w], f2);
        }
    }
}

/* Default corpse seq 164. People with hitpoints (mom) must not pin
 * graphics/effects/magic/dir.ff (~600 KB) for the session — that pack
 * plus village dir.ff OOMs 16 MB after a short walk. */
static int screen_wants_die(const struct EditorSprite *sp, int vision)
{
    int i;

    for (i = 1; i <= 100; i++) {
        int br;

        if ((int)sp[i].type != 1 || !editor_sprite_on_vision(&sp[i], vision)) {
            continue;
        }
        br = (int)sp[i].brain;
        if ((int)sp[i].base_die > 0 || br == 3 || br == 4 || br == 9 ||
            br == 10) {
            return 1;
        }
    }
    return 0;
}

/* Sticky seq (164): decode all frames, then drop the pack. Pixels stay. */
static int seq_complete(struct EdGfx *g, int n, struct SeqInfo *seqs, int seq)
{
    int nfr, f;

    if (g == NULL || seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ ||
        seqs[seq].prefix[0] == '\0') {
        return 0;
    }
    nfr = ini_seq_len(seq, seqs[seq].nframes);
    if (nfr < 1) {
        return 0;
    }
    for (f = 1; f <= nfr; f++) {
        if (edraw_find(g, n, seq, f) == NULL) {
            return 0;
        }
    }
    return 1;
}

static void drop_seq_pack(const struct SeqInfo *seqs, int seq)
{
    char dir[160];

    if (seqs == NULL || seq < 1) {
        return;
    }
    pack_dir(&seqs[seq], dir, sizeof(dir));
    if (dir[0] != '\0') {
        ff_cache_release(dir);
    }
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
    miss_clear();
    residency_swap_begin();
    printf("edraw in sprite1 seq=%d y=%d act=%d keep=%d\n", (int)sp[1].seq,
           (int)sp[1].y, (int)sp[1].active, *n);
    for (i = 1; i <= 8; i++) {
        printf("edraw slot %d act=%d seq=%d fr=%d xy=%d,%d vis=%d\n", i,
               (int)sp[i].active, (int)sp[i].seq, (int)sp[i].frame,
               (int)sp[i].x, (int)sp[i].y, (int)sp[i].vision);
    }
    {
        int need_s[DINK_EDGFX_MAX], need_f[DINK_EDGFX_MAX], nneed = 0, old, k;

        /* Combat pixels first on die screens: 164, then duck 110/120.
         * Do not prepend leftover sticky 164 onto a non-die screen (start
         * house unique already fills the table). Keep those pixels live=0. */
        if (residency_is_sticky_seq(164) && seqs[164].prefix[0] != '\0' &&
            screen_wants_die(sp, vision)) {
            int nfr, f2;

            need_push(need_s, need_f, &nneed, 164, 1);
            nfr = ini_seq_len(164, seqs[164].nframes);
            for (f2 = 2; f2 <= nfr; f2++) {
                need_push(need_s, need_f, &nneed, 164, f2);
            }
        }
        for (i = 1; i <= 100; i++) {
            int br;

            if ((int)sp[i].type != 1 ||
                !editor_sprite_on_vision(&sp[i], vision)) {
                continue;
            }
            br = (int)sp[i].brain;
            if (br == 3) {
                push_walk_frames(need_s, need_f, &nneed, seqs, 3, 110, 1);
                push_walk_frames(need_s, need_f, &nneed, seqs, 3, 120, 1);
            }
        }
        /* Blood pack is ~28 KB. Queue frame 1 with combat pixels so Ethel /
         * pig-pen occupancy cannot drop it off the need list. */
        if (screen_wants_die(sp, vision)) {
            int bseq;

            for (bseq = 187; bseq <= 189; bseq++) {
                if (bseq < DINK_MAX_SEQ && seqs[bseq].prefix[0] != '\0') {
                    need_push(need_s, need_f, &nneed, bseq, 1);
                }
            }
        }
        for (i = 1; i <= 100; i++) {
            int seq, fr, br;

            if (!editor_sprite_draw(&sp[i], vision)) {
                continue;
            }
            seq = (int)sp[i].seq;
            fr = (int)sp[i].frame < 1 ? 1 : (int)sp[i].frame;
            if (seq < 1 || seq >= DINK_MAX_SEQ || seqs[seq].prefix[0] == '\0') {
                continue;
            }
            need_push(need_s, need_f, &nneed, seq, fr);
            br = (int)sp[i].brain;
            if ((int)sp[i].type == 1 && br == 6) {
                int nfr, f2;

                nfr = ini_seq_len(seq, seqs[seq].nframes);
                for (f2 = 1; f2 <= nfr; f2++) {
                    need_push(need_s, need_f, &nneed, seq, f2);
                }
            }
            if ((int)sp[i].type == 1 && brain_needs_death_walk(br)) {
                push_walk_frames(need_s, need_f, &nneed, seqs, br,
                                 (int)sp[i].base_walk, 0);
            }
            if ((int)sp[i].type == 1 && br == 16) {
                push_walk_frames(need_s, need_f, &nneed, seqs, 16,
                                 (int)sp[i].base_walk, 0);
            }
        }
        /* create_sprite during screen MAIN (Chealse) is not an editor row.
         * swap_begin already demoted her pack to Prev; keep it Screen. */
        for (k = 0; k < g_nmark; k++) {
            need_push(need_s, need_f, &nneed, g_mark_s[k], g_mark_f[k]);
        }
        g_nmark = 0;
        /* Re-mark this Screen's packs before drop. Aged Prev (two screens
         * old) still occupies file_blob until swap_end — fopen Screen then
         * misses pigs on the first 407 visit. */
        for (k = 0; k < nneed; k++) {
            char dir[160];

            pack_dir(&seqs[need_s[k]], dir, sizeof(dir));
            if (dir[0] != '\0') {
                residency_touch(dir);
            }
        }
        ff_cache_drop_unpinned();
        old = *n;
        if (old < 0) {
            old = 0;
        }
        if (old > DINK_EDGFX_MAX) {
            old = DINK_EDGFX_MAX;
        }
        /* Free unused Screen pixels. Sticky 164 stays (live=0) so explode
         * does not reopen magic/dir.ff. Byte cap is cpu_pixels, not slots. */
        i = 0;
        while (i < old) {
            int keep = 0;
            int sticky = residency_is_sticky_seq(g[i].seq);

            for (k = 0; k < nneed; k++) {
                if (g[i].seq == need_s[k] && g[i].frame == need_f[k]) {
                    keep = 1;
                    break;
                }
            }
            if (keep) {
                g[i].live = 1;
                i++;
            } else if (sticky) {
                g[i].live = 0;
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
            (void)load_one(g, &got, seqs, need_s[k], need_f[k], 0);
        }
        /* After frame 1 decode, seq.nframes is known. Duck death before
         * people/fireplace leftover fills. */
        for (i = 1; i <= 100; i++) {
            int br, ws[6], nw, w;

            if ((int)sp[i].type != 1 ||
                !editor_sprite_on_vision(&sp[i], vision)) {
                continue;
            }
            br = (int)sp[i].brain;
            if (br != 3) {
                continue;
            }
            walk_seqs_for_brain(3, 110, ws, &nw);
            for (w = 0; w < nw; w++) {
                load_seq_frames(g, &got, seqs, ws[w]);
            }
            walk_seqs_for_brain(3, 120, ws, &nw);
            for (w = 0; w < nw; w++) {
                load_seq_frames(g, &got, seqs, ws[w]);
            }
        }
        for (i = 1; i <= 100; i++) {
            int br;

            if ((int)sp[i].type != 1 ||
                !editor_sprite_on_vision(&sp[i], vision)) {
                continue;
            }
            br = (int)sp[i].brain;
            if (br == 6) {
                load_seq_frames(g, &got, seqs, (int)sp[i].seq);
            }
            if ((int)sp[i].is_warp && (int)sp[i].parm_seq > 0) {
                load_seq_frames(g, &got, seqs, (int)sp[i].parm_seq);
            }
            if (brain_needs_death_walk(br)) {
                int ws[6], nw, w;

                /* Same as people: frame 1 opens the pack. Play-path ensure. */
                walk_seqs_for_brain(br, (int)sp[i].base_walk, ws, &nw);
                for (w = 0; w < nw; w++) {
                    (void)load_one(g, &got, seqs, ws[w], 1, 0);
                }
            }
            if (br == 16) {
                int ws[6], nw, w;

                walk_seqs_for_brain(16, (int)sp[i].base_walk, ws, &nw);
                for (w = 0; w < nw; w++) {
                    (void)load_one(g, &got, seqs, ws[w], 1, 0);
                }
            }
        }
        if (residency_is_sticky_seq(164) && seqs[164].prefix[0] != '\0') {
            if (screen_wants_die(sp, vision)) {
                load_seq_frames(g, &got, seqs, 164);
            }
            if (seq_complete(g, got, seqs, 164)) {
                drop_seq_pack(seqs, 164);
            }
        }
    }
    ff_cache_drop_unpinned();
    printf("blob bytes %u unique %d\n", (unsigned)dink_blob_bytes(), got);
    mem_log("edraw", edraw_cpu_bytes(g, got), got, 0, 0);
    memcpy(spr, sp, 101u * sizeof(*sp));
    free(sp);
    *n = got;
    return 0;
}

#ifdef _arch_dreamcast
static void upload_if_cpu(struct SpriteFrame *f)
{
    if (f != NULL && f->tex == NULL && f->argb1555 != NULL) {
        (void)sprite_upload_pvr(f);
    }
}
#else
static void upload_if_cpu(struct SpriteFrame *f)
{
    (void)f;
}
#endif

int edraw_ensure_frame(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq,
                       int frame)
{
    char dir[160];
    int got;
    struct SpriteFrame *hit;

    if (g == NULL || n == NULL || seqs == NULL || seq < 1 ||
        seq >= DINK_MAX_SEQ || frame < 1) {
        return -1;
    }
    got = *n;
    if (got < 0) {
        got = 0;
    }
    hit = edraw_find(g, got, seq, frame);
    if (hit != NULL) {
        /* preload_seq may decode CPU during play; draw skips tex==NULL. */
        upload_if_cpu(hit);
        return 0;
    }
    if (miss_has(seq, frame)) {
        return -1;
    }
    /* Play path: decode only from a pack already opened at screen load. */
    pack_dir(&seqs[seq], dir, sizeof(dir));
    if (dir[0] == '\0' || !ff_is_cached(dir)) {
        static uint8_t noted[DINK_MAX_SEQ];

        if (noted[seq] == 0) {
            noted[seq] = 1;
            printf("edraw skip seq=%d fr=%d (pack not cached)\n", seq, frame);
        }
        return -1;
    }
    {
        int waited = 0;

        if (got >= DINK_EDGFX_MAX) {
            if (evict_slot(g, &got, seqs, seq, 1, 0) != 0) {
                static uint8_t full_noted[DINK_MAX_SEQ];

                if (full_noted[seq] == 0) {
                    full_noted[seq] = 1;
                    printf("edraw full skip seq=%d fr=%d\n", seq, frame);
                }
                *n = got;
                return -1;
            }
            waited = 1;
        }
        {
            int rc, tries = 0;

            for (;;) {
                rc = load_one(g, &got, seqs, seq, frame, 1);
                if (rc == 0) {
                    break;
                }
                /* Unused Screen only. Evicting a live frame thrashes with
                 * the next sprite on this tick (Milder + pigs). */
                if (rc == -2 && tries++ < DINK_EDGFX_MAX &&
                    evict_slot(g, &got, seqs, seq, waited ? 0 : 1, 1) == 0) {
                    waited = 1;
                    continue;
                }
                if (rc == -2) {
                    miss_note(seq, frame);
                }
                *n = got;
                return -1;
            }
        }
    }
#ifdef _arch_dreamcast
    (void)sprite_upload_pvr(&g[got - 1].fr);
#endif
    *n = got;
    return 0;
}

void edraw_load_seq(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq)
{
    int got, i;

    if (g == NULL || n == NULL || seqs == NULL) {
        return;
    }
    got = *n;
    if (got < 0) {
        got = 0;
    }
    load_seq_frames(g, &got, seqs, seq);
    for (i = 0; i < got; i++) {
        if (g[i].seq == seq) {
            upload_if_cpu(&g[i].fr);
        }
    }
    *n = got;
}

void edraw_load_frame(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq,
                      int frame)
{
    int got;
    struct SpriteFrame *hit;

    if (g == NULL || n == NULL || seqs == NULL) {
        return;
    }
    got = *n;
    if (got < 0) {
        got = 0;
    }
    (void)load_one(g, &got, seqs, seq, frame, 1);
    hit = edraw_find(g, got, seq, frame);
    upload_if_cpu(hit);
    *n = got;
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
