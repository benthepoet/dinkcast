/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"

#include "fs.h"
#include "le.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <dc/sound/stream.h>
#endif

#define DINK_SFX_SLOTS 64
#define DINK_VOICES 16
#define AICA_SFX_CAP (512u * 1024u)
#define MUSIC_RING (256u * 1024u)
#define MUSIC_CHUNK (32u * 1024u)
/* Keep ~4 s of 22050 Hz s16 in the ring so a swap can skip GD-ROM. */
#define MUSIC_WATER (192u * 1024u)
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
    int owner;
    int loop;
    unsigned age;
} g_v[DINK_VOICES];

static int g_on;
static unsigned g_age;
static size_t g_bytes;
static int g_nslot;
static char g_midi[32];
#ifdef _arch_dreamcast
static uint8_t g_ring[MUSIC_RING] __attribute__((aligned(32)));
static int16_t g_bounce[8192] __attribute__((aligned(32)));
/* Disc chunk lives in BSS. 16 KiB ADPCM + 32 KiB PCM on the thread stack
 * panics (KOS ~32–64 KiB): emu.log Data address error after sfx bytes. */
static uint8_t g_mbuf[MUSIC_CHUNK / 2] __attribute__((aligned(32)));
static FILE *g_mfp;
static size_t g_moff, g_mend, g_mpos;
static int g_adpcm, g_hz, g_mch;
static int g_rpos, g_wpos, g_used;
static int16_t g_hist, g_step;
static snd_stream_hnd_t g_sh = SND_STREAM_INVALID;
static int g_stream_on;
static int g_disc_hold;
static char g_pending[32];
#endif

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
        printf("audio trunc nfr=%d keep=%lu\n", nfr, (unsigned long)keep);
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

static int name_eq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void midi_to_wav_name(const char *midi, char *out, size_t n)
{
    size_t L;
    const char *s = midi;

    if (out == NULL || n < 8) {
        return;
    }
    out[0] = '\0';
    if (s == NULL || s[0] == '\0') {
        return;
    }
    while (*s == '/' || *s == '\\') {
        s++;
    }
    {
        const char *sl = strrchr(s, '/');
        const char *bs = strrchr(s, '\\');

        if (bs != NULL && (sl == NULL || bs > sl)) {
            sl = bs;
        }
        if (sl != NULL) {
            s = sl + 1;
        }
    }
    strncpy(out, s, n - 1);
    out[n - 1] = '\0';
    L = strlen(out);
    if (L >= 4 && tolower((unsigned char)out[L - 4]) == '.' &&
        tolower((unsigned char)out[L - 3]) == 'm' &&
        tolower((unsigned char)out[L - 2]) == 'i' &&
        tolower((unsigned char)out[L - 1]) == 'd') {
        out[L - 3] = 'w';
        out[L - 2] = 'a';
        out[L - 1] = 'v';
    }
}

#ifdef _arch_dreamcast
static void music_stop_dc(void)
{
    if (g_sh != SND_STREAM_INVALID && g_stream_on) {
        snd_stream_stop(g_sh);
        g_stream_on = 0;
    }
    if (g_mfp != NULL) {
        fclose(g_mfp);
        g_mfp = NULL;
    }
    g_used = 0;
    g_rpos = 0;
    g_wpos = 0;
}

static int16_t ymz_step_d(uint8_t step, int16_t *history, int16_t *step_size)
{
    static const int step_table[8] = {230, 230, 230, 230, 307, 409, 512, 614};
    int sign = step & 8;
    int delta = step & 7;
    int diff = ((1 + (delta << 1)) * *step_size) >> 3;
    int newval = *history;
    int nstep = (step_table[delta] * *step_size) >> 8;

    if (diff < 0) {
        diff = 0;
    }
    if (diff > 32767) {
        diff = 32767;
    }
    if (sign > 0) {
        newval -= diff;
    } else {
        newval += diff;
    }
    if (nstep < 127) {
        nstep = 127;
    }
    if (nstep > 24576) {
        nstep = 24576;
    }
    if (newval < -32768) {
        newval = -32768;
    }
    if (newval > 32767) {
        newval = 32767;
    }
    *step_size = (int16_t)nstep;
    *history = (int16_t)newval;
    return *history;
}

static void music_ring_put_s16(int16_t s)
{
    if (g_used + 2 > (int)MUSIC_RING) {
        return;
    }
    g_ring[g_wpos] = (uint8_t)(s & 0xff);
    g_ring[(g_wpos + 1) % (int)MUSIC_RING] = (uint8_t)((s >> 8) & 0xff);
    g_wpos = (g_wpos + 2) % (int)MUSIC_RING;
    g_used += 2;
}

