/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Dink frame set (pspr): hits read nothing, cap evicts other seqs LRU,
 * the current seq is never evicted, a failed load evicts nothing. */
#include "pspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static int g_loads;
static int g_frees;
static int g_fail_load;
static int g_vram_left = -1; /* uploads allowed; -1 = unlimited */
static int g_upload_fails;

#define CHECK(c)                                                               \
    do {                                                                       \
        if (!(c)) {                                                            \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);       \
            g_fail = 1;                                                        \
        }                                                                      \
    } while (0)

/* walk-sized frames: 64x128 POT1555 = 16 KB; sword-hit seq 109: 64 KB. */
static int fake_load(struct SeqInfo *seqs, int seq, int frame,
                     struct SpriteFrame *out)
{
    (void)seqs;
    (void)frame;
    if (g_fail_load) {
        return -1;
    }
    g_loads++;
    out->w = out->tw = 64;
    out->h = out->th = seq == 109 ? 512 : 128;
    out->argb1555 = (uint16_t *)malloc(4);
    return 0;
}

static int fake_upload(struct SpriteFrame *f)
{
    if (g_vram_left == 0) {
        g_upload_fails++;
        return -1;
    }
    if (g_vram_left > 0) {
        g_vram_left--;
    }
    f->tex = (void *)0x1;
    free(f->argb1555);
    f->argb1555 = NULL;
    return 0;
}

static void fake_free(struct SpriteFrame *f)
{
    g_frees++;
    free(f->argb1555);
    memset(f, 0, sizeof(*f));
}

static unsigned g_gen;

static unsigned fake_gen(void)
{
    return g_gen;
}

static size_t fake_avail(void)
{
    return g_vram_left < 0 ? (size_t)-1 : (size_t)g_vram_left * 16384u;
}

static int g_relief_calls;
static size_t g_relief_need;
static int g_relief_give; /* uploads the fake relief frees */

static size_t fake_relief(size_t need)
{
    g_relief_calls++;
    g_relief_need = need;
    if (g_relief_give > 0) {
        g_vram_left += g_relief_give;
        g_gen++;
        return (size_t)g_relief_give * 16384u;
    }
    return 0;
}

static const struct PsprOps ops = {fake_load, fake_upload, fake_free,
                                   fake_gen, fake_avail};

static void reset(void)
{
    pspr_clear();
    g_loads = g_frees = g_fail_load = g_upload_fails = 0;
    g_vram_left = -1;
}

