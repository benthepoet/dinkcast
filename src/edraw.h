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
    unsigned used; /* last draw tick touched (VRAM victim order) */
    struct SpriteFrame fr;
};

void edraw_free(struct EdGfx *g, int n);
/* sprite_tex: an upload out of VRAM frees non-live Screen textures with a
 * cached pack (least recently drawn first) when they can cover the need.
 * When they fall short it calls fn(extra) once, extra = bytes still
 * missing after them (pspr_trim_for: Dink's other seqs), then re-checks.
 * Else it refuses the frame until a texture is freed and enough VRAM is
 * free. GPU idle on every upload path. */
void edraw_set_vram_relief(size_t (*fn)(size_t need));
/* For other VRAM users (Dink, pspr): free non-live cached Screen textures,
 * least recently drawn first, until need bytes are free. Frees nothing if
 * they cannot cover it. Returns bytes freed. GPU must be idle. */
size_t edraw_vram_room(struct EdGfx *g, int *n, struct SeqInfo *seqs,
                       size_t need);
/* Screen enter: VRAM bytes edraw_upload_pvr still has to place. */
size_t edraw_upload_need(const struct EdGfx *g, int n);
/* Bytes edraw_vram_room could free (non-live cached Screen textures). */
size_t edraw_vram_evictable(struct EdGfx *g, int n, struct SeqInfo *seqs);
/* Free the least recently drawn such texture; bytes freed or 0. GPU idle. */
size_t edraw_vram_free_one(struct EdGfx *g, int *n, struct SeqInfo *seqs);
/* Tests: fake PVR upload (0 ok). DC default sprite_upload_pvr; host NULL
 * (frames stay CPU, no VRAM policy). */
void edraw_set_upload(int (*fn)(struct SpriteFrame *f));
/* Frames refused for VRAM since boot (mem refuse pool=sprite_tex). */
unsigned edraw_vram_refused(void);
/* Size matches edraw.c (do not BSS an array next to g_scr). */
struct EdGfx *edraw_gfx_alloc(void);
void edraw_gfx_release(struct EdGfx *g);
/* Screen MAIN create_sprite seqs: keep as Screen after swap_begin. */
void edraw_mark_need(int seq, int frame);
/* Play-path: Screen live = this tick's draw set (not "loaded this screen"). */
void edraw_live_begin(struct EdGfx *g, int n, struct SeqInfo *seqs);
void edraw_live_touch(struct EdGfx *g, int n, int seq, int frame);
/* After play-path touch+ensure: drop unused Screen CPU. Keep tex until
 * enter unique or evict_slot. */
void edraw_reap_unused(struct EdGfx *g, int *n, struct SeqInfo *seqs);
/* Brain 6 wrap: fr+1, or 1 at end / terminator. */
int edraw_loop_next_frame(const struct SeqInfo *seqs, int seq, int fr);
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
