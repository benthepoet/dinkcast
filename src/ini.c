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

struct IniAlias {
    int seq, frame, dest_seq, dest_frame;
};

static int ini_nalias;
static struct IniAlias ini_alias[DINK_ALIAS_MAX];

struct IniFDelay {
    int seq, frame, delay;
};

static int ini_nfdelay;
static struct IniFDelay ini_fdelay[DINK_FDELAY_MAX];

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

int ini_resolve_frame(int seq, int frame, int *oseq, int *ofr)
{
    int i;

    if (oseq != NULL) {
        *oseq = seq;
    }
    if (ofr != NULL) {
        *ofr = frame;
    }
    for (i = 0; i < ini_nalias; i++) {
        if (ini_alias[i].seq == seq && ini_alias[i].frame == frame) {
            if (ini_alias[i].dest_seq < 0 || ini_alias[i].dest_frame < 0) {
                return 1;
            }
            if (oseq != NULL) {
                *oseq = ini_alias[i].dest_seq;
            }
            if (ofr != NULL) {
                *ofr = ini_alias[i].dest_frame;
            }
            return 0;
        }
    }
    return 0;
}

int ini_frame_delay(int seq, int frame, int seq_default)
{
    int i;

    for (i = 0; i < ini_nfdelay; i++) {
        if (ini_fdelay[i].seq == seq && ini_fdelay[i].frame == frame) {
            return ini_fdelay[i].delay > 0 ? ini_fdelay[i].delay : seq_default;
        }
    }
    return seq_default;
}

