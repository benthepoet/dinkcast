/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_SPRITE_H
#define DINKCAST_SPRITE_H

#include "ini.h"

#include <stdint.h>

struct SpriteFrame {
    int w, h, tw, th;
    int cx, cy;
    int hl, ht, hr, hb;
    uint16_t *argb1555; /* POT-padded; white RGB is alpha 0 */
    void *tex;          /* DC pvr_ptr_t */
};

/* ARGB1555: bit 15 set = opaque. White RGB → 0. */
#define SPRITE_ARGB1555_OPAQUE 0x8000u
int sprite_pixel_opaque(uint16_t p);

void sprite_frame_free(struct SpriteFrame *f);
/* Frame index is 1-based (ds-i4-01.bmp). */
int sprite_load_seq_frame(const struct SeqInfo *seq, int seqn, int frame,
                          struct SpriteFrame *out);

/* get_box alt crop. 1 if trimmed. sl/st/sr/sb are exclusive-right src. */
int sprite_alt_src(int fw, int fh, int al, int at, int ar, int ab, int *sl,
                   int *st, int *sr, int *sb);

#ifdef _arch_dreamcast
int sprite_upload_pvr(struct SpriteFrame *f);
void sprite_evict_pvr(struct SpriteFrame *f);
void sprite_draw_pvr(const struct SpriteFrame *f, float x, float y, float z);
void sprite_draw_pvr_alt(const struct SpriteFrame *f, float x, float y,
                         float z, int al, int at, int ar, int ab);
#endif

#endif