/* One disc chunk into the PCM ring. Never called from the stream callback. */
static void music_fill_chunk(void)
{
    size_t want, got, i;
    int space;

    if (g_mfp == NULL) {
        return;
    }
    space = (int)MUSIC_RING - g_used;
    if (space < (int)MUSIC_CHUNK / 2) {
        return;
    }
    want = (size_t)MUSIC_CHUNK / 2;
    if (g_mpos + want > g_mend) {
        want = g_mend - g_mpos;
    }
    if (want == 0) {
        g_mpos = g_moff;
        if (fseek(g_mfp, (long)g_moff, SEEK_SET) != 0) {
            return;
        }
        g_hist = 0;
        g_step = 127;
        want = (size_t)MUSIC_CHUNK / 2;
        if (g_mpos + want > g_mend) {
            want = g_mend - g_mpos;
        }
    }
    if (want > sizeof(g_mbuf)) {
        want = sizeof(g_mbuf);
    }
    got = fread(g_mbuf, 1, want, g_mfp);
    if (got == 0) {
        return;
    }
    g_mpos += got;
    if (g_adpcm) {
        for (i = 0; i < got; i++) {
            music_ring_put_s16(
                ymz_step_d((uint8_t)(g_mbuf[i] & 0x0f), &g_hist, &g_step));
            music_ring_put_s16(ymz_step_d((uint8_t)((g_mbuf[i] >> 4) & 0x0f),
                                          &g_hist, &g_step));
        }
    } else {
        for (i = 0; i + 1 < got; i += 2) {
            music_ring_put_s16(
                (int16_t)(g_mbuf[i] | (g_mbuf[i + 1] << 8)));
        }
    }
}

static void *music_cb(snd_stream_hnd_t hnd, int smp_req, int *smp_recv)
{
    int need, i, ns;

    (void)hnd;
    if (smp_recv == NULL) {
        return NULL;
    }
    /* This KOS snd_stream_fill() passes *bytes* as smp_req / *smp_recv
     * (needed_bytes / got_bytes) even though stream.h says "samples".
     * Treating req as samples (*2) drained the ring twice as fast as
     * AICA played — title theme sounded rushed and wrapped in ~6 s
     * (256 KiB PCM ring / 22050 / 2). */
    need = smp_req;
    if (need < 0) {
        need = 0;
    }
    if (need > (int)sizeof(g_bounce)) {
        need = (int)sizeof(g_bounce);
    }
    need &= ~3;
    if (g_used < need) {
        /* Underrun: silence. Do not fread here (GD-ROM hitch). */
        memset(g_bounce, 0, (size_t)need);
        *smp_recv = need;
        return g_bounce;
    }
    ns = need / 2;
    for (i = 0; i < ns; i++) {
        int p = (g_rpos + i * 2) % (int)MUSIC_RING;
        g_bounce[i] =
            (int16_t)(g_ring[p] | ((int16_t)g_ring[(p + 1) % (int)MUSIC_RING] << 8));
    }
    g_rpos = (g_rpos + need) % (int)MUSIC_RING;
    g_used -= need;
    *smp_recv = need;
    return g_bounce;
}
#endif

int audio_init(void)
{
    size_t i;

    if (g_on) {
        return 0;
    }
#ifdef _arch_dreamcast
    /* stream_init calls snd_init; do this *before* sfx load. */
    if (snd_stream_init() != 0) {
        printf("audio stream init fail\n");
        snd_init();
    } else {
        g_sh = snd_stream_alloc(music_cb, 16 * 1024);
        if (g_sh == SND_STREAM_INVALID) {
            printf("audio stream alloc fail\n");
        } else {
            dink_cd_set_pump(audio_music_pump);
        }
    }
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
        dink_cd_set_pump(NULL);
        g_disc_hold = 0;
        g_pending[0] = '\0';
        music_stop_dc();
        if (g_sh != SND_STREAM_INVALID) {
            snd_stream_destroy(g_sh);
            g_sh = SND_STREAM_INVALID;
        }
        snd_stream_shutdown();
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
    g_v[best].owner = 0;
    g_v[best].loop = 0;
    return best;
}

