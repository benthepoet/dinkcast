/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ff.h"

#include "fs.h"
#include "le.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void ff_free(struct FfFile *ff)
{
    if (ff == NULL) {
        return;
    }
    free(ff->data);
    free(ff->ent);
    memset(ff, 0, sizeof(*ff));
}

static int name_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int ff_parse_mem(const uint8_t *p, size_t n, struct FfFile *out)
{
    uint32_t nent;
    size_t off;
    int i;

    if (p == NULL || out == NULL || n < 4) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (le_u32(p, n, 0, &nent) != 0 || nent < 2 || nent > 4096) {
        return -1;
    }
    off = 4;
    out->ent = (struct FfEntry *)calloc(nent, sizeof(struct FfEntry));
    if (out->ent == NULL) {
        return -1;
    }
    out->nent = (int)nent;
    for (i = 0; i < out->nent; i++) {
        if (off + 4 + 13 > n) {
            ff_free(out);
            return -1;
        }
        if (le_u32(p, n, off, &out->ent[i].off) != 0) {
            ff_free(out);
            return -1;
        }
        memcpy(out->ent[i].name, p + off + 4, 12);
        out->ent[i].name[12] = '\0';
        off += 17;
    }
    out->data = (uint8_t *)malloc(n);
    if (out->data == NULL) {
        ff_free(out);
        return -1;
    }
    memcpy(out->data, p, n);
    out->n = n;
    return 0;
}

int ff_load_rel(const char *rel, struct FfFile *out)
{
    FILE *fp;
    long sz;
    uint8_t *raw;
    int rc;

    fp = dink_fopen(rel, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 4 || fseek(fp, 0, SEEK_SET) != 0) {
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
    rc = ff_parse_mem(raw, (size_t)sz, out);
    free(raw);
    return rc;
}

int ff_find(const struct FfFile *ff, const char *name, const uint8_t **ptr,
            size_t *len)
{
    int i;

    if (ff == NULL || name == NULL || ptr == NULL || len == NULL) {
        return -1;
    }
    for (i = 0; i < ff->nent - 1; i++) {
        uint32_t next;

        if (!name_eq(ff->ent[i].name, name)) {
            continue;
        }
        next = ff->ent[i + 1].off;
        if (next == 0 && i + 2 < ff->nent) {
            next = ff->ent[i + 2].off;
        }
        if (ff->ent[i].off > ff->n || next > ff->n || next < ff->ent[i].off) {
            return -1;
        }
        *ptr = ff->data + ff->ent[i].off;
        *len = (size_t)(next - ff->ent[i].off);
        return 0;
    }
    return -1;
}
