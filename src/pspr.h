/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PSPR_H
#define DINKCAST_PSPR_H

#include "ini.h"
#include "sprite.h"

#include <stddef.h>

/* Dink (sprite 1) frame set. FreeDink load_sequence_now keeps every Dink
 * sequence for the session; all of them are ~5 MB of POT1555, over the
 * plan §1.2 sprite_tex share. Plan 8.3 gives Dink's facing ≤ 256 KB.
 *
 * Policy: the requested seq is never evicted. Frames of other Dink seqs
 * stay until the set is over the cap, then go least recently drawn first.
 * Walk (128 KB/facing) + idle (64 KB) of one facing fit, so walk/stop/walk
 * reads no disc. A seq bigger than the cap (sword hit 384 KB) is held whole
 * while current. Bound: max(cap, largest Dink seq) + one frame. */
#define DINK_PSPR_SLOTS 64
#define DINK_PSPR_TEX_CAP (256u * 1024u)

struct PsprOps {
    int (*load)(struct SeqInfo *seqs, int seq, int frame,
                struct SpriteFrame *out); /* 0 ok */
    int (*upload)(struct SpriteFrame *f); /* 0 ok; DC drops CPU after */
    void (*free_frame)(struct SpriteFrame *f);
};

/* Tests only. NULL restores sprite_load_seq_frame / PVR upload / free. */
void pspr_set_ops(const struct PsprOps *ops);

/* Frame for Dink seq/frame: a hit costs no disc, decode or upload.
 * A miss loads first and evicts only after that load succeeds, so a NULL
 * return leaves every earlier pointer valid. Eviction frees PVR textures:
 * call only after the last scene finished (play loop waits at its top;
 * draw_screen waits before the swap). */
struct SpriteFrame *pspr_get(struct SeqInfo *seqs, int seq, int frame);

/* Free every frame. GPU must be idle. */
void pspr_clear(void);
size_t pspr_bytes(void);
int pspr_count(void);
/* Misses since start (each one was a read + decode + upload). */
unsigned pspr_misses(void);

#endif
