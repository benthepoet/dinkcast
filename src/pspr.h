/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PSPR_H
#define DINKCAST_PSPR_H

#include "ini.h"
#include "sprite.h"

#include <stddef.h>

/* Dink (sprite 1) frame set. FreeDink load_sequence_now keeps every Dink
 * sequence for the session; all of them are ~5 MB of POT1555, over the
 * plan §1.2 sprite_tex share. Plan 8.3: Dink's set is capped at 2 MB.
 *
 * Policy: the requested seq and the frame last handed out are never
 * evicted. Frames of other Dink seqs stay until the set passes the cap or
 * VRAM runs short, then go least recently drawn first (recency among
 * Dink's own frames only; 14.4c EdGfx victims stay class-based). Real
 * combat uses every facing (bar fight, #157 log: 8 walks, 4 idles, punch
 * 104/106, magic = ~1.6 MB), so the cap is the §1.2 share, not a fight
 * shape. Screen sprites and overlays take Dink frames back when short
 * (pspr_trim_for, (vram) evict). A seq bigger than the cap is held whole
 * while current. Bound: max(cap, current seq) + the last frame. */
#define DINK_PSPR_SLOTS 192
#define DINK_PSPR_TEX_CAP (2048u * 1024u)

struct PsprOps {
    int (*load)(struct SeqInfo *seqs, int seq, int frame,
                struct SpriteFrame *out); /* 0 ok */
    int (*upload)(struct SpriteFrame *f); /* 0 ok; DC drops CPU after */
    void (*free_frame)(struct SpriteFrame *f);
    unsigned (*vram_gen)(void); /* sprite_pvr_free_gen: bumps on any free */
    size_t (*vram_avail)(void); /* sprite_vram_avail */
};

/* Tests only. NULL restores sprite_load_seq_frame / PVR upload / free. */
void pspr_set_ops(const struct PsprOps *ops);

/* Frame for Dink seq/frame: a hit costs no disc, decode or upload.
 * A frame refused for VRAM is not reloaded until some texture is freed
 * (vram_gen changes), so a full VRAM does not re-read it every step.
 * NULL (load fail, no VRAM, slots full) leaves the last returned pointer
 * valid: keep drawing it. Eviction frees PVR textures, so call only after
 * the last scene finished (play loop waits at its top; draw_screen waits
 * before the swap; enter path is before the first scene). */
struct SpriteFrame *pspr_get(struct SeqInfo *seqs, int seq, int frame);

/* DinkC init redefined seq (sword/bow/fists arm reload walk/idle/hit).
 * Its frames are no longer hits; they are freed after the next successful
 * pspr_get (never while the caller may still draw one). Bumps pspr_gen. */
void pspr_forget_seq(int seq);
/* Changes on forget: the caller re-gets even if seq/frame did not change. */
unsigned pspr_gen(void);

/* Screen swap: free every frame except the current seq's (and the drawn
 * one). GPU must be idle. Returns bytes freed. */
size_t pspr_trim(void);
/* edraw/overlay relief: extra = bytes they cannot free themselves. If
 * Dink's other frames cover it, free least recently drawn ones until
 * extra is freed (not the whole set). */
size_t pspr_trim_for(size_t need);
/* Before refusing a Dink frame for VRAM: fn(need) frees other users'
 * VRAM (edraw_vram_room: non-live Screen textures), once. */
void pspr_set_vram_relief(size_t (*fn)(size_t need));
/* Fragmented VRAM: free one least recently drawn non-current frame;
 * bytes freed or 0. GPU must be idle. */
size_t pspr_free_one(void);

/* Free every frame. GPU must be idle. */
void pspr_clear(void);
size_t pspr_bytes(void);
int pspr_count(void);
/* Misses since start (each one was a read + decode + upload). */
unsigned pspr_misses(void);

#endif
