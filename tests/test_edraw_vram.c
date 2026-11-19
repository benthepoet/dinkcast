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

static size_t fake_relief(void)
{
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

    edraw_set_upload(NULL);
    edraw_free(g, n);
    free(seqs);
    if (g_fail) {
        return 1;
    }
    printf("OK test_edraw_vram\n");
    return 0;
}
