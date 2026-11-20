/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"

#include "audio.h"
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
        t += sprite_cpu_bytes(&g[i].fr);
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

/* Bumped per play tick (edraw_live_begin); EdGfx.used = last drawn. */
static unsigned g_draw_tick = 1;
/* edraw_prefetch in progress: no eviction, no refusal memo, own log tag. */
static int g_prefetching;

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
                      int keep_seq, int keep_frame, int gpu_wait,
                      int unused_only)
{
    int i, pick = -1, pick_cached = 0, pick_unused = 0;
    char dir[160];

    for (i = 0; i < *got; i++) {
        int s = g[i].seq;
        int cls, cached, unused;

        if (s == keep_seq && g[i].frame == keep_frame) {
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
        unsigned t0 = dink_io_now_us();

        pvr_wait_ready();
        dink_io_note_gpu_wait(dink_io_now_us() - t0);
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

void edraw_live_begin(struct EdGfx *g, int n, struct SeqInfo *seqs)
{
    int i;

    if (g == NULL || seqs == NULL) {
        return;
    }
    g_draw_tick++;
    for (i = 0; i < n; i++) {
        if (pixel_class(seqs, g[i].seq) == PIX_SCREEN) {
            g[i].live = 0;
        }
    }
}

void edraw_reap_unused(struct EdGfx *g, int *n, struct SeqInfo *seqs)
{
    int i;

    if (g == NULL || n == NULL || seqs == NULL) {
        return;
    }
    i = 0;
    while (i < *n) {
        int drop = 0;

        if (pixel_class(seqs, g[i].seq) == PIX_SCREEN && !g[i].live) {
            /* CPU-only: drop. Keep uploaded tex until enter-path unique or
             * evict_slot. Bar knights switch 293 walk ↔ 297 attack; seq-live
             * reap made walk vanish for a SEEK_SET. */
            drop = (g[i].fr.tex == NULL);
        }
        if (drop) {
            sprite_frame_free(&g[i].fr);
            (*n)--;
            if (i < *n) {
                g[i] = g[*n];
                memset(&g[*n], 0, sizeof(g[0]));
            } else {
                memset(&g[i], 0, sizeof(g[0]));
            }
        } else {
            i++;
        }
    }
}

void edraw_live_touch(struct EdGfx *g, int n, int seq, int frame)
{
    int i;

    if (g == NULL || seq < 1 || frame < 1) {
        return;
    }
    for (i = 0; i < n; i++) {
        if (g[i].seq == seq && g[i].frame == frame) {
            g[i].live = 1;
            g[i].used = g_draw_tick;
            return;
        }
    }
}

int edraw_loop_next_frame(const struct SeqInfo *seqs, int seq, int fr)
{
    int nfr, dseq, dfr;

    if (seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ) {
        return 1;
    }
    if (fr < 1) {
        fr = 1;
    }
    nfr = ini_seq_len(seq, seqs[seq].nframes);
    if (nfr < 1) {
        return fr;
    }
    if (fr >= nfr || ini_resolve_frame(seq, fr + 1, &dseq, &dfr) != 0) {
        return 1;
    }
    return fr + 1;
}

static void need_push_loop(int *ns, int *nf, int *n, const struct SeqInfo *seqs,
                           int seq, int fr)
{
    int nxt;

    if (fr < 1) {
        fr = 1;
    }
    need_push(ns, nf, n, seq, fr);
    nxt = edraw_loop_next_frame(seqs, seq, fr);
    if (nxt != fr) {
        need_push(ns, nf, n, seq, nxt);
    }
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

/* ---- sprite_tex (VRAM) working set, 14.4c class victims ----
 * Uploaded Screen textures stay after their frame stops drawing (reloading
 * every tick is worse). An upload out of PVR memory makes room:
 *  1. Screen textures not live this tick whose pack is still cached (they
 *     decode again), least recently drawn first, size as tie-break; only
 *     if they (with what is free) can cover the need.
 *  2. Else the relief hook once (Dink's other seqs, pspr), told the need.
 *  3. Else refuse (mem refuse pool=sprite_tex), drop the frame, and do not
 *     retry until a texture was freed AND enough VRAM is free for it
 *     (sprite_vram_retry_ok), so a full VRAM does not re-read it per tick.
 * Never Always/Sticky, never live. DinkC preload_seq/load_frame callbacks
 * can run before this tick's live marking and so see last tick's set; a
 * frame evicted that way reloads once when drawn. All callers run after
 * the last scene finished (top-of-play wait, draw_screen wait, or before
 * the first scene). */
static size_t (*g_vram_relief)(size_t need);
#ifdef _arch_dreamcast
static int (*g_upload)(struct SpriteFrame *f) = sprite_upload_pvr;
#else
static int (*g_upload)(struct SpriteFrame *f);
#endif

void edraw_set_vram_relief(size_t (*fn)(size_t need))
{
    g_vram_relief = fn;
}

void edraw_set_upload(int (*fn)(struct SpriteFrame *f))
{
    g_upload = fn;
}

#define DINK_VRAM_REFUSE 32
static int g_ref_seq[DINK_VRAM_REFUSE];
static int g_ref_fr[DINK_VRAM_REFUSE];
static unsigned g_ref_gen[DINK_VRAM_REFUSE];
static size_t g_ref_need[DINK_VRAM_REFUSE];
static int g_nref, g_ref_next;
static unsigned g_nrefused;

static void refuse_clear(void)
{
    g_nref = 0;
    g_ref_next = 0;
}

static int refuse_find(int seq, int frame)
{
    int i;

    for (i = 0; i < g_nref; i++) {
        if (g_ref_seq[i] == seq && g_ref_fr[i] == frame) {
            return i;
        }
    }
    return -1;
}

/* 1 = still refused: nothing freed since, or not enough VRAM free. */
static int refuse_blocks(int seq, int frame)
{
    int i = refuse_find(seq, frame);

    return i >= 0 && !sprite_vram_retry_ok(g_ref_need[i], g_ref_gen[i]);
}

static void refuse_note(int seq, int frame, size_t need)
{
    int i = refuse_find(seq, frame);

    if (i < 0) {
        if (g_nref < DINK_VRAM_REFUSE) {
            i = g_nref++;
        } else {
            i = g_ref_next; /* ring: overwrite the oldest entry */
            g_ref_next = (g_ref_next + 1) % DINK_VRAM_REFUSE;
        }
    }
    g_ref_seq[i] = seq;
    g_ref_fr[i] = frame;
    g_ref_need[i] = need;
    g_ref_gen[i] = sprite_pvr_free_gen();
}

unsigned edraw_vram_refused(void)
{
    return g_nrefused;
}

static void remove_at(struct EdGfx *g, int *got, int i)
{
    if (i < 0 || i >= *got) {
        return;
    }
    sprite_frame_free(&g[i].fr);
    (*got)--;
    if (i < *got) {
        g[i] = g[*got];
    }
    memset(&g[*got], 0, sizeof(g[0]));
}

static size_t sat_add(size_t a, size_t b)
{
    return a > (size_t)-1 - b ? (size_t)-1 : a + b;
}

static size_t tex_size(const struct SpriteFrame *f)
{
    return sprite_tex_bytes(f);
}

static int vram_evictable(const struct EdGfx *e, struct SeqInfo *seqs,
                          int keep_seq, int keep_frame)
{
    char dir[160];

    if (e->live || e->fr.tex == NULL ||
        (e->seq == keep_seq && e->frame == keep_frame) ||
        pixel_class(seqs, e->seq) != PIX_SCREEN) {
        return 0;
    }
    pack_dir(&seqs[e->seq], dir, sizeof(dir));
    return dir[0] != '\0' && ff_is_cached(dir); /* can come back */
}

/* Least recently drawn evictable texture; larger on a tie. */
static int vram_victim(struct EdGfx *g, int got, struct SeqInfo *seqs,
                       int keep_seq, int keep_frame)
{
    int i, pick = -1;

    for (i = 0; i < got; i++) {
        if (!vram_evictable(&g[i], seqs, keep_seq, keep_frame)) {
            continue;
        }
        if (pick < 0 || g[i].used < g[pick].used ||
            (g[i].used == g[pick].used &&
             tex_size(&g[i].fr) > tex_size(&g[pick].fr))) {
            pick = i;
        }
    }
    return pick;
}

static size_t vram_evictable_bytes(struct EdGfx *g, int got,
                                   struct SeqInfo *seqs, int keep_seq,
                                   int keep_frame)
{
    size_t b = 0;
    int i;

    for (i = 0; i < got; i++) {
        if (vram_evictable(&g[i], seqs, keep_seq, keep_frame)) {
            b += tex_size(&g[i].fr);
        }
    }
    return b;
}

size_t edraw_vram_room(struct EdGfx *g, int *n, struct SeqInfo *seqs,
                       size_t need)
{
    size_t freed = 0;

    if (g == NULL || n == NULL || seqs == NULL) {
        return 0;
    }
    if (sprite_vram_avail() >= need) {
        return 0;
    }
    if (sat_add(sprite_vram_avail(), vram_evictable_bytes(g, *n, seqs, 0, 0)) <
        need) {
        return 0; /* would empty the screen and still not fit */
    }
    while (sprite_vram_avail() < need) {
        int v = vram_victim(g, *n, seqs, 0, 0);

        if (v < 0) {
            break;
        }
        printf("edraw vram evict seq=%d fr=%d (room %u)\n", g[v].seq,
               g[v].frame, (unsigned)need);
        freed += tex_size(&g[v].fr);
        remove_at(g, n, v);
    }
    return freed;
}

size_t edraw_tex_bytes(const struct EdGfx *g, int n)
{
    size_t b = 0;
    int i;

    for (i = 0; g != NULL && i < n; i++) {
        if (g[i].fr.tex != NULL) {
            b += sprite_tex_bytes(&g[i].fr);
        }
    }
    return b;
}

size_t edraw_upload_need(const struct EdGfx *g, int n)
{
    size_t b = 0;
    int i;

    for (i = 0; g != NULL && i < n; i++) {
        if (g[i].live && g[i].fr.tex == NULL && sprite_has_cpu(&g[i].fr)) {
            b += tex_size(&g[i].fr);
        }
    }
    return b;
}

size_t edraw_vram_evictable(struct EdGfx *g, int n, struct SeqInfo *seqs)
{
    if (g == NULL || seqs == NULL) {
        return 0;
    }
    return vram_evictable_bytes(g, n, seqs, 0, 0);
}

size_t edraw_vram_free_one(struct EdGfx *g, int *n, struct SeqInfo *seqs)
{
    size_t b;
    int v;

    if (g == NULL || n == NULL || seqs == NULL) {
        return 0;
    }
    v = vram_victim(g, *n, seqs, 0, 0);
    if (v < 0) {
        return 0;
    }
    b = tex_size(&g[v].fr);
    printf("edraw vram evict seq=%d fr=%d (overlay)\n", g[v].seq, g[v].frame);
    remove_at(g, n, v);
    return b;
}

static int find_index(const struct EdGfx *g, int got, int seq, int frame)
{
    int i;

    for (i = 0; i < got; i++) {
        if (g[i].seq == seq && g[i].frame == frame) {
            return i;
        }
    }
    return -1;
}

/* Upload (seq,frame) if it holds CPU pixels and no texture, making room
 * per the policy above. 0 = has a texture (or nothing to upload); -1 =
 * refused and removed. */
static int upload_with_room(struct EdGfx *g, int *got, struct SeqInfo *seqs,
                            int seq, int frame)
{
    struct SpriteFrame *hit = edraw_find(g, *got, seq, frame);
    int relief = 0, own_room;
    size_t need;

    if (hit == NULL || g_upload == NULL) {
        return 0;
    }
    need = tex_size(hit);
    for (;;) {
        size_t room;
        int v;

        if (hit->tex != NULL || !sprite_has_cpu(hit)) {
            break;
        }
        /* Short of VRAM: make room before trying (a failed pvr_mem_malloc
         * logs on KOS and costs SCIF time). */
        if (sprite_vram_avail() >= need && g_upload(hit) == 0) {
            break;
        }
        if (g_prefetching) {
            /* Lookahead never evicts and never records a refusal (that
             * would block the real draw-time load): just drop it. */
            remove_at(g, got, find_index(g, *got, seq, frame));
            return -1;
        }
        /* Edraw victims only when they (with free VRAM) cover it: do not
         * empty the screen for a frame that is refused anyway. When they
         * fall short, ask Dink for the shortfall first, then re-check. */
        room = sat_add(sprite_vram_avail(),
                       vram_evictable_bytes(g, *got, seqs, seq, frame));
        if (room < need && !relief && g_vram_relief != NULL) {
            relief = 1;
            if (g_vram_relief(need - room) > 0) {
                continue; /* retry the upload, then re-check room */
            }
        }
        own_room = room >= need;
        v = own_room ? vram_victim(g, *got, seqs, seq, frame) : -1;
        if (v >= 0) {
            printf("edraw vram evict seq=%d fr=%d for seq=%d fr=%d\n",
                   g[v].seq, g[v].frame, seq, frame);
            remove_at(g, got, v);
            hit = edraw_find(g, *got, seq, frame);
            if (hit == NULL) {
                return -1;
            }
            continue;
        }
        if (!relief && g_vram_relief != NULL && g_vram_relief(need) > 0) {
            relief = 1;
            continue;
        }
        printf("mem refuse pool=sprite_tex seq=%d fr=%d bytes=%u%s\n", seq,
               frame, (unsigned)need,
               sprite_vram_avail() >= need ? " (fragmented)" : "");
        g_nrefused++;
        refuse_note(seq, frame, need);
        remove_at(g, got, find_index(g, *got, seq, frame));
        return -1;
    }
    if (hit->tex != NULL) {
        sprite_drop_cpu(hit);
    }
    return 0;
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
    if (ff_cached(dir, &ff) != 0) {
        return -1;
    }
    residency_live_keep(dir);
    return 0;
}

static int upload_with_room(struct EdGfx *g, int *got, struct SeqInfo *seqs,
                            int seq, int frame);
static int refuse_blocks(int seq, int frame);

static int load_one_body(struct EdGfx *g, int *got, struct SeqInfo *seqs,
                         int seq, int frame, int may_evict)
{
    static uint8_t full_noted[DINK_MAX_SEQ];

    (void)open_seq_pack(seqs, seq);
    if (*got >= DINK_EDGFX_MAX) {
        if (!may_evict || evict_slot(g, got, seqs, seq, frame, 0, 0) != 0) {
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
    g[*got].used = g_draw_tick;
    (*got)++;
    /* VRAM eviction may move slots; address the frame by seq/frame. */
    if (upload_with_room(g, got, seqs, seq, frame) != 0) {
        audio_music_pump();
        return -3;
    }
    audio_music_pump();
    /* Play-path reaps !live Screen after every sprite is touched. */
    {
        size_t need = edraw_cpu_bytes(g, *got);

        if (need > (size_t)DINK_MEM_CPU_PIXELS) {
            remove_at(g, got, find_index(g, *got, seq, frame));
            printf("mem refuse pool=cpu_pixels need=%u have=%u cap=%u\n",
                   (unsigned)need, (unsigned)edraw_cpu_bytes(g, *got),
                   (unsigned)DINK_MEM_CPU_PIXELS);
            return -2;
        }
    }
    return 0;
}

/* Diagnostic window covers pack open, eviction, decode and upload, so a
 * play-path frame line owns every disc op it caused (ioplay). */
static int load_one(struct EdGfx *g, int *got, struct SeqInfo *seqs, int seq,
                    int frame, int may_evict)
{
    struct DinkIoStats io0, io1;
    char outer[64], ctx[32];
    unsigned t0;
    int rc;

    if (edraw_find(g, *got, seq, frame) != NULL) {
        return 0;
    }
    if (miss_has(seq, frame) || refuse_blocks(seq, frame)) {
        return -1;
    }
    t0 = dink_io_now_us();
    dink_io_stats(&io0);
    dink_io_ctx_save(outer, sizeof(outer));
    snprintf(ctx, sizeof(ctx), "%s seq=%d fr=%d",
             g_prefetching ? "prefetch" : "edraw", seq, frame);
    dink_io_ctx(ctx);
    rc = load_one_body(g, got, seqs, seq, frame, may_evict);
    dink_io_ctx(outer);
    dink_io_stats(&io1);
    dink_io_note_frame(g_prefetching ? "prefetch" : "edraw",
                       rc == 0 ? seq : -seq, frame,
                       dink_io_now_us() - t0, io1.ops - io0.ops);
    return rc;
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

static void load_loop_frames(struct EdGfx *g, int *got, struct SeqInfo *seqs,
                             int seq, int fr)
{
    int nxt;

    if (fr < 1) {
        fr = 1;
    }
    (void)load_one(g, got, seqs, seq, fr, 1);
    nxt = edraw_loop_next_frame(seqs, seq, fr);
    if (nxt != fr) {
        (void)load_one(g, got, seqs, seq, nxt, 1);
    }
}

/* people/pig: +1,3,7,9. duck: those plus 4,6. pill/dragon: all 8 dirs
 * (Bonca walk 531+cardinal is 533/537/539; attack 540+cardinal 542..). */
static void walk_seqs_for_brain(int brain, int base, int *out, int *n)
{
    static const int diag[4] = {1, 3, 7, 9};
    static const int all8[8] = {1, 2, 3, 4, 6, 7, 8, 9};
    static const int duck[6] = {1, 3, 4, 6, 7, 9};
    const int *d = diag;
    int nd = 4, i;

    *n = 0;
    if (base < 0) {
        return;
    }
    if (brain == 9 || brain == 10) {
        d = all8;
        nd = 8;
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
    int ws[8], nw, w, f2, nfr;

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
    refuse_clear();
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
        /* create_sprite MAIN (439 crowd / Chealse) is not an editor row.
         * Fopen those packs while Prev still has slack. Last on the list
         * lost maiden/blue after peasant2 (file_blob cap; do not pin 257). */
        for (k = 0; k < g_nmark; k++) {
            need_push(need_s, need_f, &nneed, g_mark_s[k], g_mark_f[k]);
        }
        g_nmark = 0;
        /* Combat type-1 first so Bonca walk/attack and duck death open
         * before type-0 decor (cave dragon 212 ~842 KB). */
        for (k = 0; k < 3; k++) {
            for (i = 1; i <= 100; i++) {
                int seq, fr, br, pass;

                if (!editor_sprite_draw(&sp[i], vision)) {
                    continue;
                }
                br = (int)sp[i].brain;
                if ((int)sp[i].type == 1 && brain_needs_death_walk(br)) {
                    pass = 0;
                } else if ((int)sp[i].type == 1 && br == 16) {
                    pass = 0;
                } else if ((int)sp[i].type == 1) {
                    pass = 1;
                } else {
                    pass = 2;
                }
                if (pass != k) {
                    continue;
                }
                seq = (int)sp[i].seq;
                fr = (int)sp[i].frame < 1 ? 1 : (int)sp[i].frame;
                if (seq < 1 || seq >= DINK_MAX_SEQ ||
                    seqs[seq].prefix[0] == '\0') {
                    continue;
                }
                need_push(need_s, need_f, &nneed, seq, fr);
                if ((int)sp[i].type == 1 && br == 6) {
                    need_push_loop(need_s, need_f, &nneed, seqs, seq, fr);
                }
                if ((int)sp[i].type == 1 && brain_needs_death_walk(br)) {
                    push_walk_frames(need_s, need_f, &nneed, seqs, br,
                                     (int)sp[i].base_walk, 0);
                    if ((int)sp[i].base_attack > 0) {
                        push_walk_frames(need_s, need_f, &nneed, seqs, br,
                                         (int)sp[i].base_attack, 0);
                    }
                    if ((int)sp[i].base_die > 0) {
                        push_walk_frames(need_s, need_f, &nneed, seqs, br,
                                         (int)sp[i].base_die, 0);
                    }
                }
                if ((int)sp[i].type == 1 && br == 16) {
                    push_walk_frames(need_s, need_f, &nneed, seqs, 16,
                                     (int)sp[i].base_walk, 0);
                }
            }
        }
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
        /* Drop Prev *after* this Screen's SEEK_SET. fclose of innwalls
         * then pread a live trees FILE* wedged map 498 (west of 499). */
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
            int br, ws[8], nw, w;

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
                int fr = (int)sp[i].frame < 1 ? 1 : (int)sp[i].frame;

                load_loop_frames(g, &got, seqs, (int)sp[i].seq, fr);
            }
            if ((int)sp[i].is_warp && (int)sp[i].parm_seq > 0) {
                load_seq_frames(g, &got, seqs, (int)sp[i].parm_seq);
            }
            if (brain_needs_death_walk(br)) {
                int ws[8], nw, w;

                /* Same as people: frame 1 opens the pack. Play-path ensure. */
                walk_seqs_for_brain(br, (int)sp[i].base_walk, ws, &nw);
                for (w = 0; w < nw; w++) {
                    (void)load_one(g, &got, seqs, ws[w], 1, 0);
                }
                if ((int)sp[i].base_attack > 0) {
                    walk_seqs_for_brain(br, (int)sp[i].base_attack, ws, &nw);
                    for (w = 0; w < nw; w++) {
                        (void)load_one(g, &got, seqs, ws[w], 1, 0);
                    }
                }
                if ((int)sp[i].base_die > 0) {
                    walk_seqs_for_brain(br, (int)sp[i].base_die, ws, &nw);
                    for (w = 0; w < nw; w++) {
                        (void)load_one(g, &got, seqs, ws[w], 1, 0);
                    }
                }
            }
            if (br == 16) {
                int ws[8], nw, w;

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
        ff_cache_drop_unpinned();
    }
    ff_cache_drop_unpinned();
    printf("blob bytes %u unique %d\n", (unsigned)dink_blob_bytes(), got);
    mem_log("edraw", edraw_cpu_bytes(g, got), got, 0, 0);
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
        int rc;

        /* preload_seq may decode CPU during play; draw skips tex==NULL. */
        rc = upload_with_room(g, &got, seqs, seq, frame);
        *n = got;
        return rc;
    }
    if (miss_has(seq, frame) || refuse_blocks(seq, frame)) {
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
            if (evict_slot(g, &got, seqs, seq, frame, 1, 0) != 0) {
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
                 * the next sprite on this tick (Milder + pigs). A byte-cap
                 * miss is not a missing BMP — do not miss_note. */
                if (rc == -2 && tries++ < DINK_EDGFX_MAX &&
                    evict_slot(g, &got, seqs, seq, frame, waited ? 0 : 1,
                               1) == 0) {
                    waited = 1;
                    continue;
                }
                *n = got;
                return -1;
            }
        }
    }
    *n = got;
    return 0;
}

/* A lookahead that loaded and could not keep its frame (upload or
 * cpu_pixels): skip it until some texture is freed, so a tight VRAM does
 * not re-read and drop the same frame every tick. Not a refusal: the real
 * draw-time load still tries. */
#define DINK_PREFETCH_MISS 8
static int g_pfm_seq[DINK_PREFETCH_MISS], g_pfm_fr[DINK_PREFETCH_MISS];
static unsigned g_pfm_gen[DINK_PREFETCH_MISS];
static int g_pfm_next;

static int prefetch_missed(int seq, int frame)
{
    int i;

    for (i = 0; i < DINK_PREFETCH_MISS; i++) {
        if (g_pfm_seq[i] == seq && g_pfm_fr[i] == frame &&
            g_pfm_gen[i] == sprite_pvr_free_gen()) {
            return 1;
        }
    }
    return 0;
}

int edraw_prefetch(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq,
                   int frame)
{
    char dir[160];
    int got, rc;

    if (g == NULL || n == NULL || seqs == NULL || seq < 1 ||
        seq >= DINK_MAX_SEQ || frame < 1) {
        return -1;
    }
    got = *n < 0 ? 0 : *n;
    if (edraw_find(g, got, seq, frame) != NULL) {
        return 0;
    }
    if (miss_has(seq, frame) || refuse_blocks(seq, frame) ||
        prefetch_missed(seq, frame)) {
        return -1;
    }
    pack_dir(&seqs[seq], dir, sizeof(dir));
    if (dir[0] == '\0' || !ff_is_cached(dir)) {
        return -1; /* play path never opens a pack */
    }
    /* Spare room only: a lookahead must not displace anything. */
    if (got >= DINK_EDGFX_MAX - DINK_PREFETCH_SLOT_SPARE ||
        sprite_vram_avail() < (size_t)DINK_PREFETCH_VRAM_SPARE ||
        edraw_cpu_bytes(g, got) + (size_t)DINK_PREFETCH_VRAM_SPARE >
            (size_t)DINK_MEM_CPU_PIXELS) {
        return -1;
    }
    g_prefetching = 1;
    rc = load_one(g, &got, seqs, seq, frame, 0);
    g_prefetching = 0;
    *n = got;
    if (rc != 0 && !miss_has(seq, frame)) {
        g_pfm_seq[g_pfm_next] = seq;
        g_pfm_fr[g_pfm_next] = frame;
        g_pfm_gen[g_pfm_next] = sprite_pvr_free_gen();
        g_pfm_next = (g_pfm_next + 1) % DINK_PREFETCH_MISS;
    }
    return rc == 0 ? 1 : -1;
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
    /* A refusal or eviction reorders slots: rescan from the start. */
    for (i = 0; i < got; i++) {
        if (g[i].seq == seq && g[i].fr.tex == NULL &&
            sprite_has_cpu(&g[i].fr) && g_upload != NULL) {
            int before = got;

            (void)upload_with_room(g, &got, seqs, seq, g[i].frame);
            if (got != before) {
                i = -1;
            }
        }
    }
    *n = got;
}

void edraw_load_frame(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq,
                      int frame)
{
    int got;

    if (g == NULL || n == NULL || seqs == NULL) {
        return;
    }
    got = *n;
    if (got < 0) {
        got = 0;
    }
    {
        int rc, tries = 0, waited = 0;

        for (;;) {
            rc = load_one(g, &got, seqs, seq, frame, 1);
            if (rc == 0) {
                break;
            }
            /* Enter-path create_sprite uses load_frame, not ensure.
             * Unused Screen only — same 14.4c retry as play-path. */
            if (rc == -2 && tries++ < DINK_EDGFX_MAX &&
                evict_slot(g, &got, seqs, seq, frame, waited ? 0 : 1,
                           1) == 0) {
                waited = 1;
                continue;
            }
            break;
        }
    }
    (void)upload_with_room(g, &got, seqs, seq, frame);
    audio_music_pump();
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
        {
            int need = sprite_upload_needed(&g[i].fr);

            if (need == 0) {
                ok++;
                continue;
            }
            if (need < 0 || sprite_upload_pvr(&g[i].fr) != 0) {
                printf("edraw upload fail seq=%d\n", g[i].seq);
                continue;
            }
        }
        sprite_drop_cpu(&g[i].fr);
        ok++;
    }
    return ok > 0 || n == 0 ? 0 : -1;
}
#endif
