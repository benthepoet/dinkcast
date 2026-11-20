/* SPDX-License-Identifier: GPL-3.0-or-later */
/* sprite_tex (VRAM) policy on the play path, with a fake PVR budget:
 * out of VRAM frees non-live Screen textures (cached pack) first, never a
 * live frame or Always/Sticky; then the relief hook; then refuses and does
 * not reload until a texture is freed. Bar fight 2026-11-19: knights +
 * maiden filled VRAM and every new frame re-read + decoded each tick. */
#include "edraw.h"
#include "ff.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "residency.h"
#include "start_map.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct EdGfx g[DINK_EDGFX_MAX];
static int n;
static size_t g_budget = (size_t)-1;
static int g_uploads;
static size_t g_relief_give;
static int g_relief_calls;
static int g_fail;

#define CHECK(c)                                                               \
    do {                                                                       \
        if (!(c)) {                                                            \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);       \
            g_fail = 1;                                                        \
        }                                                                      \
    } while (0)

static size_t tex_bytes(const struct SpriteFrame *f)
{
    return (size_t)f->tw * (size_t)f->th * 2u;
}

static size_t vram_used(void)
{
    size_t u = 0;
    int i;

    for (i = 0; i < n + 1 && i < DINK_EDGFX_MAX; i++) {
        if (g[i].fr.tex != NULL) {
            u += tex_bytes(&g[i].fr);
        }
    }
    return u;
}

static int fake_upload(struct SpriteFrame *f)
{
    g_uploads++;
    if (vram_used() + tex_bytes(f) > g_budget) {
        return -1;
    }
    f->tex = (void *)0x1;
    return 0;
}

static size_t fake_avail(void)
{
    size_t u = vram_used();

    return u >= g_budget ? 0u : g_budget - u;
}

static size_t fake_relief(size_t need)
{
    (void)need;
    g_relief_calls++;
    g_budget += g_relief_give;
    return g_relief_give;
}

static int pack_cached(const struct SeqInfo *seqs, int seq)
{
    char dir[160];
    const char *sl = strrchr(seqs[seq].prefix, '/');

    if (sl == NULL) {
        return 0;
    }
    snprintf(dir, sizeof(dir), "%.*s/dir.ff", (int)(sl - seqs[seq].prefix),
             seqs[seq].prefix);
    return ff_is_cached(dir);
}

static int always_frames(const struct SeqInfo *seqs)
{
    char dir[160];
    int i, k = 0;

    for (i = 0; i < n; i++) {
        const char *sl = strrchr(seqs[g[i].seq].prefix, '/');

        if (sl == NULL) {
            continue;
        }
        snprintf(dir, sizeof(dir), "%.*s/dir.ff",
                 (int)(sl - seqs[g[i].seq].prefix), seqs[g[i].seq].prefix);
        if (residency_is_always(dir) && g[i].fr.tex != NULL) {
            k++;
        }
    }
    return k;
}

/* Every resident frame gets a texture (budget open). */
static void upload_all(struct SeqInfo *seqs)
{
    int i, s, f;

    for (i = 0; i < n; i++) {
        if (g[i].fr.tex == NULL) {
            s = g[i].seq;
            f = g[i].frame;
            (void)edraw_ensure_frame(g, &n, seqs, s, f);
            i = -1; /* slots may move */
        }
    }
}

static void touch_all(struct SeqInfo *seqs)
{
    int i;

    edraw_live_begin(g, n, seqs);
    for (i = 0; i < n; i++) {
        edraw_live_touch(g, n, g[i].seq, g[i].frame);
    }
}

