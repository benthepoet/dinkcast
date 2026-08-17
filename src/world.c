/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "world.h"

#include "fs.h"
#include "le.h"

#include <stdlib.h>
#include <string.h>

int world_parse_mem(const uint8_t *p, size_t n, struct World *out)
{
    size_t off;
    int i;

    if (p == NULL || out == NULL || n < DINK_DAT_SIZE) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    off = DINK_DAT_IDENT;
    for (i = 0; i < DINK_WORLD_SLOTS; i++) {
        if (le_i32(p, n, off, &out->loc[i]) != 0) {
            return -1;
        }
        off += 4;
    }
    for (i = 0; i < DINK_WORLD_SLOTS; i++) {
        if (le_i32(p, n, off, &out->music[i]) != 0) {
            return -1;
        }
        off += 4;
    }
    for (i = 0; i < DINK_WORLD_SLOTS; i++) {
        if (le_i32(p, n, off, &out->indoor[i]) != 0) {
            return -1;
        }
        off += 4;
    }
    return 0;
}

int world_load(struct World *out)
{
    FILE *fp;
    uint8_t *raw;
    long sz;
    int rc;

    if (out == NULL) {
        return -1;
    }
    fp = dink_fopen("dink.dat", "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < DINK_DAT_SIZE || fseek(fp, 0, SEEK_SET) != 0) {
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
    rc = world_parse_mem(raw, (size_t)sz, out);
    free(raw);
    return rc;
}

int world_loc_nonzero_count(const struct World *w)
{
    int i, n = 0;

    if (w == NULL) {
        return 0;
    }
    for (i = 0; i < DINK_WORLD_SLOTS; i++) {
        if (w->loc[i] != 0) {
            n++;
        }
    }
    return n;
}

int32_t world_max_loc(const struct World *w)
{
    int i;
    int32_t m = 0;

    if (w == NULL) {
        return 0;
    }
    for (i = 0; i < DINK_WORLD_SLOTS; i++) {
        if (w->loc[i] > m) {
            m = w->loc[i];
        }
    }
    return m;
}
