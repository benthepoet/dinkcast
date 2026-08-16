/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_RGB565_H
#define DINKCAST_RGB565_H

#include "bmp.h"
#include "boot.h"

#include <stddef.h>
#include <stdint.h>

int rgb565_from_bitmap(const struct Bitmap *bm, uint16_t **out, int *npx);

#endif
