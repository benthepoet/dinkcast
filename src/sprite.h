/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_SPRITE_H
#define DINKCAST_SPRITE_H

#include "ini.h"

#include <stdint.h>

struct SpriteFrame {
    int w, h, tw, th;
    int cx, cy;
    uint16_t *argb1555; /* POT-padded; white RGB is alpha 0 */
};

void sprite_frame_free(struct SpriteFrame *f);
/* Frame index is 1-based (ds-i4-01.bmp). */
int sprite_load_seq_frame(const struct SeqInfo *seq, int frame,
                          struct SpriteFrame *out);

#ifdef _arch_dreamcast
int sprite_upload_pvr(struct SpriteFrame *f);
void sprite_evict_pvr(struct SpriteFrame *f);
void sprite_draw_pvr(const struct SpriteFrame *f, float x, float y, float z);
#endif

#endif
