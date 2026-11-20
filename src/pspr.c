/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pspr.h"

#include <stdio.h>
#include <string.h>

struct PsprSlot {
    int seq, frame; /* seq 0 = empty */
    int stale;      /* seq redefined by DinkC init; free after next load */
    unsigned used;  /* last pspr_get tick */
    size_t bytes;
    struct SpriteFrame fr;
};

static struct PsprSlot g_slot[DINK_PSPR_SLOTS];
static size_t g_bytes;
static unsigned g_tick;
static unsigned g_misses;
static unsigned g_gen;
static int g_last = -1; /* slot the caller is drawing: never evicted */

static int default_upload(struct SpriteFrame *f)
{
#ifdef _arch_dreamcast
    if (sprite_upload_pvr(f) != 0) {
        return -1;
    }
    if (f->tex != NULL) {
        sprite_drop_cpu(f);
    }
    return 0;
#else
    (void)f;
    return 0;
#endif
}

static const struct PsprOps g_default_ops = {
    sprite_load_seq_frame,
    default_upload,
    sprite_frame_free,
    sprite_pvr_free_gen,
    sprite_vram_avail,
};
static const struct PsprOps *g_ops = &g_default_ops;

#define DINK_PSPR_REFUSE 16
static int g_ref_seq[DINK_PSPR_REFUSE];
static int g_ref_fr[DINK_PSPR_REFUSE];
static unsigned g_ref_gen[DINK_PSPR_REFUSE];
static size_t g_ref_need[DINK_PSPR_REFUSE];
static int g_nref, g_ref_next;
static size_t (*g_relief)(size_t need);

static unsigned vram_gen(void)
{
    return g_ops->vram_gen != NULL ? g_ops->vram_gen() : 0u;
}

static size_t vram_avail(void)
{
    return g_ops->vram_avail != NULL ? g_ops->vram_avail() : (size_t)-1;
}

void pspr_set_vram_relief(size_t (*fn)(size_t need))
{
    g_relief = fn;
}

/* 1 = still refused: nothing freed since, or not enough VRAM free. */
static int refused(int seq, int frame)
{
    int i;

    for (i = 0; i < g_nref; i++) {
        if (g_ref_seq[i] == seq && g_ref_fr[i] == frame) {
            return g_ref_gen[i] == vram_gen() ||
                   vram_avail() < g_ref_need[i];
        }
    }
    return 0;
}

static void refuse_note(int seq, int frame, size_t need)
{
    int i;

    for (i = 0; i < g_nref; i++) {
        if (g_ref_seq[i] == seq && g_ref_fr[i] == frame) {
            g_ref_gen[i] = vram_gen();
            g_ref_need[i] = need;
            return;
        }
    }
    if (g_nref < DINK_PSPR_REFUSE) {
        i = g_nref++;
    } else {
        i = g_ref_next;
        g_ref_next = (g_ref_next + 1) % DINK_PSPR_REFUSE;
    }
    g_ref_seq[i] = seq;
    g_ref_fr[i] = frame;
    g_ref_gen[i] = vram_gen();
    g_ref_need[i] = need;
}


void pspr_set_ops(const struct PsprOps *ops)
{
    g_ops = ops != NULL ? ops : &g_default_ops;
}

static size_t frame_bytes(const struct SpriteFrame *f)
{
    return (size_t)f->tw * (size_t)f->th * 2u;
}

static void slot_free(int i)
{
    g_ops->free_frame(&g_slot[i].fr);
    g_bytes -= g_slot[i].bytes;
    memset(&g_slot[i], 0, sizeof(g_slot[i]));
    if (g_last == i) {
        g_last = -1;
    }
}

/* Stale first, then least recently drawn, never keep_seq (unless stale)
 * and never the frame the caller is drawing. -1 if none. */
static int victim(int keep_seq)
{
    int i, pick = -1;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        const struct PsprSlot *s = &g_slot[i];

        if (s->seq == 0 || i == g_last ||
            (s->seq == keep_seq && !s->stale)) {
            continue;
        }
        if (pick < 0 || (s->stale && !g_slot[pick].stale) ||
            (s->stale == g_slot[pick].stale && s->used < g_slot[pick].used)) {
            pick = i;
        }
    }
    return pick;
}

static void evict(int i, int for_seq, const char *why)
{
    printf("pspr evict seq=%d fr=%d for seq=%d (%s)\n", g_slot[i].seq,
           g_slot[i].frame, for_seq, why);
    slot_free(i);
}

struct SpriteFrame *pspr_get(struct SeqInfo *seqs, int seq, int frame)
{
    struct SpriteFrame fr;
    struct PsprSlot *s;
    int i, empty = -1, slot;