int audio_playsound(int sound, int min, int plus, int sound3d, int repeat)
{
    int v, ch = 0;
#ifdef _arch_dreamcast
    sfx_play_data_t play;
#endif
    (void)plus;
    (void)sound3d;
    if (!g_on || sound < 1 || sound >= DINK_SFX_SLOTS || !g_s[sound].loaded) {
        return 0;
    }
    v = steal_voice();
#ifdef _arch_dreamcast
    /* This KOS snd_sfx_play_ex takes sfx_play_data_t, not (hnd,vol,pan,loop). */
    memset(&play, 0, sizeof(play));
    play.chn = -1;
    play.idx = g_s[sound].hnd;
    play.vol = 255;
    play.pan = 128;
    play.loop = repeat ? 1 : 0;
    if (min > 0) {
        play.freq = min;
    }
    ch = snd_sfx_play_ex(&play);
    if (ch < 0) {
        return 0;
    }
#else
    ch = v;
    (void)min;
    (void)repeat;
#endif
    g_v[v].used = 1;
    g_v[v].ch = ch;
    g_v[v].owner = sound3d;
    g_v[v].loop = repeat ? 1 : 0;
    g_v[v].age = g_age++;
    return ch + 1;
}

int audio_warp_sound(int editor_sound)
{
    /* FreeDink special_block: default OPEN.WAV 7 @ 12000. */
    if (editor_sound == 0) {
        return audio_playsound(7, 12000, 0, 0, 0);
    }
    return audio_playsound(editor_sound, 22050, 0, 0, 0);
}

void audio_halt_owner(int sprite)
{
    int i;

    if (sprite < 1) {
        return;
    }
    for (i = 0; i < DINK_VOICES; i++) {
        if (!g_v[i].used || g_v[i].owner != sprite) {
            continue;
        }
#ifdef _arch_dreamcast
        snd_sfx_stop(g_v[i].ch);
#endif
        g_v[i].used = 0;
        g_v[i].owner = 0;
        g_v[i].loop = 0;
    }
}

void audio_halt_loops(void)
{
    int i;

    for (i = 0; i < DINK_VOICES; i++) {
        if (!g_v[i].used || !g_v[i].loop) {
            continue;
        }
#ifdef _arch_dreamcast
        snd_sfx_stop(g_v[i].ch);
#endif
        g_v[i].used = 0;
        g_v[i].owner = 0;
        g_v[i].loop = 0;
    }
}

int audio_owner_looping(int sprite)
{
    int i;

    if (sprite < 1) {
        return 0;
    }
    for (i = 0; i < DINK_VOICES; i++) {
        if (g_v[i].used && g_v[i].loop && g_v[i].owner == sprite) {
            return 1;
        }
    }
    return 0;
}

