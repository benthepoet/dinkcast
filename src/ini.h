/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_INI_H
#define DINKCAST_INI_H

#include <stddef.h>

#define DINK_MAX_SEQ 1000
#define DINK_MAX_FRAMES 50

struct SeqInfo {
    char prefix[128];
    int delay;
    int cx, cy;
    int hl, ht, hr, hb;
    int nframes;
};

int ini_parse_mem(const char *text, size_t n, struct SeqInfo *seqs, int nseq);
int ini_load(struct SeqInfo *seqs, int nseq);
int ini_count_ff_frames(const char *prefix);

#endif
