/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_FF_H
#define DINKCAST_FF_H

#include <stddef.h>
#include <stdint.h>

#define DINK_FF_NAME 13

struct FfEntry {
    uint32_t off;
    char name[DINK_FF_NAME];
};

struct FfFile {
    uint8_t *data;
    size_t n;
    struct FfEntry *ent;
    int nent;
    int borrowed; /* 1 = data is dink_blob_get; ff_free must not free it */
};

void ff_free(struct FfFile *ff);
int ff_parse_mem(const uint8_t *p, size_t n, struct FfFile *out);
int ff_load_rel(const char *rel, struct FfFile *out);
/* Keep idle+walk resident. Re-fread of walk/dir.ff from /cd can hang. */
int ff_cached(const char *rel, struct FfFile **out);
/* 1 if this pack is already in the session cache (no fopen). */
int ff_is_cached(const char *rel);
int ff_disc_loads(void);
void ff_cache_clear(void);
void ff_cache_drop_unpinned(void);
/* Case-insensitive 8.3 name. Points into ff->data; not a copy. */
int ff_find(const struct FfFile *ff, const char *name, const uint8_t **ptr,
            size_t *len);

#endif
