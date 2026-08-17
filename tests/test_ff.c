/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ff.h"
#include "fs.h"
#include "le.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    uint8_t buf[64];
    struct FfFile ff;
    const uint8_t *p;
    size_t ln;
    uint32_t nent = 2, off0 = 4 + 17 * 2;

    memset(buf, 0, sizeof(buf));
    buf[0] = 2;
    buf[4] = (uint8_t)off0;
    memcpy(buf + 8, "a.bmp", 5);
    buf[4 + 17] = (uint8_t)(off0 + 4);
    memcpy(buf + off0, "ABCD", 4);
    if (ff_parse_mem(buf, sizeof(buf), &ff) != 0) {
        fprintf(stderr, "FAIL parse\n");
        return 1;
    }
    if (ff_find(&ff, "a.bmp", &p, &ln) != 0 || ln != 4 || memcmp(p, "ABCD", 4) != 0) {
        fprintf(stderr, "FAIL find\n");
        ff_free(&ff);
        return 1;
    }
    ff_free(&ff);

    if (dink_fs_init() == 0) {
        memset(&ff, 0, sizeof(ff));
        if (ff_load_rel("graphics/dink/idle/dir.ff", &ff) != 0) {
            fprintf(stderr, "FAIL official idle dir.ff\n");
            return 1;
        }
        if (ff_find(&ff, "ds-i4-01.bmp", &p, &ln) != 0 || ln < 54) {
            fprintf(stderr, "FAIL ds-i4-01.bmp in dir.ff\n");
            ff_free(&ff);
            return 1;
        }
        printf("ff ds-i4-01.bmp %zu bytes entries %d\n", ln, ff.nent);
        ff_free(&ff);
    }
    (void)nent;
    printf("OK test_ff\n");
    return 0;
}