int ini_seq_len(int seq, int bmp_nframes)
{
    int i, n, term;

    n = bmp_nframes > 0 ? bmp_nframes : 0;
    term = 0;
    for (i = 0; i < ini_nalias; i++) {
        if (ini_alias[i].seq != seq) {
            continue;
        }
        if (ini_alias[i].dest_seq < 0 || ini_alias[i].dest_frame < 0) {
            if (term == 0 || ini_alias[i].frame < term) {
                term = ini_alias[i].frame;
            }
            continue;
        }
        if (ini_alias[i].frame > n) {
            n = ini_alias[i].frame;
        }
    }
    if (term > 0 && (n < 1 || term - 1 < n)) {
        n = term - 1;
    }
    return n > 0 ? n : 0;
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

static void ini_store_alias(int seq, int frame, int dest_seq, int dest_frame)
{
    int i;

    if (seq < 1 || frame < 1) {
        return;
    }
    for (i = 0; i < ini_nalias; i++) {
        if (ini_alias[i].seq == seq && ini_alias[i].frame == frame) {
            ini_alias[i].dest_seq = dest_seq;
            ini_alias[i].dest_frame = dest_frame;
            return;
        }
    }
    if (ini_nalias >= DINK_ALIAS_MAX) {
        return;
    }
    ini_alias[ini_nalias].seq = seq;
    ini_alias[ini_nalias].frame = frame;
    ini_alias[ini_nalias].dest_seq = dest_seq;
    ini_alias[ini_nalias].dest_frame = dest_frame;
    ini_nalias++;
}

static void ini_store_fdelay(int seq, int frame, int delay)
{
    int i;

    if (seq < 1 || frame < 1) {
        return;
    }
    for (i = 0; i < ini_nfdelay; i++) {
        if (ini_fdelay[i].seq == seq && ini_fdelay[i].frame == frame) {
            ini_fdelay[i].delay = delay;
            return;
        }
    }
    if (ini_nfdelay >= DINK_FDELAY_MAX) {
        return;
    }
    ini_fdelay[ini_nfdelay].seq = seq;
    ini_fdelay[ini_nfdelay].frame = frame;
    ini_fdelay[ini_nfdelay].delay = delay;
    ini_nfdelay++;
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

static int tok_kw(const char *p, const char *w)
{
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    while (*w) {
        if (tolower((unsigned char)*p) != tolower((unsigned char)*w)) {
            return 0;
        }
        p++;
        w++;
    }
    return *p == '\0' || *p == ' ' || *p == '\t';
}

static void skip_word(const char **pp)
{
    while (**pp == ' ' || **pp == '\t') {
        (*pp)++;
    }
    while (**pp && **pp != ' ' && **pp != '\t') {
        (*pp)++;
    }
}

/* FreeDink figure_out: BLACK/LEFTALIGN/NOTANIM vs delay+offsets. */
static void parse_load_tail(const char **pp, int *delay, int *cx, int *cy,
                            int *hl, int *ht, int *hr, int *hb, int *reuse)
{
    *delay = 0;
    *cx = *cy = 0;
    *hl = *ht = *hr = *hb = 0;
    *reuse = 0;
    while (**pp == ' ' || **pp == '\t') {
        (*pp)++;
    }
    if (tok_kw(*pp, "BLACK")) {
        *reuse = 1;
        skip_word(pp);
        return;
    }
    if (tok_kw(*pp, "LEFTALIGN")) {
        skip_word(pp);
        return;
    }
    if (tok_kw(*pp, "NOTANIM")) {
        skip_word(pp);
        return;
    }
    /* Animation: flags = DINKINI_NOTANIM (reuse frame 1 offsets). */
    *reuse = 1;
    (void)tok_int(pp, delay);
    (void)tok_int(pp, cx);
    (void)tok_int(pp, cy);
    (void)tok_int(pp, hl);
    (void)tok_int(pp, ht);
    (void)tok_int(pp, hr);
    (void)tok_int(pp, hb);
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
    /* FreeDink load_sprite_pak: xoffset/yoffset if >0, even when hardbox.right
     * is 0 (item-pig ARM omits the box on seq 74/76). Gating center on hr
     * made left/right walk jump every frame. */
    if (seq != NULL) {
        if (seq->cx > 0) {
            *cx = seq->cx;
        }
        if (seq->cy > 0) {
            *cy = seq->cy;
        }
        /* load_sprite_pak: right>0 and bottom>0 independently.
         * Seq 421 food omits bottom (0); using ht=21 hb=0 inverted the
         * touch box so BAR-F1 loot could not be picked up. */
        if (seq->hr > 0) {
            *hl = seq->hl;
            *hr = seq->hr;
        }
        if (seq->hb > 0) {
            *ht = seq->ht;
            *hb = seq->hb;
        }
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
    ini_nalias = 0;
    memset(ini_alias, 0, sizeof(ini_alias));
    ini_nfdelay = 0;
    memset(ini_fdelay, 0, sizeof(ini_fdelay));
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
            if (strcmp(cmd, "set_frame_frame") == 0) {
                int fr = 0, dseq = -1, dfr = -1;

                if (tok_int(&p, &seq) == 0 && tok_int(&p, &fr) == 0) {
                    (void)tok_int(&p, &dseq);
                    (void)tok_int(&p, &dfr);
                    ini_store_alias(seq, fr, dseq, dfr);
                }
                continue;
            }
            if (strcmp(cmd, "set_frame_delay") == 0) {
                int fr = 0, dly = 0;

                if (tok_int(&p, &seq) == 0 && tok_int(&p, &fr) == 0) {
                    (void)tok_int(&p, &dly);
                    ini_store_fdelay(seq, fr, dly);
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
        {
            int reuse = 0;

            parse_load_tail(&p, &delay, &cx, &cy, &hl, &ht, &hr, &hb, &reuse);
            strncpy(seqs[seq].prefix, prefix, sizeof(seqs[seq].prefix) - 1);
            seqs[seq].delay = delay;
            seqs[seq].cx = cx;
            seqs[seq].cy = cy;
            seqs[seq].hl = hl;
            seqs[seq].ht = ht;
            seqs[seq].hr = hr;
            seqs[seq].hb = hb;
            seqs[seq].reuse_off = reuse;
        }
    }
    return 0;
}

int ini_apply_line(const char *line, struct SeqInfo *seqs, int nseq)
{
    const char *p;
    int seq = 0, delay = 0, cx = 0, cy = 0, hl = 0, ht = 0, hr = 0, hb = 0;
    char prefix[128];
    char cmd[24];
    int c = 0;

    if (line == NULL || seqs == NULL || nseq < 1) {
        return -1;
    }
    p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    while (p[c] && p[c] != ' ' && p[c] != '\t' && c + 1 < (int)sizeof(cmd)) {
        cmd[c] = (char)tolower((unsigned char)p[c]);
        c++;
    }
    cmd[c] = '\0';
    p += c;
    if (strcmp(cmd, "load_sequence_now") != 0 &&
        strcmp(cmd, "load_sequence") != 0) {
        return 0;
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
    if (tok_int(&p, &seq) != 0 || seq < 1 || seq >= nseq || seq >= DINK_MAX_SEQ) {
        return -1;
    }
    {
        int reuse = 0;

        parse_load_tail(&p, &delay, &cx, &cy, &hl, &ht, &hr, &hb, &reuse);
        strncpy(seqs[seq].prefix, prefix, sizeof(seqs[seq].prefix) - 1);
        seqs[seq].prefix[sizeof(seqs[seq].prefix) - 1] = '\0';
        seqs[seq].delay = delay;
        seqs[seq].cx = cx;
        seqs[seq].cy = cy;
        seqs[seq].hl = hl;
        seqs[seq].ht = ht;
        seqs[seq].hr = hr;
        seqs[seq].hb = hb;
        seqs[seq].reuse_off = reuse;
        seqs[seq].nframes = ini_seq_len(seq, ini_count_ff_frames(prefix));
    }
    return seq;
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
