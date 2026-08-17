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
/* Draw splash until Start/A on Maple port 0; then pvr_mem_free the tex. */
int title_present_pvr(const struct TitleStill *t);
#endif

#endif
