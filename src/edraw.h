/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_EDRAW_H
#define DINKCAST_EDRAW_H

#include "ini.h"
#include "mapscr.h"
#include "sprite.h"

#define DINK_EDGFX_MAX 96

struct EdGfx {
    int seq;
    int frame;
    struct SpriteFrame fr;
};

void edraw_free(struct EdGfx *g, int n);
/* Unique (seq,frame) for active editor sprites. */
int edraw_load_screen(const struct EditorSprite *spr, struct SeqInfo *seqs,
                      struct EdGfx *g, int *n);
#ifdef _arch_dreamcast
/* After pvr_init (tiles_upload_pvr). Load is CPU-only. */
int edraw_upload_pvr(struct EdGfx *g, int n);
#endif
struct SpriteFrame *edraw_find(struct EdGfx *g, int n, int seq, int frame);

#endif
