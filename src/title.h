/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_TITLE_H
#define DINKCAST_TITLE_H

#include <stdint.h>

struct TitleStill {
    int w;
    int h;
    uint16_t *rgb565;
    int npx;
};

int title_load(struct TitleStill *out);
void title_free(struct TitleStill *t);

#ifdef _arch_dreamcast
/* One Splash.bmp frame. No pad wait. Tex freed; last frame stays until START. */
int title_present_pvr(const struct TitleStill *t);
#endif

#endif
