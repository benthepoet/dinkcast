/* SPDX-License-Identifier: GPL-3.0-or-later */
/* PAL8 sprite decode: with the main palette set, a colour-keyed frame keeps
 * its 8-bit indices and is pixel-for-pixel the same picture as the
 * ARGB1555 decode (index 0 <-> transparent, others <-> the 1555 colour),
 * at half the texture bytes. No-colour-key frames and frames off the main
 * palette stay ARGB1555. */
#include "fs.h"
#include "ini.h"
#include "sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DINK_IDLE_SEQ 14

static int g_fail;
#define CHECK(c)                                                               \
    do {                                                                       \
        if (!(c)) {                                                            \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);       \
            g_fail = 1;                                                        \
        }                                                                      \
    } while (0)

static uint16_t to1555(const uint8_t *rgb)
{
    if (rgb[0] > 240 && rgb[1] > 240 && rgb[2] > 240) {
        return 0;
    }
    return (uint16_t)(SPRITE_ARGB1555_OPAQUE | ((rgb[0] >> 3) << 10) |
                      ((rgb[1] >> 3) << 5) | (rgb[2] >> 3));
}

int main(void)
{
    static uint8_t pal[768], other[768];
    struct SeqInfo *seqs;
    struct SpriteFrame p8, c16, nk;
    int i, n = 0, mism = 0, clear = 0;

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    CHECK(sprite_load_main_palette(pal) == 0);
    /* Survey: white is index 0, black 255. */
    CHECK(pal[0] == 255 && pal[1] == 255 && pal[2] == 255);
    CHECK(pal[765] == 0 && pal[766] == 0 && pal[767] == 0);
    seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
    if (seqs == NULL || ini_load(seqs, DINK_MAX_SEQ) != 0) {
        fprintf(stderr, "FAIL ini\n");
        return 1;
    }

    memset(&c16, 0, sizeof(c16));
    sprite_set_main_palette(NULL);
    CHECK(!sprite_pal8_on());
    CHECK(sprite_load_seq_frame(&seqs[DINK_IDLE_SEQ], DINK_IDLE_SEQ, 1,
                                &c16) == 0);
    CHECK(c16.bpp == 16 && c16.argb1555 != NULL && c16.pal8 == NULL);

    memset(&p8, 0, sizeof(p8));
    sprite_set_main_palette(pal);
    CHECK(sprite_pal8_on());
    CHECK(sprite_load_seq_frame(&seqs[DINK_IDLE_SEQ], DINK_IDLE_SEQ, 1,
                                &p8) == 0);
    CHECK(p8.bpp == 8 && p8.pal8 != NULL && p8.argb1555 == NULL);
    CHECK(p8.w == c16.w && p8.h == c16.h && p8.tw == c16.tw &&
          p8.th == c16.th && p8.cx == c16.cx && p8.cy == c16.cy);
    CHECK(sprite_has_cpu(&p8));
    CHECK(sprite_tex_bytes(&p8) * 2u == sprite_tex_bytes(&c16));
    CHECK(sprite_cpu_bytes(&p8) * 2u == sprite_cpu_bytes(&c16));
    if (p8.pal8 != NULL && c16.argb1555 != NULL) {
        n = p8.tw * p8.th;
        for (i = 0; i < n; i++) {
            uint16_t want = c16.argb1555[i];
            uint16_t got = to1555(&pal[(size_t)p8.pal8[i] * 3u]);

            if (want != got) {
                mism++;
            }
            if (p8.pal8[i] == 0) {
                clear++;
            }
        }
    }
    CHECK(mism == 0);
    CHECK(clear > 10);

    /* Dropping CPU keeps bpp (VRAM size accounting after upload). */
    sprite_drop_cpu(&p8);
    CHECK(!sprite_has_cpu(&p8) && p8.bpp == 8);
    CHECK(sprite_tex_bytes(&p8) * 2u == sprite_tex_bytes(&c16));

    /* No colour key (HUD/status compositing reads 1555 pixels): 16 bpp. */
    memset(&nk, 0, sizeof(nk));
    CHECK(sprite_load_seq_frame_nocolorkey(&seqs[DINK_IDLE_SEQ],
                                           DINK_IDLE_SEQ, 1, &nk) == 0);
    CHECK(nk.bpp == 16 && nk.argb1555 != NULL && nk.pal8 == NULL);
    sprite_frame_free(&nk);

    /* A BMP off the main palette falls back to ARGB1555. */
    memcpy(other, pal, sizeof(other));
    other[3] ^= 0xff;
    sprite_set_main_palette(other);
    memset(&nk, 0, sizeof(nk));
    CHECK(sprite_load_seq_frame(&seqs[DINK_IDLE_SEQ], DINK_IDLE_SEQ, 1,
                                &nk) == 0);
    CHECK(nk.bpp == 16 && nk.argb1555 != NULL && nk.pal8 == NULL);
    sprite_frame_free(&nk);

    sprite_set_main_palette(NULL);
    sprite_frame_free(&p8);
    sprite_frame_free(&c16);
    free(seqs);
    if (g_fail) {
        return 1;
    }
    printf("OK test_pal8 (%d of %d pixels transparent, 0 mismatches)\n",
           clear, n);
    return 0;
}
