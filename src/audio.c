/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"

#include "fs.h"
#include "le.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#endif

#define DINK_SFX_SLOTS 64
#define DINK_VOICES 16
#define AICA_SFX_CAP (512u * 1024u)
#ifdef _arch_dreamcast
#define AICA_SFX_MAX_SAMPLES 65534
#define WAVE_PCM 1
#define WAVE_YAMAHA_ADPCM 0x14
#endif

/* Story/START.c main() load_sound — do not run START main(). */
static const struct {
    int id;
    const char *file;
} k_start[] = {
    {1, "QUACK.WAV"},   {2, "PIG1.WAV"},    {3, "PIG2.WAV"},    {4, "PIG3.WAV"},
    {5, "PIG4.WAV"},    {6, "BURN.WAV"},    {7, "OPEN.WAV"},    {8, "SWING.WAV"},
    {9, "PUNCH.WAV"},   {10, "SWORD2.WAV"}, {11, "SELECT.WAV"}, {12, "WSCREAM.WAV"},
    {13, "PICKER.WAV"}, {14, "GOLD.WAV"},   {15, "GRUNT1.WAV"}, {16, "GRUNT2.WAV"},
    {17, "SEL1.WAV"},   {18, "ESCAPE.WAV"}, {19, "NONO.WAV"},   {20, "SEL2.WAV"},
    {21, "SEL3.WAV"},   {22, "HIGH2.WAV"},  {23, "FIRE.WAV"},   {24, "SPELL1.WAV"},
    {25, "CAVEENT.WAV"}, {26, "SNARL1.WAV"}, {27, "SNARL2.WAV"}, {28, "SNARL3.WAV"},
    {29, "HURT1.WAV"},  {30, "HURT2.WAV"},  {31, "ATTACK1.WAV"}, {32, "CAVEENT.WAV"},
    {33, "LEVEL.WAV"},  {34, "SAVE.WAV"},   {35, "SPLASH.WAV"}, {36, "SWORD1.WAV"},
    {37, "BHIT.WAV"},   {38, "SQUISH.WAV"}, {39, "STAIRS.WAV"}, {40, "STEPS.WAV"},
    {41, "ARROW.WAV"},  {42, "FLYBY.WAV"},  {43, "SECRET.WAV"}, {44, "BOW1.WAV"},
    {45, "KNOCK.WAV"},  {46, "DRAG1.WAV"},  {47, "DRAG2.WAV"},  {48, "AXE.WAV"},
    {49, "BIRD1.WAV"},
};

static struct {
    int loaded;
    size_t bytes;
    char file[16];
#ifdef _arch_dreamcast
    sfxhnd_t hnd;
#endif
} g_s[DINK_SFX_SLOTS];

static struct {
    int used;
    int ch;
    unsigned age;
} g_v[DINK_VOICES];

static int g_on;
static unsigned g_age;
static size_t g_bytes;
static int g_nslot;

static FILE *open_wav(const char *file)
{
    const char *dir[2] = {"Sound", "sound"};
    char rel[64];
    FILE *fp;
    int i;

    if (file == NULL) {
        return NULL;
    }
    for (i = 0; i < 2; i++) {
        snprintf(rel, sizeof(rel), "%s/%s", dir[i], file);
        fp = dink_fopen(rel, "rb");
        if (fp != NULL) {
            return fp;
        }
    }
    return NULL;
}

