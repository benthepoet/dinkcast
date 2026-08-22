/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_EDRAW_H
#define DINKCAST_EDRAW_H

#include "ini.h"
#include "mapscr.h"
#include "sprite.h"

#include <stddef.h>

#define DINK_EDGFX_MAX 128

struct EdGfx {
    int seq;
    int frame;
    int live; /* Screen: needed this tick. Always/Sticky never play-evicted. */
    struct SpriteFrame fr;
};

void edraw_free(struct EdGfx *g, int n);
/* Size matches edraw.c (do not BSS an array next to g_scr). */
struct EdGfx *edraw_gfx_alloc(void);
void edraw_gfx_release(struct EdGfx *g);
/* Screen MAIN create_sprite seqs: keep as Screen after swap_begin. */
void edraw_mark_need(int seq, int frame);
/* Play-path: Screen live = this tick's draw set (not "loaded this screen"). */
void edraw_live_begin(struct EdGfx *g, int n, struct SeqInfo *seqs);
void edraw_live_touch(struct EdGfx *g, int n, int seq, int frame);
/* Unique (seq,frame) for active editor sprites. */
int edraw_load_screen(struct EditorSprite *spr, struct SeqInfo *seqs,
                      struct EdGfx *g, int *n, int vision);
#ifdef _arch_dreamcast
/* After pvr_init (tiles_upload_pvr). Load is CPU-only. */
int edraw_upload_pvr(struct EdGfx *g, int n);
#endif
struct SpriteFrame *edraw_find(struct EdGfx *g, int n, int seq, int frame);
/* Decode (and on DC upload) a frame if missing. *n may grow.
 * Refuses to fopen: the seq's dir.ff must already be in the ff cache. */
int edraw_ensure_frame(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq,
                       int frame);
/* Enter path: may fopen. Load all frames of seq into g (girl create, etc.). */
void edraw_load_seq(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq);
/* Enter path: one frame (people walk dirs: frame 1 only). */
void edraw_load_frame(struct EdGfx *g, int *n, struct SeqInfo *seqs, int seq,
                      int frame);
size_t edraw_cpu_bytes(const struct EdGfx *g, int n);

#endif