int main(void)
{
    struct World w;
    struct MapScreen scr;
    struct SeqInfo *seqs;
    int rec, i, live_before, alw, nb, uploads, refused;

    if (dink_fs_init() != 0 || world_load(&w) != 0) {
        fprintf(stderr, "FAIL world (DINK_DATA)\n");
        return 1;
    }
    rec = (int)w.loc[DINK_START_PLAYER_MAP];
    seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
    if (map_load_record(rec, &scr) != 0 || seqs == NULL ||
        ini_load(seqs, DINK_MAX_SEQ) != 0 ||
        edraw_load_screen(scr.sprite, seqs, g, &n, 0) != 0) {
        fprintf(stderr, "FAIL house setup\n");
        free(seqs);
        return 1;
    }
    /* Mom walk (Screen, pack cached at screen load) frames 2+ are not on
     * the house screen; they are the play-path loads below. */
    CHECK(pack_cached(seqs, 351) && pack_cached(seqs, 353));
    CHECK(edraw_find(g, n, 351, 2) == NULL && edraw_find(g, n, 353, 2) == NULL);

    edraw_set_upload(fake_upload);
    sprite_set_vram_avail(fake_avail);
    upload_all(seqs);
    for (i = 0; i < n; i++) {
        CHECK(g[i].fr.tex != NULL && g[i].fr.argb1555 == NULL);
    }
    alw = always_frames(seqs);

    /* 1. Full VRAM, one Screen frame not drawn this tick: it is freed for
     *    the new frame; every live frame stays. */
    touch_all(seqs);
    {
        int victim = -1;

        /* Largest innwalls (Screen, cached) frame: room for a 16 KB mom
         * frame with one eviction. */
        for (i = 0; i < n; i++) {
            if (g[i].seq == 31 &&
                (victim < 0 || tex_bytes(&g[i].fr) > tex_bytes(&g[victim].fr))) {
                victim = i;
            }
        }
        CHECK(victim >= 0 && tex_bytes(&g[victim].fr) >= 16384u);
        CHECK(victim >= 0);
        edraw_live_begin(g, n, seqs);
        for (i = 0; i < n; i++) {
            if (i != victim) {
                edraw_live_touch(g, n, g[i].seq, g[i].frame);
            }
        }
        nb = n;
        live_before = nb - 1;
        g_budget = vram_used();
        CHECK(edraw_ensure_frame(g, &n, seqs, 351, 2) == 0);
        CHECK(edraw_find(g, n, 351, 2) != NULL &&
              edraw_find(g, n, 351, 2)->tex != NULL);
        CHECK(n == nb); /* one out, one in */
        for (i = 0; i < n; i++) {
            CHECK(g[i].live || g[i].fr.tex != NULL);
        }
        CHECK(always_frames(seqs) == alw);
        (void)live_before;
    }

    /* 2. Everything live, VRAM full, no relief: refuse, frame not kept,
     *    and the next tick does not reload it (no upload attempt). */
    touch_all(seqs);
    g_budget = vram_used();
    refused = (int)edraw_vram_refused();
    nb = n;
    CHECK(edraw_ensure_frame(g, &n, seqs, 353, 2) != 0);
    CHECK(edraw_find(g, n, 353, 2) == NULL);
    CHECK(n == nb);
    CHECK((int)edraw_vram_refused() == refused + 1);
    uploads = g_uploads;
    CHECK(edraw_ensure_frame(g, &n, seqs, 353, 2) != 0);
    CHECK(g_uploads == uploads); /* memo: no decode, no upload */
    CHECK((int)edraw_vram_refused() == refused + 1);
    CHECK(always_frames(seqs) == alw);

    /* 3. A texture is freed (gen bumps): one more try, and it fits. */
    for (i = 0; i < n; i++) {
        if (g[i].seq == 351 && g[i].frame == 2) {
            sprite_evict_pvr(&g[i].fr);
        }
    }
    CHECK(edraw_ensure_frame(g, &n, seqs, 353, 2) == 0);
    CHECK(edraw_find(g, n, 353, 2) != NULL &&
          edraw_find(g, n, 353, 2)->tex != NULL);

    /* 4. Everything live and full: relief (Dink's other seqs) is asked
     *    once before refusing. */
    touch_all(seqs);
    g_budget = vram_used();
    g_relief_give = 1u << 20;
    edraw_set_vram_relief(fake_relief);
    CHECK(edraw_ensure_frame(g, &n, seqs, 357, 2) == 0);
    CHECK(g_relief_calls == 1);
    g_relief_give = 0;
    touch_all(seqs);
    g_budget = vram_used();
    CHECK(edraw_ensure_frame(g, &n, seqs, 359, 2) != 0);
    CHECK(g_relief_calls == 2); /* asked once, gave nothing, refused */
    edraw_set_vram_relief(NULL);

    /* 5. Victims that cannot cover the need are not evicted (no emptying
     *    the screen for a frame that is refused anyway); edraw_vram_room
     *    frees least recently drawn first and only when it can cover. */
    {
        int a = -1, b = -1, before;

        touch_all(seqs);
        /* Two innwalls frames go unlive on different ticks: a older. */
        for (i = 0; i < n; i++) {
            if (g[i].seq == 31 && tex_bytes(&g[i].fr) >= 16384u) {
                if (a < 0) {
                    a = i;
                } else if (b < 0) {
                    b = i;
                }
            }
        }
        CHECK(a >= 0 && b >= 0);
        {
            int as = g[a].seq, af = g[a].frame, bs = g[b].seq, bf = g[b].frame;

            edraw_live_begin(g, n, seqs); /* tick T: b drawn, a not */
            for (i = 0; i < n; i++) {
                if (i != a) {
                    edraw_live_touch(g, n, g[i].seq, g[i].frame);
                }
            }
            edraw_live_begin(g, n, seqs); /* tick T+1: neither drawn */
            for (i = 0; i < n; i++) {
                if (!(g[i].seq == as && g[i].frame == af) &&
                    !(g[i].seq == bs && g[i].frame == bf)) {
                    edraw_live_touch(g, n, g[i].seq, g[i].frame);
                }
            }
            g_budget = vram_used();
            before = n;
            CHECK(edraw_vram_room(g, &n, seqs, (size_t)1 << 24) == 0);
            CHECK(n == before); /* cannot cover: nothing freed */
            CHECK(edraw_vram_room(g, &n, seqs, 1u) > 0);
            CHECK(n == before - 1);
            CHECK(edraw_find(g, n, as, af) == NULL); /* older went first */
            CHECK(edraw_find(g, n, bs, bf) != NULL);
        }
    }

    /* 6. Combined pools: VRAM is 8 KB over budget, the one non-live
     *    16 KB innwalls frame leaves 8 KB missing for a 16 KB mom frame,
     *    and only Dink (relief) can give the rest. Edraw evicts its
     *    victim, sees the shortfall, asks relief once, then fits. */
    {
        int v = -1, vs, vf;

        touch_all(seqs);
        for (i = 0; i < n; i++) {
            if (g[i].seq == 31 && tex_bytes(&g[i].fr) == 16384u) {
                v = i;
                break;
            }
        }
        if (v >= 0) {
            vs = g[v].seq;
            vf = g[v].frame;
            edraw_live_begin(g, n, seqs);
            for (i = 0; i < n; i++) {
                if (!(g[i].seq == vs && g[i].frame == vf)) {
                    edraw_live_touch(g, n, g[i].seq, g[i].frame);
                }
            }
            g_budget = vram_used() - 8192u;
            g_relief_give = 8192u + 16384u;
            g_relief_calls = 0;
            edraw_set_vram_relief(fake_relief);
            CHECK(edraw_ensure_frame(g, &n, seqs, 355, 2) == 0);
            CHECK(g_relief_calls == 1);
            edraw_set_vram_relief(NULL);
            g_relief_give = 0;
        }
    }

    /* 7. Lookahead (edraw_prefetch): spare room only. Mom walk 357
     * (10 frames; fr 2 loaded in case 4). */
    {
        int before, uploads0;

        edraw_set_vram_relief(NULL);
        touch_all(seqs);
        g_budget = vram_used() + (2u << 20);
        CHECK(edraw_find(g, n, 357, 3) == NULL);
        CHECK(edraw_prefetch(g, &n, seqs, 357, 3) == 1);
        CHECK(edraw_find(g, n, 357, 3) != NULL &&
              edraw_find(g, n, 357, 3)->tex != NULL);
        CHECK(edraw_prefetch(g, &n, seqs, 357, 3) == 0); /* resident */
        /* Short of the spare margin: skipped before any read, nothing
         * evicted even though every frame is non-live. */
        edraw_live_begin(g, n, seqs);
        g_budget = vram_used() + 4096u;
        before = n;
        uploads0 = g_uploads;
        CHECK(edraw_prefetch(g, &n, seqs, 357, 4) == -1);
        CHECK(n == before && g_uploads == uploads0);
        /* Upload fails although avail says yes (fragmentation): dropped,
         * no eviction, no refusal; remembered so the next tick does not
         * re-read it; the real draw-time ensure still loads it. */
        refused = (int)edraw_vram_refused();
        g_budget = vram_used();
        sprite_set_vram_avail(NULL); /* host default: plenty */
        before = n;
        uploads0 = g_uploads;
        CHECK(edraw_prefetch(g, &n, seqs, 357, 4) == -1);
        CHECK(g_uploads == uploads0 + 1);
        CHECK(n == before && edraw_find(g, n, 357, 4) == NULL);
        CHECK(edraw_vram_refused() == (unsigned)refused);
        CHECK(edraw_prefetch(g, &n, seqs, 357, 4) == -1);
        CHECK(g_uploads == uploads0 + 1); /* memo: no second try */
        sprite_set_vram_avail(fake_avail);
        g_budget = vram_used() + (1u << 20);
        CHECK(edraw_ensure_frame(g, &n, seqs, 357, 4) == 0);
    }

    sprite_set_vram_avail(NULL);
    edraw_set_upload(NULL);
    edraw_free(g, n);
    free(seqs);
    if (g_fail) {
        return 1;
    }
    printf("OK test_edraw_vram\n");
    return 0;
}
