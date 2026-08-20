/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_INI_H
#define DINKCAST_INI_H

#include <stddef.h>

#define DINK_MAX_SEQ 1000
#define DINK_MAX_FRAMES 50
#define DINK_SSI_MAX 400
#define DINK_SPECIAL_MAX 80
#define DINK_ALIAS_MAX 128
#define DINK_FDELAY_MAX 80

struct SeqInfo {
    char prefix[128];
    int delay;
    int cx, cy;
    int hl, ht, hr, hb;
    int nframes;
};

/* Last SET_SPRITE_INFO for (seq,frame) wins — FreeDink program_idata. */
struct IniFrame {
    int seq, frame;
    int cx, cy, hl, ht, hr, hb;
};

extern int ini_nframe;
extern struct IniFrame ini_frame[DINK_SSI_MAX];
/* SET_FRAME_SPECIAL last-wins (seq,frame) → 1. */
int ini_frame_special(int seq, int frame);
/* SET_FRAME_FRAME last-wins. Terminator dest_seq or dest_frame < 0.
 * Returns 1 if this slot is a terminator (do not draw; wrap / end seq). */
int ini_resolve_frame(int seq, int frame, int *oseq, int *ofr);
int ini_frame_delay(int seq, int frame, int seq_default);
/* bmp_nframes from dir.ff; aliases extend; terminator N means len N-1. */
int ini_seq_len(int seq, int bmp_nframes);

int ini_parse_mem(const char *text, size_t n, struct SeqInfo *seqs, int nseq);
int ini_load(struct SeqInfo *seqs, int nseq);
int ini_count_ff_frames(const char *prefix);
/* Fill offsets/hardbox: SET_SPRITE_INFO, else seq line if hr>0, else size guess. */
void ini_frame_geom(const struct SeqInfo *seq, int seqn, int frame, int fw,
                    int fh, int *cx, int *cy, int *hl, int *ht, int *hr,
                    int *hb);

#endif
