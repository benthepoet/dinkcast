/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "bmp.h"

int main(int argc, char **argv)
{
    struct Bitmap bm;
    unsigned first = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: bmp_info path.bmp\n");
        return 2;
    }
    if (bitmap_load_file(argv[1], &bm) != 0) {
        fprintf(stderr, "FAIL load %s\n", argv[1]);
        return 1;
    }
    if (bm.bpp == 8) {
        first = bm.pixels[0];
    } else {
        first = ((unsigned)bm.pixels[0] << 16) | ((unsigned)bm.pixels[1] << 8) |
                (unsigned)bm.pixels[2];
    }
    printf("%d %d %d %u\n", bm.w, bm.h, bm.bpp, first);
    bitmap_free(&bm);
    return 0;
}