int audio_music_play(const char *midi_name)
{
    char wav[32];

    if (!g_on || midi_name == NULL || midi_name[0] == '\0') {
        return 0;
    }
    midi_to_wav_name(midi_name, wav, sizeof(wav));
    if (name_eq(g_midi, midi_name) && audio_music_playing()) {
#ifdef _arch_dreamcast
        g_pending[0] = '\0';
#endif
        return 1;
    }
#ifdef _arch_dreamcast
    if (g_disc_hold) {
        strncpy(g_pending, midi_name, sizeof(g_pending) - 1);
        g_pending[sizeof(g_pending) - 1] = '\0';
        return 1;
    }
    {
        FILE *fp;
        uint8_t rid[12], chunk[8], fmt[16];
        uint32_t dsz = 0, rate = 22050, sz;
        uint16_t af = 1, ch = 1, bits = 16;
        size_t doff = 0;
        int nfill, have_fmt = 0;
        long pos;

        music_stop_dc();
        fp = open_wav(wav);
        if (fp == NULL) {
            printf("music miss %s\n", wav);
            g_midi[0] = '\0';
            return 0;
        }
        if (fread(rid, 1, 12, fp) != 12 || memcmp(rid, "RIFF", 4) != 0) {
            fclose(fp);
            printf("music bad wav %s\n", wav);
            return 0;
        }
        pos = 12;
        while (fread(chunk, 1, 8, fp) == 8) {
            sz = (uint32_t)chunk[4] | ((uint32_t)chunk[5] << 8) |
                 ((uint32_t)chunk[6] << 16) | ((uint32_t)chunk[7] << 24);
            pos += 8;
            if (memcmp(chunk, "fmt ", 4) == 0) {
                if (sz < 16 || fread(fmt, 1, 16, fp) != 16) {
                    fclose(fp);
                    return 0;
                }
                if (sz > 16 && fseek(fp, (long)(sz - 16), SEEK_CUR) != 0) {
                    fclose(fp);
                    return 0;
                }
                af = (uint16_t)(fmt[0] | (fmt[1] << 8));
                ch = (uint16_t)(fmt[2] | (fmt[3] << 8));
                rate = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) |
                       ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
                bits = (uint16_t)(fmt[14] | (fmt[15] << 8));
                have_fmt = 1;
            } else if (memcmp(chunk, "data", 4) == 0) {
                doff = (size_t)pos;
                dsz = sz;
                break;
            } else if (fseek(fp, (long)sz, SEEK_CUR) != 0) {
                fclose(fp);
                return 0;
            }
            pos += (long)sz + (long)(sz & 1u);
            if (sz & 1u) {
                (void)fseek(fp, 1, SEEK_CUR);
            }
        }
        if (!have_fmt || doff == 0) {
            fclose(fp);
            printf("music no data %s\n", wav);
            return 0;
        }
        g_mfp = fp;
        g_moff = doff;
        g_mend = doff + dsz;
        g_mpos = doff;
        g_adpcm = (af == WAVE_YAMAHA_ADPCM || bits == 4);
        g_hz = rate > 0 ? (int)rate : 22050;
        g_mch = ch > 0 ? (int)ch : 1;
        g_hist = 0;
        g_step = 127;
        g_used = 0;
        g_rpos = 0;
        g_wpos = 0;
        if (fseek(g_mfp, (long)g_moff, SEEK_SET) != 0) {
            music_stop_dc();
            return 0;
        }
        nfill = 0;
        while (g_used < (int)MUSIC_WATER && nfill < 8) {
            int before = g_used;

            music_fill_chunk();
            if (g_used == before) {
                break;
            }
            nfill++;
        }
        if (g_sh != SND_STREAM_INVALID && g_used > 0) {
            snd_stream_start(g_sh, (uint32_t)g_hz, g_mch > 1 ? 1 : 0);
            snd_stream_volume(g_sh, 255);
            g_stream_on = 1;
        }
        printf("music play %s wav=%s adpcm=%d hz=%d used=%d bytes=%lu\n",
               midi_name, wav, g_adpcm, g_hz, g_used,
               (unsigned long)(g_mend > g_moff ? g_mend - g_moff : 0));
    }
#else
    (void)wav;
#endif
    strncpy(g_midi, midi_name, sizeof(g_midi) - 1);
    g_midi[sizeof(g_midi) - 1] = '\0';
    return 1;
}

int audio_music_map(int midi_id)
{
    char name[20];

    if (midi_id == 0 || midi_id == -1) {
        return audio_music_playing();
    }
    if (midi_id > 1000) {
        midi_id -= 1000;
    }
    snprintf(name, sizeof(name), "%d.mid", midi_id);
    return audio_music_play(name);
}

void audio_music_stop(void)
{
#ifdef _arch_dreamcast
    music_stop_dc();
#endif
    g_midi[0] = '\0';
}

void audio_music_pump(void)
{
#ifdef _arch_dreamcast
    if (g_sh != SND_STREAM_INVALID && g_stream_on) {
        (void)snd_stream_poll(g_sh);
    }
#endif
}

void audio_music_disc_hold(int on)
{
#ifdef _arch_dreamcast
    char pending[32];

    if (on) {
        g_disc_hold++;
        return;
    }
    if (g_disc_hold > 0) {
        g_disc_hold--;
    }
    if (g_disc_hold != 0 || g_pending[0] == '\0') {
        return;
    }
    memcpy(pending, g_pending, sizeof(pending));
    g_pending[0] = '\0';
    (void)audio_music_play(pending);
#else
    (void)on;
#endif
}

void audio_music_poll(void)
{
#ifdef _arch_dreamcast
    int nfill = 0;

    /* Fill on the 60 Hz thread *before* snd_stream_poll. Cap fills so a
     * hitch cannot stall a frame on GD-ROM. Skip fread while a screen
     * swap owns the drive (AICA loops its 16 KiB buffer if we starve). */
    if (!g_disc_hold) {
        while (g_mfp != NULL && g_used < (int)MUSIC_WATER && nfill < 8) {
            int before = g_used;

            music_fill_chunk();
            if (g_used == before) {
                break;
            }
            nfill++;
        }
    }
    audio_music_pump();
#endif
}

int audio_music_playing(void)
{
    if (g_midi[0] == '\0') {
        return 0;
    }
#ifdef _arch_dreamcast
    return g_stream_on;
#else
    return 1;
#endif
}
