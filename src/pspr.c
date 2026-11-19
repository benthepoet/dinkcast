/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pspr.h"

#include <stdio.h>
#include <string.h>

struct PsprSlot {
    int seq, frame; /* seq 0 = empty */
    unsigned used;  /* last pspr_get tick */
    size_t bytes;
    struct SpriteFrame fr;
};

static struct PsprSlot g_slot[DINK_PSPR_SLOTS];
static size_t g_bytes;
static unsigned g_tick;
static unsigned g_misses;

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
};
static const struct PsprOps *g_ops = &g_default_ops;

void pspr_set_ops(const struct PsprOps *ops)
{
    g_ops = ops != NULL ? ops : &g_default_ops;
}

static size_t frame_bytes(const struct SpriteFrame *f)
{
    return (size_t)f->tw * (size_t)f->th * 2u;
}

static void slot_free(struct PsprSlot *s)
{
    g_ops->free_frame(&s->fr);
    g_bytes -= s->bytes;
    memset(s, 0, sizeof(*s));
}

/* Least recently drawn frame not in keep_seq, or -1. */
static int lru_victim(int keep_seq)
{
    int i, pick = -1;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].seq == 0 || g_slot[i].seq == keep_seq) {
            continue;
        }
        if (pick < 0 || g_slot[i].used < g_slot[pick].used) {
            pick = i;
        }
    }
    return pick;
}

static void evict(int i, int for_seq, const char *why)
{
    printf("pspr evict seq=%d fr=%d for seq=%d (%s)\n", g_slot[i].seq,
           g_slot[i].frame, for_seq, why);
    slot_free(&g_slot[i]);
}

struct SpriteFrame *pspr_get(struct SeqInfo *seqs, int seq, int frame)
{
    struct SpriteFrame fr;
    struct PsprSlot *s;
    int i, empty = -1;

    if (seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ || frame < 1) {
        return NULL;
    }
    g_tick++;
    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].seq == seq && g_slot[i].frame == frame) {
            s = &g_slot[i];
            s->used = g_tick;
            if (s->fr.tex == NULL && s->fr.argb1555 != NULL) {
                /* An earlier upload ran out of VRAM; try again. */
                (void)g_ops->upload(&s->fr);
            }
            return &s->fr;
        }
        if (g_slot[i].seq == 0 && empty < 0) {
            empty = i;
        }
    }
    /* Load before evicting anything: a failed read keeps the caller's
     * previous frame valid. */
    memset(&fr, 0, sizeof(fr));
    if (g_ops->load(&seqs[seq], seq, frame, &fr) != 0) {
        return NULL;
    }
    g_misses++;
    if (empty < 0) {
        int v = lru_victim(seq);

        if (v < 0) {
            /* Every slot holds this seq: more frames than slots. */
            printf("pspr full seq=%d fr=%d\n", seq, frame);
            g_ops->free_frame(&fr);
            return NULL;
        }
        evict(v, seq, "slots");
        empty = v;
    }
    s = &g_slot[empty];
    s->seq = seq;
    s->frame = frame;
    s->used = g_tick;
    s->bytes = frame_bytes(&fr);
    s->fr = fr;
    g_bytes += s->bytes;
    /* Out of VRAM: give back other Dink frames, oldest first. */
    while (g_ops->upload(&s->fr) != 0) {
        int v = lru_victim(seq);

        if (v < 0) {
            printf("pspr upload fail seq=%d fr=%d bytes=%u\n", seq, frame,
                   (unsigned)s->bytes);
            break;
        }
        evict(v, seq, "vram");
    }
    while (g_bytes > DINK_PSPR_TEX_CAP) {
        int v = lru_victim(seq);

        if (v < 0) {
            break;
        }
        evict(v, seq, "cap");
    }
    return &s->fr;
}

void pspr_clear(void)
{
    int i;

    for (i = 0; i < DINK_PSPR_SLOTS; i++) {
        if (g_slot[i].seq != 0) {
            slot_free(&g_slot[i]);
        }
    }
    g_bytes = 0;
    g_tick = 0;
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