int main(void)
{
    static struct SeqInfo seqs[DINK_MAX_SEQ];
    struct SpriteFrame *a, *b;
    int fr, lap;

    pspr_set_ops(&ops);

    /* Walk cycle: 8 loads the first lap, none after (the old slot reread
     * every frame change). */
    reset();
    for (lap = 0; lap < 5; lap++) {
        for (fr = 1; fr <= 8; fr++) {
            CHECK(pspr_get(seqs, 72, fr) != NULL);
        }
    }
    CHECK(g_loads == 8);
    CHECK(pspr_bytes() == 8u * 16384u);

    /* Stop (idle 12, 4 frames) and walk again: walk + idle of one facing
     * fit the cap, so nothing reloads. */
    for (fr = 1; fr <= 4; fr++) {
        CHECK(pspr_get(seqs, 12, fr) != NULL);
    }
    for (fr = 1; fr <= 8; fr++) {
        CHECK(pspr_get(seqs, 72, fr) != NULL);
    }
    CHECK(g_loads == 12);
    CHECK(g_frees == 0);
    CHECK(pspr_bytes() == 12u * 16384u);

    /* Turn to walk 76: over the cap, oldest other-seq frames go first
     * (idle frames were drawn before walk 72's second lap). */
    for (fr = 1; fr <= 8; fr++) {
        CHECK(pspr_get(seqs, 76, fr) != NULL);
    }
    CHECK(pspr_bytes() <= DINK_PSPR_TEX_CAP);
    CHECK(g_frees == 4); /* 20 frames * 16 KB = 320 KB → 4 out */
    CHECK(pspr_get(seqs, 72, 8) != NULL); /* recent walk 72 still held */
    CHECK(g_loads == 20);

    /* Current seq bigger than the cap (sword hit 6 x 64 KB) is held
     * whole; every other frame is let go. */
    reset();
    for (fr = 1; fr <= 8; fr++) {
        (void)pspr_get(seqs, 72, fr);
    }
    for (fr = 1; fr <= 6; fr++) {
        CHECK(pspr_get(seqs, 109, fr) != NULL);
    }
    CHECK(pspr_count() == 6);
    CHECK(pspr_bytes() == 6u * 65536u);
    for (fr = 1; fr <= 6; fr++) {
        (void)pspr_get(seqs, 109, fr);
    }
    CHECK(g_loads == 8 + 6);

    /* A failed load evicts nothing: the caller's last pointer stays. */
    reset();
    a = pspr_get(seqs, 72, 1);
    CHECK(a != NULL);
    for (fr = 1; fr <= 8; fr++) {
        (void)pspr_get(seqs, 76, fr);
    }
    g_fail_load = 1;
    for (fr = 1; fr <= 8; fr++) {
        CHECK(pspr_get(seqs, 78, fr) == NULL);
    }
    CHECK(g_frees == 0);
    CHECK(pspr_get(seqs, 72, 1) == a);
    g_fail_load = 0;

    /* Out of VRAM: give back other Dink frames (never the one being
     * drawn), then refuse with NULL so the caller keeps its last frame
     * and no CPU pixels are held. */
    reset();
    for (fr = 1; fr <= 4; fr++) {
        b = pspr_get(seqs, 12, fr);
    }
    g_vram_left = 0;
    CHECK(pspr_get(seqs, 72, 1) == NULL);
    CHECK(g_upload_fails == 4); /* first try + idle 1..3 (4 is drawn) */
    CHECK(pspr_count() == 1);
    CHECK(pspr_get(seqs, 12, 4) == b && b->tex != NULL);
    /* Refused: the next step does not reload until VRAM is freed. */
    {
        int loads = g_loads;

        CHECK(pspr_get(seqs, 72, 1) == NULL);
        CHECK(g_loads == loads);
        g_gen++; /* a texture freed somewhere, but VRAM still short */
        CHECK(pspr_get(seqs, 72, 1) == NULL);
        CHECK(g_loads == loads); /* no read: retry needs room too */
    }
    /* VRAM back: the next get loads and uploads. */
    g_vram_left = -1;
    g_gen++;
    CHECK(pspr_get(seqs, 72, 1) != NULL);

    /* No Dink frame to give back: ask the screen (relief) once with the
     * need before refusing. */
    reset();
    pspr_set_vram_relief(fake_relief);
    g_vram_left = 0;
    g_relief_calls = 0;
    g_relief_give = 1;
    CHECK(pspr_get(seqs, 72, 1) != NULL);
    CHECK(g_relief_calls == 1 && g_relief_need == 16384u);
    g_relief_give = 0;
    CHECK(pspr_get(seqs, 72, 2) == NULL); /* relief had nothing */
    CHECK(g_relief_calls == 2);
    pspr_set_vram_relief(NULL);
    g_vram_left = -1;

    /* edraw relief: trim only when VRAM is short and the trim covers it. */
    reset();
    for (fr = 1; fr <= 4; fr++) {
        (void)pspr_get(seqs, 12, fr);
    }
    (void)pspr_get(seqs, 72, 1);
    CHECK(pspr_trim_for(16384u) == 0); /* VRAM free: nothing to do */
    g_vram_left = 0;
    CHECK(pspr_trim_for(10u * 16384u) == 0); /* 4 idle frames short of 10 */
    CHECK(pspr_count() == 5);
    CHECK(pspr_trim_for(2u * 16384u) == 4u * 16384u);
    CHECK(pspr_count() == 1);
    g_vram_left = -1;

    /* DinkC init redefines walk 72 (sword arm): cached frames are not
     * hits, the drawn one survives until a new one loads, then the old
     * pixels are freed. */
    reset();
    for (fr = 1; fr <= 8; fr++) {
        a = pspr_get(seqs, 72, fr);
    }
    {
        unsigned gen = pspr_gen();

        pspr_forget_seq(72);
        CHECK(pspr_gen() != gen);
    }
    pspr_forget_seq(500); /* not cached: no gen bump needed */
    CHECK(pspr_count() == 8);
    g_fail_load = 1;
    CHECK(pspr_get(seqs, 72, 8) == NULL); /* reload fails: keep drawing a */
    CHECK(a->tex != NULL);
    g_fail_load = 0;
    b = pspr_get(seqs, 72, 8);
    CHECK(b != NULL && b != a);
    CHECK(pspr_count() == 1); /* 8 stale frames freed */
    CHECK(g_loads == 9);
    for (fr = 1; fr <= 8; fr++) {
        (void)pspr_get(seqs, 72, fr);
    }
    CHECK(g_loads == 16);

    /* Screen swap trim: only the current seq stays. */
    reset();
    for (fr = 1; fr <= 4; fr++) {
        (void)pspr_get(seqs, 12, fr);
    }
    for (fr = 1; fr <= 8; fr++) {
        (void)pspr_get(seqs, 72, fr);
    }
    CHECK(pspr_trim() == 4u * 16384u);
    CHECK(pspr_count() == 8);
    CHECK(pspr_trim() == 0);

    /* The VRAM loop never takes the frame being drawn, even from another
     * seq: Dink draws walk 72 fr 1, loading walk 76 runs out of VRAM, the
     * idle frames go back, 72 fr 1 stays. */
    reset();
    for (fr = 1; fr <= 4; fr++) {
        (void)pspr_get(seqs, 12, fr);
    }
    a = pspr_get(seqs, 72, 1);
    g_vram_left = 0; /* upload fails: VRAM loop must also skip a */
    CHECK(pspr_get(seqs, 76, 1) == NULL);
    CHECK(pspr_get(seqs, 72, 1) == a && a->tex != NULL);
    CHECK(pspr_count() == 1);
    g_vram_left = -1;

    /* Slot table full of one seq: refuse, keep everything. */
    reset();
    for (fr = 1; fr <= DINK_PSPR_SLOTS; fr++) {
        (void)pspr_get(seqs, 12, fr);
    }
    CHECK(pspr_count() == DINK_PSPR_SLOTS);
    CHECK(pspr_get(seqs, 12, DINK_PSPR_SLOTS + 1) == NULL);
    CHECK(pspr_count() == DINK_PSPR_SLOTS);

    /* Bad args. */
    CHECK(pspr_get(NULL, 72, 1) == NULL);
    CHECK(pspr_get(seqs, 0, 1) == NULL);
    CHECK(pspr_get(seqs, 72, 0) == NULL);
    CHECK(pspr_get(seqs, DINK_MAX_SEQ, 1) == NULL);

    reset();
    CHECK(pspr_count() == 0 && pspr_bytes() == 0);
    pspr_set_ops(NULL);
    if (g_fail) {
        return 1;
    }
    printf("test_pspr ok\n");
    return 0;
}
