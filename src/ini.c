/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ini.h"

#include "ff.h"
#include "fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void slash(char *s)
{
    for (; *s; s++) {
        if (*s == '\\') {
            *s = '/';
        }
    }
}

static int tok_int(const char **pp, int *out)
{
    char *end;
    long v;

    while (**pp == ' ' || **pp == '\t') {
        (*pp)++;
    }
    if (!isdigit((unsigned char)**pp) && **pp != '-') {
        return -1;
    }
    v = strtol(*pp, &end, 10);
    if (end == *pp) {
        return -1;
    }
    *pp = end;
    *out = (int)v;
    return 0;
}

int ini_parse_mem(const char *text, size_t n, struct SeqInfo *seqs, int nseq)
{
    size_t i = 0;

    if (text == NULL || seqs == NULL || nseq < DINK_MAX_SEQ) {
        return -1;
    }
    memset(seqs, 0, (size_t)nseq * sizeof(*seqs));
    while (i < n) {
        char line[256];
        size_t L = 0;
        const char *p;
        int seq = 0, delay = 0, cx = 0, cy = 0, hl = 0, ht = 0, hr = 0, hb = 0;
        char prefix[128];

        while (i < n && text[i] != '\n' && L + 1 < sizeof(line)) {
            if (text[i] != '\r') {
                line[L++] = text[i];
            }
            i++;
        }
        if (i < n && text[i] == '\n') {
            i++;
        }
        line[L] = '\0';
        p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0' || *p == ';' || *p == '/') {
            continue;
        }
        if (strncmp(p, "load_sequence_now", 17) == 0) {
            p += 17;
        } else if (strncmp(p, "load_sequence", 13) == 0) {
            p += 13;
        } else {
            continue;
        }
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        {
            int k = 0;
            while (*p && *p != ' ' && *p != '\t' && k + 1 < (int)sizeof(prefix)) {
                prefix[k++] = *p++;
            }
            prefix[k] = '\0';
        }
        slash(prefix);
        if (tok_int(&p, &seq) != 0 || seq < 1 || seq >= DINK_MAX_SEQ) {
            continue;
        }
        (void)tok_int(&p, &delay);
        (void)tok_int(&p, &cx);
        (void)tok_int(&p, &cy);
        (void)tok_int(&p, &hl);
        (void)tok_int(&p, &ht);
        (void)tok_int(&p, &hr);
        (void)tok_int(&p, &hb);
        strncpy(seqs[seq].prefix, prefix, sizeof(seqs[seq].prefix) - 1);
        seqs[seq].delay = delay;
        seqs[seq].cx = cx;
        seqs[seq].cy = cy;
        seqs[seq].hl = hl;
        seqs[seq].ht = ht;
        seqs[seq].hr = hr;
        seqs[seq].hb = hb;
    }
    return 0;
}

int ini_load(struct SeqInfo *seqs, int nseq)
{
    FILE *fp;
    long sz;
    char *raw;
    int rc;

    fp = dink_fopen("dink.ini", "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 8 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    raw = (char *)malloc((size_t)sz + 1);
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
    raw[sz] = '\0';
    rc = ini_parse_mem(raw, (size_t)sz, seqs, nseq);
    free(raw);
    return rc;
}

int ini_count_ff_frames(const char *prefix)
{
    char dir[160], base[32], name[24];
    const char *slash;
    struct FfFile ff;
    int n = 0, i;

    if (prefix == NULL || prefix[0] == '\0') {
        return 0;
    }
    slash = strrchr(prefix, '/');
    if (slash == NULL) {
        return 0;
    }
    snprintf(dir, sizeof(dir), "%.*s/dir.ff", (int)(slash - prefix), prefix);
    snprintf(base, sizeof(base), "%s", slash + 1);
    memset(&ff, 0, sizeof(ff));
    if (ff_load_rel(dir, &ff) != 0) {
        return 0;
    }
    for (i = 1; i < DINK_MAX_FRAMES; i++) {
        const uint8_t *p;
        size_t ln;
        snprintf(name, sizeof(name), i < 10 ? "%s0%d.bmp" : "%s%d.bmp", base, i);
        if (ff_find(&ff, name, &p, &ln) != 0) {
            break;
        }
        n = i;
    }
    ff_free(&ff);
    return n;
}
