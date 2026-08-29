/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Convert official Dink WAV (PCM) to AICA-ready WAV.
 * Yamaha 4-bit ADPCM (WAVE format 0x14) unless PCM payload < 8 KiB
 * (then 16-bit PCM). Codec: YMZ/AICA step table (public-domain ymz_codec).
 */
#include "le.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WAVE_PCM 1
#define WAVE_YAMAHA_ADPCM 0x14
#define PCM_KEEP_MAX 8192
#define AICA_SFX_MAX_SAMPLES 65534

#define CLAMP(x, lo, hi) (((x) > (hi)) ? (hi) : (((x) < (lo)) ? (lo) : (x)))

static int16_t ymz_step(uint8_t step, int16_t *history, int16_t *step_size)
{
    static const int step_table[8] = {230, 230, 230, 230, 307, 409, 512, 614};
    int sign = step & 8;
    int delta = step & 7;
    int diff = ((1 + (delta << 1)) * *step_size) >> 3;
    int newval = *history;
    int nstep = (step_table[delta] * *step_size) >> 8;

    diff = CLAMP(diff, 0, 32767);
    if (sign > 0) {
        newval -= diff;
    } else {
        newval += diff;
    }
    *step_size = (int16_t)CLAMP(nstep, 127, 24576);
    *history = (int16_t)CLAMP(newval, -32768, 32767);
    return *history;
}

static void pcm2adpcm(uint8_t *out, const int16_t *in, size_t nsamp)
{
    int16_t step_size = 127;
    int16_t history = 0;
    uint8_t buf = 0;
    int nibble = 0;
    size_t i;

    for (i = 0; i < nsamp; i++) {
        int step = (in[i] & -8) - history;
        unsigned adpcm = (unsigned)((abs(step) << 16) / (step_size << 14));
        if (adpcm > 7) {
            adpcm = 7;
        }
        if (step < 0) {
            adpcm |= 8;
        }
        if (!nibble) {
            buf = (uint8_t)(adpcm & 0x0f);
        } else {
            *out++ = (uint8_t)(buf | (adpcm << 4));
        }
        nibble ^= 1;
        ymz_step((uint8_t)adpcm, &history, &step_size);
    }
    if (nibble) {
        *out = buf;
    }
}

#ifndef WAV_TO_ADPCM_NO_MAIN
static int ends_wav(const char *n)
{
    size_t L = strlen(n);
    return L >= 4 && (n[L - 4] == '.') &&
           (tolower((unsigned char)n[L - 3]) == 'w') &&
           (tolower((unsigned char)n[L - 2]) == 'a') &&
           (tolower((unsigned char)n[L - 1]) == 'v');
}
#endif

