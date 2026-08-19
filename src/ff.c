/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ff.h"

#include "fs.h"
#include "le.h"

#include <ctype.h>
#include <stdio.h>
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

static int ff_parse_ents(const uint8_t *p, size_t n, struct FfFile *out)
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
    return 0;
}

int ff_parse_mem(const uint8_t *p, size_t n, struct FfFile *out)
{
    if (ff_parse_ents(p, n, out) != 0) {
        return -1;
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
    uint8_t *raw;
    size_t n;

    if (rel == NULL || out == NULL) {
        return -1;
    }
    printf("ff load %s\n", rel);
    raw = NULL;
    n = 0;
    if (dink_slurp_rel(rel, &raw, &n) != 0 || n < 4) {
        free(raw);
        return -1;
    }
    if (ff_parse_ents(raw, n, out) != 0) {
        free(raw);
        return -1;
    }
    out->data = raw;
    out->n = n;
    printf("ff ok %s %u\n", rel, (unsigned)n);
    return 0;
}

#define DINK_FF_SLOTS 32
#define DINK_FF_PIN_BYTES (80u * 1024u)

static struct {
    char rel[160];
    struct FfFile ff;
    int pin;
    int tick;
} g_slot[DINK_FF_SLOTS];
static int g_tick;
static int g_disc_loads;

static int rel_has(const char *rel, const char *needle)
{
    size_t i, j;

    if (rel == NULL || needle == NULL) {
        return 0;
    }
    for (i = 0; rel[i] != '\0'; i++) {
        for (j = 0; needle[j] != '\0'; j++) {
            char a = rel[i + j];
            char b = needle[j];

            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

int ff_disc_loads(void)
{
    return g_disc_loads;
}

void ff_cache_clear(void)
{
    int i;

    for (i = 0; i < DINK_FF_SLOTS; i++) {
        ff_free(&g_slot[i].ff);
        g_slot[i].rel[0] = '\0';
        g_slot[i].pin = 0;
        g_slot[i].tick = 0;
    }
    g_tick = 0;
}

void ff_cache_drop_unpinned(void)
{
    /* Large packs stay. Reopening trees/home/walls hangs /cd. */
}

int ff_cached(const char *rel, struct FfFile **out)
{
    int i, hit = -1, empty = -1, victim = -1, pin;

    if (rel == NULL || rel[0] == '\0' || out == NULL) {
        return -1;
    }
    pin = rel_has(rel, "dink/idle") || rel_has(rel, "dink/walk");
    g_tick++;
    for (i = 0; i < DINK_FF_SLOTS; i++) {
        if (g_slot[i].rel[0] != '\0' && strcmp(g_slot[i].rel, rel) == 0 &&
            g_slot[i].ff.data != NULL) {
            hit = i;
            break;
        }
        if (empty < 0 && g_slot[i].rel[0] == '\0') {
            empty = i;
        }
        if (!g_slot[i].pin &&
            (victim < 0 || g_slot[i].tick < g_slot[victim].tick)) {
            victim = i;
        }
    }
    if (hit >= 0) {
        g_slot[hit].tick = g_tick;
        printf("ff hit %s\n", rel);
        *out = &g_slot[hit].ff;
        return 0;
    }
    if (empty < 0) {
        if (victim < 0) {
            printf("ff cache full no victim %s\n", rel);
            return -1;
        }
        printf("ff evict %s\n", g_slot[victim].rel);
        empty = victim;
        ff_free(&g_slot[empty].ff);
        g_slot[empty].rel[0] = '\0';
        g_slot[empty].pin = 0;
    }
    if (ff_load_rel(rel, &g_slot[empty].ff) != 0) {
        g_slot[empty].rel[0] = '\0';
        return -1;
    }
    if (g_slot[empty].ff.n >= DINK_FF_PIN_BYTES) {
        pin = 1;
    }
    snprintf(g_slot[empty].rel, sizeof(g_slot[empty].rel), "%s", rel);
    g_slot[empty].pin = pin;
    g_slot[empty].tick = g_tick;
    g_disc_loads++;
    *out = &g_slot[empty].ff;
    return 0;
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
