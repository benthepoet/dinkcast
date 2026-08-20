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
    player_init(&p);
    p.x = 100;
    p.y = 100;
    player_step(&p, 9, &mask, seqs); /* up-right: speed-speed/3 = 2 */
    if (p.x != 102 || p.y != 98) {
        fprintf(stderr, "FAIL diag 9 %d,%d\n", p.x, p.y);
        hard_mask_free(&mask);
        return 1;
    }
    player_step(&p, 0, &mask, seqs);
    if (p.dir != 8 || p.seq != DINK_BASE_IDLE + 8) {
        fprintf(stderr, "FAIL idle snap 9→8 dir=%d seq=%d\n", p.dir, p.seq);
        hard_mask_free(&mask);
        return 1;
    }
    {
        const char *ini =
            "set_frame_frame 14 5 14 3\n"
            "set_frame_frame 14 6 14 2\n";
        if (ini_parse_mem(ini, strlen(ini), seqs, DINK_MAX_SEQ) != 0) {
            fprintf(stderr, "FAIL idle alias parse\n");
            hard_mask_free(&mask);
            return 1;
        }
        for (i = 1; i < DINK_MAX_SEQ; i++) {
            seqs[i].delay = 50;
            seqs[i].nframes = 4;
        }
        player_init(&p);
        p.x = 100;
        p.y = 100;
        p.frame = 4;
        p.acc = 34;
        player_step(&p, 0, &mask, seqs);
        if (p.frame != 5 || p.seq != DINK_BASE_IDLE + 4) {
            fprintf(stderr, "FAIL idle ping 4->5 got frame %d seq %d\n",
                    p.frame, p.seq);
            hard_mask_free(&mask);
            return 1;
        }
        p.frame = 6;
        p.acc = 34;
        player_step(&p, 0, &mask, seqs);
        if (p.frame != 1) {
            fprintf(stderr, "FAIL idle ping 6->1 got %d\n", p.frame);
            hard_mask_free(&mask);
            return 1;
        }
    }
    {
        int ox = p.x, oy = p.y;

        p.freeze = 1;
        player_step(&p, 6, &mask, seqs);
        if (p.x != ox || p.y != oy) {
            fprintf(stderr, "FAIL freeze moved %d,%d\n", p.x, p.y);
            hard_mask_free(&mask);
            return 1;
        }
    }
    hard_mask_free(&mask);
    printf("OK test_player\n");
    return 0;
}
