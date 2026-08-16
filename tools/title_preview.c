/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Load official title BMP via DINK_DATA and write a PPM (not committed). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fs.h"
#include "title.h"
#include "title_path.h"

int main(int argc, char **argv)
{
    const char *out = (argc > 1) ? argv[1] : "build/title_preview.ppm";
    struct TitleStill t;
    FILE *fp;
    int i;

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL dink_fs_init (set DINK_DATA)\n");
        return 2;
    }
    if (title_load(&t) != 0) {
        fprintf(stderr, "FAIL title_load %s under %s\n", DINK_TITLE_REL,
                dink_fs_root());
        return 1;
    }
    if (t.w != 640 || t.h != 480) {
        fprintf(stderr, "FAIL unexpected size %dx%d\n", t.w, t.h);
        title_free(&t);
        return 1;
    }
    fp = fopen(out, "wb");
    if (fp == NULL) {
        perror(out);
        title_free(&t);
        return 1;
    }
    fprintf(fp, "P6\n%d %d\n255\n", t.w, t.h);
    for (i = 0; i < t.npx; i++) {
        uint16_t p = t.rgb565[i];
        unsigned char rgb[3];
        rgb[0] = (unsigned char)(((p >> 11) & 0x1f) << 3);
        rgb[1] = (unsigned char)(((p >> 5) & 0x3f) << 2);
        rgb[2] = (unsigned char)((p & 0x1f) << 3);
        fwrite(rgb, 1, 3, fp);
    }
    fclose(fp);
    printf("OK title %dx%d -> %s root=%s path=%s\n", t.w, t.h, out,
           dink_fs_root(), DINK_TITLE_REL);
    title_free(&t);
    return 0;
}