#ifdef _arch_dreamcast
static int wav_nframes(const uint8_t *p, size_t n, uint32_t *dsz, size_t *data_off,
                       uint16_t *fmt, uint16_t *bits, uint16_t *ch)
{
    size_t i = 12;
    uint32_t fsz = 0, dbytes = 0;
    size_t foff = 0, doff = 0;
    uint16_t af = 0, c = 1, b = 16;

    if (n < 44 || memcmp(p, "RIFF", 4) != 0) {
        return -1;
    }
    while (i + 8 <= n) {
        uint32_t sz;
        if (le_u32(p, n, i + 4, &sz) != 0) {
            return -1;
        }
        if (memcmp(p + i, "fmt ", 4) == 0) {
            foff = i + 8;
            fsz = sz;
        } else if (memcmp(p + i, "data", 4) == 0) {
            doff = i + 8;
            dbytes = sz;
            break;
        }
        i += 8 + (size_t)sz + (sz & 1u);
    }
    if (fsz < 16 || doff == 0 || doff + dbytes > n) {
        return -1;
    }
    le_u16(p, n, foff, &af);
    le_u16(p, n, foff + 2, &c);
    le_u16(p, n, foff + 14, &b);
    *fmt = af;
    *bits = b;
    *ch = c;
    *dsz = dbytes;
    *data_off = doff;
    if (af == WAVE_YAMAHA_ADPCM || b == 4) {
        return (int)(dbytes * 2 / (c ? c : 1));
    }
    if (b >= 8) {
        return (int)(dbytes / ((b / 8) * (c ? c : 1)));
    }
    return -1;
}
#endif

#ifdef _arch_dreamcast
static sfxhnd_t load_dc_fp(FILE *fp, size_t *out_bytes)
{
    uint8_t *buf = NULL;
    size_t n = 0;
    uint32_t dsz;
    size_t doff;
    uint16_t fmt, bits, ch;
    int nfr;
    sfxhnd_t h;

    if (fp == NULL) {
        return 0;
    }
    if (dink_fread_all(fp, &buf, &n) != 0 || buf == NULL) {
        return 0;
    }
    nfr = wav_nframes(buf, n, &dsz, &doff, &fmt, &bits, &ch);
    if (nfr > AICA_SFX_MAX_SAMPLES) {
        uint32_t keep;
        if (fmt == WAVE_YAMAHA_ADPCM || bits == 4) {
            keep = (uint32_t)((AICA_SFX_MAX_SAMPLES + 1) / 2);
        } else {
            keep = (uint32_t)AICA_SFX_MAX_SAMPLES * (bits / 8) * (ch ? ch : 1);
        }
        if (keep > dsz) {
            keep = dsz;
        }
        buf[doff - 4] = (uint8_t)keep;
        buf[doff - 3] = (uint8_t)(keep >> 8);
        buf[doff - 2] = (uint8_t)(keep >> 16);
        buf[doff - 1] = (uint8_t)(keep >> 24);
        n = doff + keep;
        printf("audio trunc nfr=%d keep=%u\n", nfr, keep);
        nfr = AICA_SFX_MAX_SAMPLES;
    }
    h = snd_sfx_load_buf((char *)buf);
    if (out_bytes != NULL) {
        *out_bytes = (nfr > 0 && bits == 4) ? (size_t)((nfr + 1) / 2) : (size_t)dsz;
    }
    free(buf);
    return h;
}
#endif

static int load_slot(int id, const char *file)
{
    FILE *fp;
    int j;
    size_t bytes = 0;

    if (id < 1 || id >= DINK_SFX_SLOTS || file == NULL) {
        return -1;
    }
    strncpy(g_s[id].file, file, sizeof(g_s[id].file) - 1);
    g_s[id].file[sizeof(g_s[id].file) - 1] = '\0';
    for (j = 1; j < DINK_SFX_SLOTS; j++) {
        if (j != id && g_s[j].loaded && strcmp(g_s[j].file, file) == 0) {
            g_s[id] = g_s[j];
            strncpy(g_s[id].file, file, sizeof(g_s[id].file) - 1);
            return 0;
        }
    }
    fp = open_wav(file);
    if (fp == NULL) {
        return -1;
    }
#ifdef _arch_dreamcast
    g_s[id].hnd = load_dc_fp(fp, &bytes);
    fclose(fp);
    if (g_s[id].hnd == 0) {
        printf("audio load fail %s\n", file);
        return -1;
    }
#else
    {
        long sz = 0;
        if (fseek(fp, 0, SEEK_END) == 0) {
            sz = ftell(fp);
        }
        fclose(fp);
        if (sz > 0) {
            bytes = (size_t)sz;
        }
    }
#endif
    g_s[id].loaded = 1;
    g_s[id].bytes = bytes;
    g_bytes += bytes;
    g_nslot++;
    return 0;
}

