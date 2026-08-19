/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ini.h"

#include "ff.h"
#include "fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ini_nframe;
struct IniFrame ini_frame[DINK_SSI_MAX];

struct IniSpecial {
    int seq, frame, on;
};

static int ini_nspecial;
static struct IniSpecial ini_special[DINK_SPECIAL_MAX];

int ini_frame_special(int seq, int frame)
{
    int i;

    for (i = 0; i < ini_nspecial; i++) {
        if (ini_special[i].seq == seq && ini_special[i].frame == frame) {
            return ini_special[i].on != 0;
        }
    }
    return 0;
}

static void ini_store_special(int seq, int frame, int on)
{
    int i;

    if (seq < 1 || frame < 1) {
        return;
    }
    for (i = 0; i < ini_nspecial; i++) {
        if (ini_special[i].seq == seq && ini_special[i].frame == frame) {
            ini_special[i].on = on;
            return;
        }
    }
    if (ini_nspecial >= DINK_SPECIAL_MAX) {
        return;
    }
    ini_special[ini_nspecial].seq = seq;
    ini_special[ini_nspecial].frame = frame;
    ini_special[ini_nspecial].on = on;
    ini_nspecial++;
}

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

static void ini_store_frame(int seq, int frame, int cx, int cy, int hl, int ht,
                            int hr, int hb)
{
    int i;

    if (seq < 1 || frame < 1) {
        return;
    }
    for (i = 0; i < ini_nframe; i++) {
        if (ini_frame[i].seq == seq && ini_frame[i].frame == frame) {
            ini_frame[i].cx = cx;
            ini_frame[i].cy = cy;
            ini_frame[i].hl = hl;
            ini_frame[i].ht = ht;
            ini_frame[i].hr = hr;
            ini_frame[i].hb = hb;
            return;
        }
    }
    if (ini_nframe >= DINK_SSI_MAX) {
        return;
    }
    ini_frame[ini_nframe].seq = seq;
    ini_frame[ini_nframe].frame = frame;
    ini_frame[ini_nframe].cx = cx;
    ini_frame[ini_nframe].cy = cy;
    ini_frame[ini_nframe].hl = hl;
    ini_frame[ini_nframe].ht = ht;
    ini_frame[ini_nframe].hr = hr;
    ini_frame[ini_nframe].hb = hb;
    ini_nframe++;
}

void ini_frame_geom(const struct SeqInfo *seq, int seqn, int frame, int fw,
                    int fh, int *cx, int *cy, int *hl, int *ht, int *hr,
                    int *hb)
{
    int i;

    if (cx == NULL || cy == NULL || hl == NULL || ht == NULL || hr == NULL ||
        hb == NULL) {
        return;
    }
    *cx = (fw - fw / 2) + fw / 6;
    *cy = (fh - fh / 4) - fh / 30;
    *hl = -(fw / 4);
    *ht = -(fh / 10);
    *hr = fw / 4;
    *hb = fh / 10;
    if (seq != NULL && seq->hr > 0) {
        *cx = seq->cx > 0 ? seq->cx : *cx;
        *cy = seq->cy > 0 ? seq->cy : *cy;
        *hl = seq->hl;
        *ht = seq->ht;
        *hr = seq->hr;
        *hb = seq->hb;
    }
    for (i = 0; i < ini_nframe; i++) {
        if (ini_frame[i].seq == seqn && ini_frame[i].frame == frame) {
            *cx = ini_frame[i].cx;
            *cy = ini_frame[i].cy;
            *hl = ini_frame[i].hl;
            *ht = ini_frame[i].ht;
            *hr = ini_frame[i].hr;
            *hb = ini_frame[i].hb;
            return;
        }
    }
}

int ini_parse_mem(const char *text, size_t n, struct SeqInfo *seqs, int nseq)
{
    size_t i = 0;

    if (text == NULL || seqs == NULL || nseq < DINK_MAX_SEQ) {
        return -1;
    }
    memset(seqs, 0, (size_t)nseq * sizeof(*seqs));
    ini_nframe = 0;
    memset(ini_frame, 0, sizeof(ini_frame));
    ini_nspecial = 0;
    memset(ini_special, 0, sizeof(ini_special));
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
        {
            char cmd[24];
            int c = 0;

            while (p[c] && p[c] != ' ' && p[c] != '\t' &&
                   c + 1 < (int)sizeof(cmd)) {
                cmd[c] = (char)tolower((unsigned char)p[c]);
                c++;
            }
            cmd[c] = '\0';
            p += c;
            if (strcmp(cmd, "set_sprite_info") == 0) {
                if (tok_int(&p, &seq) == 0 && tok_int(&p, &delay) == 0) {
                    (void)tok_int(&p, &cx);
                    (void)tok_int(&p, &cy);
                    (void)tok_int(&p, &hl);
                    (void)tok_int(&p, &ht);
                    (void)tok_int(&p, &hr);
                    (void)tok_int(&p, &hb);
                    ini_store_frame(seq, delay, cx, cy, hl, ht, hr, hb);
                }
                continue;
            }
            if (strcmp(cmd, "set_frame_special") == 0) {
                int fr = 0, on = 0;

                if (tok_int(&p, &seq) == 0 && tok_int(&p, &fr) == 0) {
                    (void)tok_int(&p, &on);
                    ini_store_special(seq, fr, on);
                }
                continue;
            }
            if (strcmp(cmd, "load_sequence_now") != 0 &&
                strcmp(cmd, "load_sequence") != 0) {
                continue;
            }
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
    const uint8_t *blob;
    size_t sz;
    char *raw;
    int rc;

    blob = NULL;
    sz = 0;
    if (dink_blob_get("dink.ini", &blob, &sz) != 0 || blob == NULL || sz < 8) {
        return -1;
    }
    raw = (char *)malloc(sz + 1);
    if (raw == NULL) {
        return -1;
    }
    memcpy(raw, blob, sz);
    raw[sz] = '\0';
    rc = ini_parse_mem(raw, sz, seqs, nseq);
    free(raw);
    return rc;
}

int ini_count_ff_frames(const char *prefix)
{
    char dir[160], base[32], name[24];
    const char *slash;
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
    {
        struct FfFile *cached = NULL;

        if (ff_cached(dir, &cached) != 0 || cached == NULL) {
            return 0;
        }
        for (i = 1; i < DINK_MAX_FRAMES; i++) {
            const uint8_t *p;
            size_t ln;
            snprintf(name, sizeof(name), i < 10 ? "%s0%d.bmp" : "%s%d.bmp",
                     base, i);
            if (ff_find(cached, name, &p, &ln) != 0) {
                break;
            }
            n = i;
        }
    }
    return n;
}
