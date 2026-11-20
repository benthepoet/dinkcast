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
    /* walk/idle 16 KB, punch 104/106 32 KB, sword hit 109 64 KB, test-only
     * seq 500 512 KB per frame */
    out->w = out->tw = seq == 500 ? 512 : 64;
    out->h = out->th = seq == 500   ? 512
                       : seq == 109 ? 512
                       : (seq == 104 || seq == 106) ? 256
                                                    : 128;
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

    /* Bar fight combat set: walk 72 + idle 12 + punch 106 (5 x 32 KB) +
     * the other walk facing 76 = 480 KB, inside the 512 KB cap: turning
     * and punching back and forth reloads nothing. */
    for (lap = 0; lap < 3; lap++) {
        for (fr = 1; fr <= 5; fr++) {
            CHECK(pspr_get(seqs, 106, fr) != NULL);
        }
        for (fr = 1; fr <= 8; fr++) {
            CHECK(pspr_get(seqs, 76, fr) != NULL);
        }
        for (fr = 1; fr <= 8; fr++) {
            CHECK(pspr_get(seqs, 72, fr) != NULL);
        }
    }
    CHECK(g_frees == 0);
    CHECK(g_loads == 12 + 5 + 8);
    CHECK(pspr_bytes() == 480u * 1024u);

    /* The whole bar-fight set (#157 log): 8 walk facings + 4 idles +
     * punch 104/106 = 1.57 MB, inside the 2 MB cap: laps over all of it
     * reload nothing. */
    {
        static const int walks[] = {71, 72, 73, 74, 76, 77, 78, 79};
        static const int idles[] = {12, 14, 16, 18};
        int k, loads0 = g_loads;

        for (lap = 0; lap < 3; lap++) {
            for (k = 0; k < 8; k++) {
                for (fr = 1; fr <= 8; fr++) {
                    CHECK(pspr_get(seqs, walks[k], fr) != NULL);
                }
            }
            for (k = 0; k < 4; k++) {
                for (fr = 1; fr <= 4; fr++) {
                    CHECK(pspr_get(seqs, idles[k], fr) != NULL);
                }
            }
            for (fr = 1; fr <= 5; fr++) {
                CHECK(pspr_get(seqs, 104, fr) != NULL);
                CHECK(pspr_get(seqs, 106, fr) != NULL);
            }
        }
        CHECK(g_frees == 0);
        /* New since the combat set: 6 walks, 3 idles, punch 104. */
        CHECK(g_loads == loads0 + 6 * 8 + 3 * 4 + 5);
        CHECK(pspr_bytes() == 1024u * 1024u + 256u * 1024u + 320u * 1024u);
    }

    /* Past the cap (two 512 KB frames): least recently drawn other-seq
     * frames go until the set is back under 2 MB. */
    CHECK(pspr_get(seqs, 500, 1) != NULL);
    CHECK(pspr_get(seqs, 500, 2) != NULL);
    CHECK(pspr_bytes() <= DINK_PSPR_TEX_CAP);
    /* 1600 KB set: the first 512 KB frame overshoots by 64 KB (4 oldest
     * 16 KB frames go), the second by 512 KB (32 more): 36 frees, oldest
     * first (walk 71, 72, 73, 74, then 76 fr 1..4), landing on the cap. */
    CHECK(g_frees == 36);
    CHECK(pspr_bytes() == DINK_PSPR_TEX_CAP);
    {
        int loads1 = g_loads;

        CHECK(pspr_get(seqs, 106, 5) != NULL); /* drawn last lap: kept */
        CHECK(pspr_get(seqs, 79, 8) != NULL);  /* newest walk: kept */
        CHECK(g_loads == loads1);
        CHECK(pspr_get(seqs, 71, 1) != NULL);  /* oldest: was evicted */
        CHECK(g_loads == loads1 + 1);
        CHECK(g_frees == 37); /* the reload pushed 16 KB over: one more */
        CHECK(pspr_bytes() == DINK_PSPR_TEX_CAP);
    }

    /* Current seq bigger than the cap (5 x 512 KB = 2.5 MB) is held whole;
     * every other frame is let go. */
    reset();
    for (fr = 1; fr <= 8; fr++) {
        (void)pspr_get(seqs, 72, fr);
    }
    for (fr = 1; fr <= 5; fr++) {
        CHECK(pspr_get(seqs, 500, fr) != NULL);
    }
    CHECK(pspr_count() == 5);
    CHECK(pspr_bytes() == 5u * 512u * 1024u);
    for (fr = 1; fr <= 5; fr++) {
        (void)pspr_get(seqs, 500, fr);
    }
    CHECK(g_loads == 8 + 5);

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
    CHECK(g_upload_fails == 0); /* short of VRAM: no malloc attempt at all */
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
    CHECK(pspr_trim_for(0u) == 0); /* nothing missing */
    g_vram_left = 0;
    CHECK(pspr_trim_for(10u * 16384u) == 0); /* 4 idle frames short of 10 */
    CHECK(pspr_count() == 5);
    /* Only the shortfall, oldest first: 2 of the 4 idle frames. */
    CHECK(pspr_trim_for(2u * 16384u) == 2u * 16384u);
    CHECK(pspr_count() == 3);
    CHECK(pspr_get(seqs, 12, 4) != NULL); /* newest idle frame kept */
    CHECK(pspr_count() == 3);
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

    /* Lookahead (pspr_prefetch): free slot within the cap only, never
     * moves the drawn frame, never evicts, no refusal memo. */
    reset();
    a = pspr_get(seqs, 72, 1);
    {
        int loads0 = g_loads;

        CHECK(pspr_prefetch(seqs, 72, 2) == 1);
        CHECK(pspr_prefetch(seqs, 72, 2) == 0);
        CHECK(g_loads == loads0 + 1 && pspr_count() == 2);
        CHECK(pspr_get(seqs, 72, 2) != NULL && g_loads == loads0 + 1);
    }
    /* The drawn frame stays the pinned one after a prefetch: VRAM short,
     * a Dink load gives back other frames but never 72 fr 2 (drawn). */
    b = pspr_get(seqs, 72, 2);
    CHECK(pspr_prefetch(seqs, 12, 1) == 1);
    g_vram_left = 0;
    CHECK(pspr_prefetch(seqs, 12, 2) == -1); /* no VRAM: skipped */
    CHECK(pspr_count() == 3);
    g_vram_left = -1;
    CHECK(pspr_get(seqs, 12, 2) != NULL); /* not refused: real get loads */
    /* Over the cap: skipped rather than evicting. */
    reset();
    for (fr = 1; fr <= 4; fr++) {
        (void)pspr_get(seqs, 500, fr); /* 4 x 512 KB = the 2 MB cap */
    }
    CHECK(pspr_bytes() == DINK_PSPR_TEX_CAP);
    {
        int frees0 = g_frees;

        int loads0 = g_loads;

        CHECK(pspr_prefetch(seqs, 72, 1) == -1);
        /* Skipped on the size estimate: nothing read, nothing freed. */
        CHECK(g_loads == loads0 && g_frees == frees0 && pspr_count() == 4);
    }
    (void)b;

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