static int read_file(const char *path, uint8_t **out, size_t *n)
{
    FILE *fp = fopen(path, "rb");
    long sz;

    if (fp == NULL) {
        fprintf(stderr, "wav_to_adpcm: open %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (sz = ftell(fp)) < 12) {
        fclose(fp);
        fprintf(stderr, "wav_to_adpcm: bad size %s\n", path);
        return -1;
    }
    rewind(fp);
    *out = malloc((size_t)sz);
    if (*out == NULL) {
        fclose(fp);
        return -1;
    }
    if (fread_exact(fp, *out, (size_t)sz) != 0) {
        free(*out);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *n = (size_t)sz;
    return 0;
}

static int find_chunk(const uint8_t *p, size_t n, const char *id, size_t *off, uint32_t *csz)
{
    size_t i = 12;

    while (i + 8 <= n) {
        uint32_t sz;
        if (le_u32(p, n, i + 4, &sz) != 0) {
            return -1;
        }
        if (memcmp(p + i, id, 4) == 0) {
            *off = i + 8;
            *csz = sz;
            return 0;
        }
        i += 8 + (size_t)sz + (sz & 1u);
    }
    return -1;
}

static void wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int write_wav(const char *path, uint16_t fmt, uint16_t ch, uint32_t rate,
                     uint16_t bits, const void *data, uint32_t dsz)
{
    uint8_t hdr[44];
    FILE *fp;
    uint16_t ba = (uint16_t)((ch * bits) / 8);
    uint32_t bps = rate * ba;

    if (ba == 0) {
        ba = 1;
        bps = rate * ch * bits / 8;
    }
    memcpy(hdr, "RIFF", 4);
    wr_u32(hdr + 4, 36 + dsz);
    memcpy(hdr + 8, "WAVEfmt ", 8);
    wr_u32(hdr + 16, 16);
    wr_u16(hdr + 20, fmt);
    wr_u16(hdr + 22, ch);
    wr_u32(hdr + 24, rate);
    wr_u32(hdr + 28, bps);
    wr_u16(hdr + 32, ba);
    wr_u16(hdr + 34, bits);
    memcpy(hdr + 36, "data", 4);
    wr_u32(hdr + 40, dsz);
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "wav_to_adpcm: write %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fwrite(hdr, 1, 44, fp) != 44 || fwrite(data, 1, dsz, fp) != dsz) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int pcm_to_s16(const uint8_t *data, uint32_t dsz, uint16_t bits, uint16_t ch,
                      int16_t **out, size_t *nframes)
{
    size_t i, nf;
    int16_t *pcm;

    if (ch == 0) {
        return -1;
    }
    if (bits == 16) {
        nf = dsz / (2u * ch);
        pcm = malloc(nf * ch * sizeof(int16_t));
        if (pcm == NULL) {
            return -1;
        }
        for (i = 0; i < nf * ch; i++) {
            pcm[i] = (int16_t)(data[i * 2] | (data[i * 2 + 1] << 8));
        }
        *out = pcm;
        *nframes = nf;
        return 0;
    }
    if (bits == 8) {
        nf = dsz / ch;
        pcm = malloc(nf * ch * sizeof(int16_t));
        if (pcm == NULL) {
            return -1;
        }
        for (i = 0; i < nf * ch; i++) {
            pcm[i] = (int16_t)(((int)data[i] - 128) << 8);
        }
        *out = pcm;
        *nframes = nf;
        return 0;
    }
    return -1;
}

int wav_convert_path(const char *in_path, const char *out_path); /* tests */

int wav_convert_path(const char *in_path, const char *out_path)
{
    uint8_t *buf = NULL;
    size_t n = 0, fmt_off, data_off, nframes, i, nsamp;
    uint32_t fmt_sz, data_sz, rate;
    uint16_t afmt, ch, bits;
    int16_t *pcm = NULL;
    uint8_t *adp = NULL;
    size_t adpsz;
    int rc = -1;

    if (read_file(in_path, &buf, &n) != 0) {
        return -1;
    }
    if (n < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "wav_to_adpcm: not WAVE %s\n", in_path);
        goto done;
    }
    if (find_chunk(buf, n, "fmt ", &fmt_off, &fmt_sz) != 0 || fmt_sz < 16) {
        fprintf(stderr, "wav_to_adpcm: no fmt %s\n", in_path);
        goto done;
    }
    if (le_u16(buf, n, fmt_off, &afmt) != 0 || le_u16(buf, n, fmt_off + 2, &ch) != 0 ||
        le_u32(buf, n, fmt_off + 4, &rate) != 0 || le_u16(buf, n, fmt_off + 14, &bits) != 0) {
        goto done;
    }
    if (find_chunk(buf, n, "data", &data_off, &data_sz) != 0 || data_off + data_sz > n) {
        fprintf(stderr, "wav_to_adpcm: no data %s\n", in_path);
        goto done;
    }
    if (afmt == WAVE_YAMAHA_ADPCM || (afmt == WAVE_PCM && data_sz < PCM_KEEP_MAX && bits == 16)) {
        rc = write_wav(out_path, afmt, ch, rate, bits, buf + data_off, data_sz);
        goto done;
    }
    if (afmt != WAVE_PCM) {
        fprintf(stderr, "wav_to_adpcm: unsupported format %#x %s\n", afmt, in_path);
        goto done;
    }
    if (pcm_to_s16(buf + data_off, data_sz, bits, ch, &pcm, &nframes) != 0) {
        fprintf(stderr, "wav_to_adpcm: pcm %s\n", in_path);
        goto done;
    }
    nsamp = nframes * ch;
    if (nframes > AICA_SFX_MAX_SAMPLES) {
        fprintf(stderr,
                "wav_to_adpcm: warn %s nframes %zu > %d (KOS sfxmgr); convert full, load truncates\n",
                in_path, nframes, AICA_SFX_MAX_SAMPLES);
    }
    if (nframes * 2u * ch < PCM_KEEP_MAX) {
        rc = write_wav(out_path, WAVE_PCM, ch, rate, 16, pcm, (uint32_t)(nsamp * 2));
        goto done;
    }
    adpsz = (nsamp + 1) / 2;
    adp = calloc(1, adpsz ? adpsz : 1);
    if (adp == NULL) {
        goto done;
    }
    if (ch == 1) {
        pcm2adpcm(adp, pcm, nframes);
    } else {
        int16_t *left = malloc(nframes * 2);
        int16_t *right = malloc(nframes * 2);
        size_t chb = (nframes + 1) / 2;
        if (left == NULL || right == NULL) {
            free(left);
            free(right);
            goto done;
        }
        for (i = 0; i < nframes; i++) {
            left[i] = pcm[i * ch];
            right[i] = pcm[i * ch + 1];
        }
        pcm2adpcm(adp, left, nframes);
        pcm2adpcm(adp + chb, right, nframes);
        free(left);
        free(right);
        adpsz = chb * 2;
    }
    rc = write_wav(out_path, WAVE_YAMAHA_ADPCM, ch, rate, 4, adp, (uint32_t)adpsz);
    if (rc == 0) {
        printf("wav_to_adpcm: %s -> ADPCM %zu B (%zu frames %u Hz ch=%u)\n", in_path, adpsz,
               nframes, rate, ch);
    }
done:
    free(buf);
    free(pcm);
    free(adp);
    return rc;
}

#ifndef WAV_TO_ADPCM_NO_MAIN
static int convert_dir(const char *src, const char *dst)
{
    DIR *d;
    struct dirent *ent;
    int n = 0, fail = 0;

    if (dst != NULL && strcmp(src, dst) != 0) {
        if (mkdir(dst, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "wav_to_adpcm: mkdir %s: %s\n", dst, strerror(errno));
            return -1;
        }
    }
    d = opendir(src);
    if (d == NULL) {
        fprintf(stderr, "wav_to_adpcm: dir %s: %s\n", src, strerror(errno));
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        char in[512], out[512];
        if (!ends_wav(ent->d_name)) {
            continue;
        }
        snprintf(in, sizeof(in), "%s/%s", src, ent->d_name);
        if (dst == NULL || strcmp(src, dst) == 0) {
            snprintf(out, sizeof(out), "%s/%s", src, ent->d_name);
        } else {
            snprintf(out, sizeof(out), "%s/%s", dst, ent->d_name);
        }
        if (wav_convert_path(in, out) != 0) {
            fail++;
        } else {
            n++;
        }
    }
    closedir(d);
    printf("wav_to_adpcm: converted %d wav (%d fail) in %s\n", n, fail, src);
    return fail ? -1 : 0;
}

static void usage(void)
{
    fprintf(stderr,
            "wav_to_adpcm: PCM WAV -> AICA Yamaha ADPCM WAV (fmt 0x14)\n"
            "  wav_to_adpcm IN.wav OUT.wav\n"
            "  wav_to_adpcm --dir SoundDir --out build/sfx\n"
            "  wav_to_adpcm --dir staged/Sound --inplace\n"
            "PCM payload < 8 KiB stays 16-bit PCM. Output is not committed.\n");
}

int main(int argc, char **argv)
{
    const char *dir = NULL, *out = NULL;
    int inplace = 0, i;

    if (argc < 2) {
        usage();
        return 2;
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            dir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--inplace") == 0) {
            inplace = 1;
            continue;
        }
        if (argv[i][0] == '-') {
            usage();
            return 2;
        }
        break;
    }
    if (dir != NULL) {
        return convert_dir(dir, inplace ? dir : out) == 0 ? 0 : 1;
    }
    if (i + 1 >= argc) {
        usage();
        return 2;
    }
    return wav_convert_path(argv[i], argv[i + 1]) == 0 ? 0 : 1;
}
#endif
