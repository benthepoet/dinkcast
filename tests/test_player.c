/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hard.h"
#include "pad.h"
#include "player.h"
#include "tiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct HardMask mask;
    struct SeqInfo seqs[DINK_MAX_SEQ];
    struct Player p;
    int i;

    if (pad_dir_from_buttons(0) != 0 ||
        pad_dir_from_buttons(DINK_PAD_LEFT) != 4 ||
        pad_dir_from_buttons(DINK_PAD_UP | DINK_PAD_LEFT) != 7) {
        fprintf(stderr, "FAIL pad dir\n");
        return 1;
    }
    memset(&seqs, 0, sizeof(seqs));
    for (i = 1; i < DINK_MAX_SEQ; i++) {
        seqs[i].delay = 50;
        seqs[i].nframes = 2;
        seqs[i].hl = -4;
        seqs[i].ht = -4;
        seqs[i].hr = 4;
        seqs[i].hb = 4;
    }
    memset(&mask, 0, sizeof(mask));
    mask.pix = calloc((size_t)DINK_PLAY_W * DINK_PLAY_H, 1);
    if (mask.pix == NULL) {
        return 1;
    }
    /* Wall at play-local x=200 (screen 220). */
    for (i = 0; i < DINK_PLAY_H; i++) {
        mask.pix[i * DINK_PLAY_W + 200] = 1;
    }
    player_init(&p);
    p.x = 100;
    p.y = 100;
    player_step(&p, 6, &mask, seqs); /* right */
    if (p.x <= 100 || p.dir != 6 || p.seq != DINK_BASE_WALK + 6) {
        fprintf(stderr, "FAIL walk right %d seq %d\n", p.x, p.seq);
        hard_mask_free(&mask);
        return 1;
    }
    /* Origin sample: wall at play 200 = screen 220. Stop at 219. */
    p.x = DINK_PLAY_LEFT + 200 - 2;
    p.y = 100;
    {
        int ox = p.x;
        player_step(&p, 6, &mask, seqs);
        if (p.x != ox + 1) {
            fprintf(stderr, "FAIL stop at wall x %d from %d\n", p.x, ox);
            hard_mask_free(&mask);
            return 1;
        }
    }
    {
        int ox = p.x;
        player_step(&p, 6, &mask, seqs);
        if (p.x != ox) {
            fprintf(stderr, "FAIL walk into wall x %d\n", p.x);
            hard_mask_free(&mask);
            return 1;
        }
        if (p.seq != DINK_BASE_WALK + 6) {
            fprintf(stderr, "FAIL still walk against wall\n");
            hard_mask_free(&mask);
            return 1;
        }
    }
    player_step(&p, 0, &mask, seqs);
    if (p.seq != DINK_BASE_IDLE + p.dir) {
        fprintf(stderr, "FAIL idle seq %d\n", p.seq);
        hard_mask_free(&mask);
        return 1;
    }
    hard_mask_free(&mask);
    printf("OK test_player\n");
    return 0;
}
