/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hard.h"

#include "fs.h"
#include "le.h"

#include <stdlib.h>
#include <string.h>

int hard_parse_defaults(const uint8_t *p, size_t n, struct HardMap *out)
{
    size_t off;
    int i;

    if (p == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    off = (size_t)DINK_HARD_TILES * (size_t)DINK_HARD_REC;
    if (off + (size_t)DINK_BTILE_MAX * 4u > n) {
        return -1;
    }
    for (i = 0; i < DINK_BTILE_MAX; i++) {
        int32_t v;
        if (le_i32(p, n, off, &v) != 0) {
            return -1;
        }
        out->btile_default[i] = (uint16_t)(v < 0 ? 0 : v);
        off += 4;
    }
    return 0;
}

int hard_load(struct HardMap *out)
{
    FILE *fp;
    long sz;
    uint8_t *raw;
    int rc;

    fp = dink_fopen("hard.dat", "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    raw = (uint8_t *)malloc((size_t)sz);
    if (raw == NULL) {
        fclose(fp);
        return -1;
    }
    if (fread(raw, 1, (size_t)sz, fp) != (size_t)sz) {
        free(raw);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    rc = hard_parse_defaults(raw, (size_t)sz, out);
    free(raw);
    return rc;
}

int hard_id_for_tile(const struct HardMap *h, int32_t square_full_idx0,
                     int32_t althard)
{
    if (althard > 0) {
        return (int)althard;
    }
    if (h == NULL || square_full_idx0 < 0 || square_full_idx0 >= DINK_BTILE_MAX) {
        return 0;
    }
    return (int)h->btile_default[square_full_idx0];
}