int audio_init(void)
{
    size_t i;

    if (g_on) {
        return 0;
    }
#ifdef _arch_dreamcast
    snd_init();
#endif
    memset(g_s, 0, sizeof(g_s));
    memset(g_v, 0, sizeof(g_v));
    g_bytes = 0;
    g_nslot = 0;
    g_age = 1;
    for (i = 0; i < sizeof(k_start) / sizeof(k_start[0]); i++) {
        if (load_slot(k_start[i].id, k_start[i].file) != 0) {
            printf("audio skip slot %d %s\n", k_start[i].id, k_start[i].file);
        }
    }
    g_on = 1;
    printf("audio sfx bytes=%zu slots=%d cap=%u\n", g_bytes, g_nslot,
           (unsigned)AICA_SFX_CAP);
#ifdef _arch_dreamcast
    if (g_bytes > AICA_SFX_CAP) {
        printf("audio warn sfx over cap\n");
    }
#endif
    return 0;
}

void audio_shutdown(void)
{
#ifdef _arch_dreamcast
    int i;
    if (g_on) {
        snd_sfx_stop_all();
        for (i = 1; i < DINK_SFX_SLOTS; i++) {
            if (g_s[i].loaded && g_s[i].hnd) {
                int j, share = 0;
                for (j = i + 1; j < DINK_SFX_SLOTS; j++) {
                    if (g_s[j].loaded && g_s[j].hnd == g_s[i].hnd) {
                        share = 1;
                        break;
                    }
                }
                if (!share) {
                    snd_sfx_unload(g_s[i].hnd);
                }
            }
        }
    }
#endif
    memset(g_s, 0, sizeof(g_s));
    memset(g_v, 0, sizeof(g_v));
    g_on = 0;
    g_bytes = 0;
    g_nslot = 0;
}

int audio_slot_loaded(int sound)
{
    if (sound < 1 || sound >= DINK_SFX_SLOTS) {
        return 0;
    }
    return g_s[sound].loaded;
}

size_t audio_sfx_bytes(void)
{
    return g_bytes;
}

int audio_slot_count(void)
{
    return g_nslot;
}

static int steal_voice(void)
{
    int i, best = 0;
    unsigned oldest = (unsigned)-1;

    for (i = 0; i < DINK_VOICES; i++) {
        if (!g_v[i].used) {
            return i;
        }
        if (g_v[i].age < oldest) {
            oldest = g_v[i].age;
            best = i;
        }
    }
#ifdef _arch_dreamcast
    snd_sfx_stop(g_v[best].ch);
#endif
    g_v[best].used = 0;
    return best;
}

int audio_playsound(int sound, int min, int plus, int sound3d, int repeat)
{
    int v, ch = 0;
    (void)min;
    (void)plus;
    (void)sound3d;
    if (!g_on || sound < 1 || sound >= DINK_SFX_SLOTS || !g_s[sound].loaded) {
        return 0;
    }
    v = steal_voice();
#ifdef _arch_dreamcast
    if (repeat) {
        ch = snd_sfx_play_ex(g_s[sound].hnd, 255, 128, 1);
    } else {
        ch = snd_sfx_play(g_s[sound].hnd, 255, 128);
    }
    if (ch < 0) {
        return 0;
    }
#else
    ch = v;
    (void)repeat;
#endif
    g_v[v].used = 1;
    g_v[v].ch = ch;
    g_v[v].age = g_age++;
    return ch + 1;
}
