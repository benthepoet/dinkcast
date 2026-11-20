/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_TWIDDLE_H
#define DINKCAST_TWIDDLE_H

#include <stdint.h>

/* PVR twiddled layout, bit-for-bit the same as KOS pvr_txr_load_ex
 * (TWIDOUT(x, y) = TWIDTAB(y) | TWIDTAB(x) << 1; non-square textures are
 * min×min squares laid end to end; 8bpp packs rows y and y+1 into one
 * 16-bit word). Shifts and a 1024-entry table instead of KOS's per-pixel
 * divides. w, h: powers of two, 8..1024. dst may be RAM (then copy to VRAM
 * linearly, pvr_txr_load) or VRAM (16-bit writes, like KOS). */
void twiddle_8bpp(const uint8_t *src, uint16_t *dst, int w, int h);
void twiddle_16bpp(const uint16_t *src, uint16_t *dst, int w, int h);

#endif