    if (seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ || frame < 1) {
        return NULL;
    }
    g_tick++;
    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        s = &g_slot[i];
        if (s->seq == seq && s->frame == frame && !s->stale) {
            s->used = g_tick;
            g_last = i;
            return &s->fr;
        }
        if (s->seq == 0 && empty < 0) {
            empty = i;
        }
    }
    if (refused(seq, frame)) {
        return NULL; /* nothing freed since the last try */
    }
    /* Load before evicting anything: a failed read keeps the caller's
     * frame (g_last) and every other slot. */
    memset(&fr, 0, sizeof(fr));
    if (g_ops->load(&seqs[seq], seq, frame, &fr) != 0) {
        return NULL;
    }
    g_misses++;
    if (empty < 0) {
        int v = victim(seq);

        if (v < 0) {
            printf("pspr full seq=%d fr=%d\n", seq, frame);
            g_ops->free_frame(&fr);
            return NULL;
        }
        evict(v, seq, "slots");
        empty = v;
    }
    slot = empty;
    s = &g_slot[slot];
    s->seq = seq;
    s->frame = frame;
    s->stale = 0;
    s->used = g_tick;
    s->bytes = frame_bytes(&fr);
    s->fr = fr;
    g_bytes += s->bytes;
    /* Out of VRAM: give back other Dink frames, oldest first; then ask
     * the screen (non-live edraw textures) once; then refuse. */
    {
        int relief = 0;

        /* Short of VRAM: make room before trying (a failed pvr_mem_malloc
         * logs on KOS). */
        while (vram_avail() < s->bytes || g_ops->upload(&s->fr) != 0) {
            int v = victim(seq);

            if (v >= 0 && v != slot) {
                evict(v, seq, "vram");
                continue;
            }
            if (!relief && g_relief != NULL && g_relief(s->bytes) > 0) {
                relief = 1;
                continue;
            }
            printf("mem refuse pool=sprite_tex dink seq=%d fr=%d bytes=%u\n",
                   seq, frame, (unsigned)s->bytes);
            {
                size_t need = s->bytes;

                slot_free(slot);
                refuse_note(seq, frame, need);
            }
            return NULL;
        }
    }
    g_last = slot;
    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].stale && i != g_last) {
            evict(i, seq, "stale");
        }
    }
    while (g_bytes > DINK_PSPR_TEX_CAP) {
        int v = victim(seq);

        if (v < 0) {
            break;
        }
        evict(v, seq, "cap");
    }
    return &s->fr;
}

void pspr_forget_seq(int seq)
{
    int i, n = 0;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].seq == seq && seq > 0 && !g_slot[i].stale) {
            g_slot[i].stale = 1;
            n++;
        }
    }
    if (n > 0) {
        g_gen++;
        printf("pspr stale seq=%d n=%d\n", seq, n);
    }
}

unsigned pspr_gen(void)
{
    return g_gen;
}

size_t pspr_trim(void)
{
    size_t before = g_bytes;
    int keep = g_last >= 0 ? g_slot[g_last].seq : 0;
    int i;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].seq != 0 && i != g_last &&
            (g_slot[i].seq != keep || g_slot[i].stale)) {
            slot_free(i);
        }
    }
    return before - g_bytes;
}

size_t pspr_trim_for(size_t need)
{
    size_t can = 0;
    int keep = g_last >= 0 ? g_slot[g_last].seq : 0;
    int i;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].seq != 0 && i != g_last &&
            (g_slot[i].seq != keep || g_slot[i].stale)) {
            can += g_slot[i].bytes;
        }
    }
    if (need == 0 || can < need) {
        return 0; /* nothing missing, or would not be enough */
    }
    /* Only what is missing, least recently drawn first: a 512 KB set
     * dropped whole would re-decode ~15 frames mid-fight. */
    {
        size_t freed = 0;

        while (freed < need) {
            int v = victim(keep);

            if (v < 0) {
                break;
            }
            freed += g_slot[v].bytes;
            evict(v, keep, "trim");
        }
        return freed;
    }
}

void pspr_clear(void)
{
    int i;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].seq != 0) {
            slot_free(i);
        }
    }
    g_bytes = 0;
    g_tick = 0;
    g_last = -1;
    g_nref = 0;
    g_ref_next = 0;
}

size_t pspr_bytes(void)
{
    return g_bytes;
}

int pspr_count(void)
{
    int i, n = 0;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        n += g_slot[i].seq != 0;
    }
    return n;
}

unsigned pspr_misses(void)
{
    return g_misses;
}
