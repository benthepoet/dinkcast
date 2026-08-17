/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"
#include "ini.h"
#include "sprite.h"
#include "start_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct SeqInfo *seqs;
    struct SpriteFrame fr;
    int i, n, clear, opaque;

    if (!sprite_pixel_opaque(SPRITE_ARGB1555_OPAQUE | 0x4210)) {
        fprintf(stderr, "FAIL opaque bit\n");
        return 1;
    }
    if (sprite_pixel_opaque(0)) {
        fprintf(stderr, "FAIL clear 0\n");
        return 1;
    }
    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
    if (seqs == NULL || ini_load(seqs, DINK_MAX_SEQ) != 0) {
        fprintf(stderr, "FAIL ini\n");
        free(seqs);
        return 1;
    }
    memset(&fr, 0, sizeof(fr));
    if (sprite_load_seq_frame(&seqs[DINK_IDLE_SEQ], DINK_IDLE_SEQ, 1, &fr) != 0) {
        fprintf(stderr, "FAIL load idle\n");
        free(seqs);
        return 1;
    }
    free(seqs);
    n = fr.tw * fr.th;
    clear = opaque = 0;
    for (i = 0; i < n; i++) {
        if (sprite_pixel_opaque(fr.argb1555[i])) {
            opaque++;
        } else {
            clear++;
        }
    }
    /* Official idle has a white key around the figure. */
    if (clear < 10 || opaque < 10) {
        fprintf(stderr, "FAIL idle punch %d clear %d opaque\n", clear, opaque);
        sprite_frame_free(&fr);
        return 1;
    }
    if (sprite_pixel_opaque(fr.argb1555[0])) {
        fprintf(stderr, "FAIL idle (0,0) should be key\n");
        sprite_frame_free(&fr);
        return 1;
    }
    printf("idle punch clear %d opaque %d %dx%d pad %dx%d\n", clear, opaque, fr.w,
           fr.h, fr.tw, fr.th);
    sprite_frame_free(&fr);
    printf("OK test_sprite\n");
    return 0;
}
