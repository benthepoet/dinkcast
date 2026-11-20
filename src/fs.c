/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"

#include "mem.h"
#include "residency.h"

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <time.h>

#ifdef _arch_dreamcast
#include <arch/timer.h>
#include <kos.h>
#include <kos/mutex.h>
#endif

/* RAM-only decode under 2 ms is counted, not printed. */
#define DINK_IO_FRAME_QUIET_US 2000u
/* Lines are queued, not printed, so SCIF time never lands inside a timed
 * window. dink_io_flush prints them once per tick (outside any window). */
#define DINK_IO_RING 48
#define DINK_IO_LINE 128

static int g_io_play;
static char g_io_ctx[64];
static struct DinkIoStats g_io;
static char g_io_ring[DINK_IO_RING][DINK_IO_LINE];
static int g_io_nring;
static unsigned g_io_dropped;

unsigned dink_io_now_us(void)
{
#ifdef _arch_dreamcast
    return (unsigned)timer_us_gettime64();
#else
    struct timespec t;

    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        return 0;
    }
    return (unsigned)((unsigned long long)t.tv_sec * 1000000ull +
                      (unsigned long long)t.tv_nsec / 1000ull);
#endif
}

void dink_io_set_play(int on)
{
    g_io_play = on ? 1 : 0;
}

int dink_io_play(void)
{
    return g_io_play;
}

void dink_io_ctx(const char *ctx)
{
    if (ctx == NULL) {
        g_io_ctx[0] = '\0';
        return;
    }
    snprintf(g_io_ctx, sizeof(g_io_ctx), "%s", ctx);
}

void dink_io_ctx_save(char *dst, size_t n)
{
    if (dst != NULL && n > 0) {
        snprintf(dst, n, "%s", g_io_ctx);
    }
}

static void io_queue(const char *line)
{
    if (g_io_nring >= DINK_IO_RING) {
        g_io_dropped++;
        return;
    }
    snprintf(g_io_ring[g_io_nring], DINK_IO_LINE, "%s", line);
    g_io_nring++;
}

int dink_io_pending(void)
{
    return g_io_nring;
}

static unsigned g_log_pending; /* flush us since the last tick */

void dink_io_flush(void)
{
    int i, n = g_io_nring;
    unsigned t0;

    if (n == 0 && g_io_dropped == 0) {
        return;
    }
    t0 = dink_io_now_us();
    for (i = 0; i < g_io_nring; i++) {
        printf("ioplay %s\n", g_io_ring[i]);
    }
    g_io_nring = 0;
    if (g_io_dropped != 0) {
        printf("ioplay dropped=%u (ring %d)\n", g_io_dropped, DINK_IO_RING);
        g_io_dropped = 0;
    }
    {
        unsigned d = dink_io_now_us() - t0;

        g_log_pending += d;
        g_io.log_us += d;
        g_io.logged += (unsigned)n;
    }
}

void dink_io_note(const char *op, const char *what, size_t bytes, unsigned us)
{
    char line[DINK_IO_LINE];

    /* Stream refills: tallied in either phase, never printed per chunk. */
    if (op != NULL && strcmp(op, "music") == 0) {
        g_io.music_ops++;
        g_io.music_bytes += (unsigned)bytes;
        g_io.music_us += us;
        return;
    }
    /* One audio_music_poll (up to 8 chunks + seeks): worst poll, not chunk. */
    if (op != NULL && strcmp(op, "music-poll") == 0) {
        if (us > g_io.music_max) {
            g_io.music_max = us;
        }
        return;
    }
    if (!g_io_play) {
        return;
    }
    g_io.ops++;
    g_io.bytes += (unsigned)bytes;
    g_io.us += us;
    if (us > g_io.max_us) {
        g_io.max_us = us;
    }
    snprintf(line, sizeof(line), "disc op=%s bytes=%u us=%u ctx=%s",
             op != NULL ? op : "?", (unsigned)bytes, us,
             (what != NULL && what[0] != '\0')
                 ? what
                 : (g_io_ctx[0] ? g_io_ctx : "-"));
    io_queue(line);
}

void dink_io_note_frame(const char *who, int seq, int frame, unsigned us,
                        unsigned disc_ops)
{
    char line[DINK_IO_LINE];

    if (!g_io_play) {
        return;
    }
    g_io.frames++;
    g_io.frame_us += us;
    if (us > g_io.frame_max) {
        g_io.frame_max = us;
    }
    /* Dink's walk decodes every few frames; print only real stalls. A
     * failed load (us flagged by caller via seq < 0) always prints. */
    if (seq >= 0 && disc_ops == 0 && us < DINK_IO_FRAME_QUIET_US) {
        return;
    }
    snprintf(line, sizeof(line), "frame who=%s seq=%d fr=%d us=%u disc_ops=%u%s",
             who != NULL ? who : "?", seq < 0 ? -seq : seq, frame, us,
             disc_ops, seq < 0 ? " fail" : "");
    io_queue(line);
}

void dink_io_note_gpu_wait(unsigned us)
{
    char line[DINK_IO_LINE];

    if (!g_io_play) {
        return;
    }
    g_io.gpu_waits++;
    g_io.gpu_us += us;
    snprintf(line, sizeof(line), "gpu_wait us=%u ctx=%s", us,
             g_io_ctx[0] ? g_io_ctx : "-");
    io_queue(line);
}

void dink_io_stats(struct DinkIoStats *out)
{
    if (out != NULL) {
        *out = g_io;
    }
}

static unsigned g_tick_last;
static int g_tick_have;

void dink_io_note_vram(unsigned free_b, unsigned screen_tex, unsigned dink_tex)
{
    if (!dink_io_play()) {
        return;
    }
    if (g_io.vram_samples == 0 || free_b < g_io.vram_min) {
        g_io.vram_min = free_b;
        g_io.min_screen_tex = screen_tex;
        g_io.min_dink_tex = dink_tex;
    }
    if (g_io.vram_samples == 0) {
        g_io.vram_enter = free_b;
    }
    if (screen_tex > g_io.peak_screen_tex) {
        g_io.peak_screen_tex = screen_tex;
    }
    if (dink_tex > g_io.peak_dink_tex) {
        g_io.peak_dink_tex = dink_tex;
    }
    g_io.vram_samples++;
}

void dink_io_tick(unsigned now_us)
{
    if (!dink_io_play()) {
        g_tick_have = 0;
        return;
    }
    if (g_tick_have) {
        unsigned d = now_us - g_tick_last; /* wraps cleanly */

        /* Our own serial logging is not game work. Other printfs inside
         * the tick (evictions, refusals) are still counted. */
        d = d > g_log_pending ? d - g_log_pending : 0u;

        g_io.ticks++;
        if (d > 20000u) {
            g_io.ticks_over20++;
        }
        if (d > 34000u) {
            g_io.ticks_over34++;
        }
        if (d > g_io.tick_max_us) {
            g_io.tick_max_us = d;
        }
    }
    g_tick_last = now_us;
    g_tick_have = 1;
    g_log_pending = 0;
}

void dink_io_note_vram_upload(unsigned free_b)
{
    if (!dink_io_play()) {
        return;
    }
    if (g_io.upload_samples == 0 || free_b < g_io.min_upload) {
        g_io.min_upload = free_b;
    }
    g_io.upload_samples++;
}

void dink_io_summary(int map)
{
    dink_io_flush();
    /* map 0 = boot/title tally (menu music); reset without a line. */
    if (map > 0) {
        printf("ioplay summary map=%d disc_ops=%u bytes=%u us=%u max_us=%u "
               "frames=%u frame_us=%u frame_max_us=%u gpu_waits=%u "
               "gpu_us=%u music_ops=%u music_bytes=%u music_us=%u "
               "music_max_poll_us=%u\n",
               map, g_io.ops, g_io.bytes, g_io.us, g_io.max_us, g_io.frames,
               g_io.frame_us, g_io.frame_max, g_io.gpu_waits, g_io.gpu_us,
               g_io.music_ops, g_io.music_bytes, g_io.music_us,
               g_io.music_max);
        if (g_io.vram_samples > 0) {
            unsigned lo = g_io.vram_min;

            if (g_io.upload_samples > 0 && g_io.min_upload < lo) {
                lo = g_io.min_upload;
            }
            /* Split is partial: tile atlas, HUD/status, overlays, font and
             * allocator overhead are not in screen/dink. */
            printf("ioplay vram map=%d samples=%u enter=%u min=%u drop=%u "
                   "min_tick=%u min_upload=%u uploads=%u "
                   "tick_min_screen_tex=%u tick_min_dink_tex=%u "
                   "peak_screen_tex=%u peak_dink_tex=%u\n",
                   map, g_io.vram_samples, g_io.vram_enter, lo,
                   g_io.vram_enter > lo ? g_io.vram_enter - lo : 0u,
                   g_io.vram_min, g_io.min_upload, g_io.upload_samples,
                   g_io.min_screen_tex, g_io.min_dink_tex,
                   g_io.peak_screen_tex, g_io.peak_dink_tex);
        }
        if (g_io.ticks > 0) {
            printf("ioplay ticks map=%d n=%u over20ms=%u over34ms=%u "
                   "max_us=%u log_us=%u logged=%u\n",
                   map, g_io.ticks, g_io.ticks_over20, g_io.ticks_over34,
                   g_io.tick_max_us, g_io.log_us, g_io.logged);
        }
    }
    memset(&g_io, 0, sizeof(g_io));
    g_tick_have = 0;
    g_log_pending = 0;
}

#ifndef DINK_DATA_DEFAULT
#define DINK_DATA_DEFAULT ""
#endif

static char g_root[DINK_FS_PATH_MAX];
static char g_distill[DINK_FS_PATH_MAX];
static const char *g_pc = "/pc/dink";
static const char *g_cd = "/cd/dink";
static const char *g_fallback = DINK_DATA_DEFAULT;

static int dink_fs_try_root(const char *path);

static void slashes(char *s)
{
    for (; *s; s++) {
        if (*s == '\\') {
            *s = '/';
        }
    }
}

int dink_fs_exists_dir(const char *path)
{
    struct stat st;
    DIR *d;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 1;
    }
    /* KOS ISO9660 often fails S_ISDIR; opendir is the real check. */
    d = opendir(path);
    if (d != NULL) {
        closedir(d);
        return 1;
    }
    return 0;
}

int dink_fs_join(char *dst, size_t dstsz, const char *root, const char *rel)
{
    char tmp[DINK_FS_PATH_MAX];
    size_t n;

    if (dst == NULL || dstsz < 2 || root == NULL) {
        return -1;
    }
    if (rel == NULL) {
        rel = "";
    }
    if (strstr(rel, "..") != NULL) {
        /* Reject ".." even inside a name-adjacent scan; join is not a sandbox. */
        const char *q = rel;
        while (*q != '\0') {
            if ((q[0] == '.' && q[1] == '.') &&
                (q[2] == '\0' || q[2] == '/' || q[2] == '\\') &&
                (q == rel || q[-1] == '/' || q[-1] == '\\')) {
                return -1;
            }
            q++;
        }
    }
    n = (size_t)snprintf(tmp, sizeof(tmp), "%s/%s", root, rel);
    if (n >= sizeof(tmp)) {
        return -1;
    }
    slashes(tmp);
    /* collapse duplicate slashes except a leading pair */
    {
        char *w = tmp;
        char *r = tmp;
        char prev = 0;
        if (r[0] == '/' && r[1] == '/') {
            *w++ = *r++;
        }
        for (; *r; r++) {
            if (*r == '/' && prev == '/') {
                continue;
            }
            *w++ = *r;
            prev = *r;
        }
        *w = '\0';
    }
    /* trim trailing slash except root "/" */
    n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') {
        tmp[--n] = '\0';
    }
    if (n >= dstsz) {
        return -1;
    }
    memcpy(dst, tmp, n + 1);
    return 0;
}

void dink_fs_set_probe_roots(const char *pc, const char *cd, const char *fallback)
{
    g_pc = (pc != NULL) ? pc : "";
    g_cd = (cd != NULL) ? cd : "";
    g_fallback = (fallback != NULL) ? fallback : "";
    g_root[0] = '\0';
}

int dink_fs_init(void)
{
    const char *env;
    char parent[DINK_FS_PATH_MAX];
    char *slash;

    g_root[0] = '\0';
    g_distill[0] = '\0';
    {
        const char *dist = getenv("DINK_DISTILL");

        if (dist != NULL && dist[0] != '\0' && dink_fs_exists_dir(dist)) {
            snprintf(g_distill, sizeof(g_distill), "%s", dist);
        }
    }
    dink_blob_clear();
    if (dink_fs_try_root(g_pc) == 0 || dink_fs_try_root("/pc/DINK") == 0) {
        return 0;
    }
    if (dink_fs_try_root(g_cd) == 0 || dink_fs_try_root("/cd/DINK") == 0) {
        return 0;
    }
    /* ISO9660: files may sit at /cd or /cd/DINK, not /cd/dink. */
    if (dink_fs_try_root("/cd") == 0) {
        return 0;
    }
    /* Older CDIs staged as -d build/iso → /cd/iso/dink. */
    if (dink_fs_try_root("/cd/iso") == 0 ||
        dink_fs_try_root("/cd/iso/dink") == 0 ||
        dink_fs_try_root("/cd/iso/DINK") == 0) {
        return 0;
    }
    snprintf(parent, sizeof(parent), "%s", g_cd);
    slash = strrchr(parent, '/');
    if (slash != NULL && slash != parent) {
        *slash = '\0';
        if (dink_fs_try_root(parent) == 0) {
            return 0;
        }
    }
    env = getenv("DINK_DATA");
    if (env != NULL && dink_fs_try_root(env) == 0) {
        return 0;
    }
    if (dink_fs_try_root(g_fallback) == 0) {
        return 0;
    }
    if (dink_fs_try_root("/dink") == 0 || dink_fs_try_root("/DINK") == 0) {
        return 0;
    }
    return -1;
}

const char *dink_fs_root(void)
{
    return g_root;
}

/* ISO9660-ish 8.3: first 8 of stem + '.' + first 3 of ext, uppercased. */
static void to_83(char *dst, size_t dstsz, const char *name)
{
    char stem[9];
    char ext[4];
    const char *dot = strrchr(name, '.');
    size_t i, n;

    stem[0] = ext[0] = '\0';
    if (dot == NULL || dot == name) {
        n = strlen(name);
        if (n > 8) {
            n = 8;
        }
        for (i = 0; i < n; i++) {
            stem[i] = (char)toupper((unsigned char)name[i]);
        }
        stem[n] = '\0';
        snprintf(dst, dstsz, "%s", stem);
        return;
    }
    n = (size_t)(dot - name);
    if (n > 8) {
        n = 8;
    }
    for (i = 0; i < n; i++) {
        stem[i] = (char)toupper((unsigned char)name[i]);
    }
    stem[n] = '\0';
    n = strlen(dot + 1);
    if (n > 3) {
        n = 3;
    }
    for (i = 0; i < n; i++) {
        ext[i] = (char)toupper((unsigned char)dot[1 + i]);
    }
    ext[n] = '\0';
    snprintf(dst, dstsz, "%s.%s", stem, ext);
}

static int names_equal_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Resolve one path component inside dir into out (full path). */
static int resolve_comp(const char *dir, const char *comp, char *out, size_t outsz)
{
    DIR *d;
    struct dirent *de;
    char want83[16];
    char cand[DINK_FS_PATH_MAX];
    int found = 0;

    if (dink_fs_join(cand, sizeof(cand), dir, comp) == 0) {
        struct stat st;
        if (stat(cand, &st) == 0) {
            if (strlen(cand) >= outsz) {
                return -1;
            }
            memcpy(out, cand, strlen(cand) + 1);
            return 0;
        }
    }

    to_83(want83, sizeof(want83), comp);
    d = opendir(dir);
    if (d == NULL) {
        return -1;
    }
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if (names_equal_ci(de->d_name, comp) || names_equal_ci(de->d_name, want83)) {
            if (dink_fs_join(out, outsz, dir, de->d_name) != 0) {
                closedir(d);
                return -1;
            }
            found = 1;
            break;
        }
        {
            char ent83[16];
            to_83(ent83, sizeof(ent83), de->d_name);
            if (names_equal_ci(ent83, want83) || names_equal_ci(ent83, comp)) {
                if (dink_fs_join(out, outsz, dir, de->d_name) != 0) {
                    closedir(d);
                    return -1;
                }
                found = 1;
                break;
            }
        }
    }
    closedir(d);
    return found ? 0 : -1;
}

#ifdef _arch_dreamcast
static mutex_t g_cd_mu;
static int g_cd_mu_ok;

static void cd_lock(void)
{
    if (!g_cd_mu_ok) {
        mutex_init(&g_cd_mu, MUTEX_TYPE_NORMAL);
        g_cd_mu_ok = 1;
    }
    mutex_lock(&g_cd_mu);
}

static void cd_unlock(void)
{
    mutex_unlock(&g_cd_mu);
}
#else
static void cd_lock(void) {}
static void cd_unlock(void) {}
#endif

static FILE *fopen_under(const char *root, const char *rel, const char *mode)
{
    char cur[DINK_FS_PATH_MAX];
    char next[DINK_FS_PATH_MAX];
    char relnorm[DINK_FS_PATH_MAX];
    char *save = NULL;
    char *tok;

    if (root == NULL || root[0] == '\0' || rel == NULL || mode == NULL) {
        return NULL;
    }
    snprintf(relnorm, sizeof(relnorm), "%s", rel);
    slashes(relnorm);
    snprintf(cur, sizeof(cur), "%s", root);

    tok = strtok_r(relnorm, "/", &save);
    if (tok == NULL) {
        return NULL;
    }
    do {
        if (strcmp(tok, "..") == 0 || strcmp(tok, ".") == 0) {
            return NULL;
        }
        if (resolve_comp(cur, tok, next, sizeof(next)) != 0) {
            return NULL;
        }
        memcpy(cur, next, strlen(next) + 1);
        tok = strtok_r(NULL, "/", &save);
    } while (tok != NULL);

    return fopen(cur, mode);
}

FILE *dink_fopen(const char *rel, const char *mode)
{
    FILE *fp;
    unsigned t0;

    if (g_root[0] == '\0' || rel == NULL || mode == NULL) {
        return NULL;
    }
    t0 = dink_io_now_us();
    cd_lock();
    if (g_distill[0] != '\0') {
        fp = fopen_under(g_distill, rel, mode);
        if (fp != NULL) {
            cd_unlock();
            dink_io_note("open", rel, 0, dink_io_now_us() - t0);
            return fp;
        }
    }
    fp = fopen_under(g_root, rel, mode);
    cd_unlock();
    dink_io_note(fp != NULL ? "open" : "open-miss", rel, 0,
                 dink_io_now_us() - t0);
    /* Seek/open can stall past the 16 KiB AICA loop. Pump after. */
    dink_cd_yield();
    return fp;
}

#define DINK_CD_CHUNK 8192u
#define DINK_SLURP_MAX (4u * 1024u * 1024u)

static void (*g_cd_pump)(void);

void dink_cd_set_pump(void (*fn)(void))
{
    g_cd_pump = fn;
}

static void cd_pump(void)
{
    if (g_cd_pump != NULL) {
        g_cd_pump();
    }
}

void dink_cd_yield(void)
{
    /* Do not thd_pass mid-file (KOS #1492). Pump AICA from the PCM ring. */
    cd_pump();
}

void dink_cd_settle(void)
{
    dink_cd_yield();
#ifdef _arch_dreamcast
    /* Do not vid_waitvbl here: vblank may not fire during load. */
    thd_sleep(20);
#endif
}

int dink_fread_n(FILE *fp, uint8_t *dst, size_t n)
{
    size_t got = 0, chunk, nrd;
    unsigned t0;

    if (fp == NULL || dst == NULL) {
        return -1;
    }
    t0 = dink_io_now_us();
    cd_lock();
    while (got < n) {
        chunk = n - got;
        if (chunk > DINK_CD_CHUNK) {
            chunk = DINK_CD_CHUNK;
        }
        nrd = fread(dst + got, 1, chunk, fp);
        if (nrd == 0) {
            cd_unlock();
            dink_io_note("read-fail", NULL, got, dink_io_now_us() - t0);
            return -1;
        }
        got += nrd;
        cd_pump();
    }
    cd_unlock();
    dink_io_note("read", NULL, got, dink_io_now_us() - t0);
    return 0;
}

int dink_pread(FILE *fp, long off, uint8_t *dst, size_t n)
{
    size_t got = 0, chunk, nrd;
    unsigned t0;

    if (fp == NULL || dst == NULL) {
        return -1;
    }
    t0 = dink_io_now_us();
    cd_lock();
    if (fseek(fp, off, SEEK_SET) != 0) {
        cd_unlock();
        dink_io_note("seek-fail", NULL, 0, dink_io_now_us() - t0);
        return -1;
    }
    cd_pump();
    while (got < n) {
        chunk = n - got;
        if (chunk > DINK_CD_CHUNK) {
            chunk = DINK_CD_CHUNK;
        }
        nrd = fread(dst + got, 1, chunk, fp);
        if (nrd == 0) {
            cd_unlock();
            dink_io_note("pread-fail", NULL, got, dink_io_now_us() - t0);
            return -1;
        }
        got += nrd;
        cd_pump();
    }
    cd_unlock();
    dink_io_note("pread", NULL, got, dink_io_now_us() - t0);
    return 0;
}

int dink_fp_size(FILE *fp, size_t *n)
{
    struct stat st;
    int fd;

    if (fp == NULL || n == NULL) {
        return -1;
    }
    fd = fileno(fp);
    if (fd < 0 || fstat(fd, &st) != 0 || st.st_size <= 0) {
        return -1;
    }
    if ((size_t)st.st_size > DINK_SLURP_MAX) {
        return -1;
    }
    *n = (size_t)st.st_size;
    return 0;
}

int dink_fread_all(FILE *fp, uint8_t **out, size_t *n)
{
    uint8_t *p;
    size_t cap, got, nrd, known;
    unsigned t0;

    if (fp == NULL || out == NULL || n == NULL) {
        return -1;
    }
    t0 = dink_io_now_us();
    /* fstat, not SEEK_END. Doubling 32 KiB → 1 MiB for a 594 KB pack
     * sbrk-failed on 409 while Prev still held 408. */
    if (dink_fp_size(fp, &known) == 0) {
        p = (uint8_t *)malloc(known);
        if (p == NULL) {
            return -1;
        }
        got = 0;
        cd_lock();
        while (got < known) {
            size_t chunk = known - got;

            if (chunk > DINK_CD_CHUNK) {
                chunk = DINK_CD_CHUNK;
            }
            nrd = fread(p + got, 1, chunk, fp);
            if (nrd == 0) {
                break;
            }
            got += nrd;
            cd_pump();
        }
        cd_unlock();
        dink_io_note(got == 0 ? "slurp-fail" : "slurp", NULL, got,
                     dink_io_now_us() - t0);
        if (got == 0) {
            free(p);
            return -1;
        }
        *out = p;
        *n = got;
        return 0;
    }
    cap = 32u * 1024u;
    p = (uint8_t *)malloc(cap);
    if (p == NULL) {
        return -1;
    }
    got = 0;
    cd_lock();
    for (;;) {
        if (got + DINK_CD_CHUNK > cap) {
            uint8_t *np;
            size_t ncap = cap * 2u;

            if (ncap > DINK_SLURP_MAX || ncap < cap) {
                cd_unlock();
                free(p);
                return -1;
            }
            np = (uint8_t *)realloc(p, ncap);
            if (np == NULL) {
                cd_unlock();
                free(p);
                return -1;
            }
            p = np;
            cap = ncap;
        }
        nrd = fread(p + got, 1, DINK_CD_CHUNK, fp);
        if (nrd == 0) {
            break;
        }
        got += nrd;
        cd_pump();
    }
    cd_unlock();
    dink_io_note("slurp", NULL, got, dink_io_now_us() - t0);
    *out = p;
    *n = got;
    return 0;
}

#define DINK_BLOB_INIT 64

/* Session cache. Never free a live slot to insert another — ff/hard borrow
 * these pointers until dink_blob_clear. Grow when full. */
static struct {
    char rel[DINK_FS_PATH_MAX];
    uint8_t *data;
    size_t n;
    int cls;
    int age_out;
} *g_blob;
static int g_nblob;
static int g_disc_opens;

/* Prev dir.ff must drop for this Screen's tilesheet too. ts41.bmp is
 * 161 KB; village 408 already sits on the 4.5 MB cap. Do not treat
 * tiles/ as a make_room victim (drop_one_prev stays dir.ff only). */
static int blob_needs_room(const char *key)
{
    size_t kn;

    if (key == NULL || key[0] == '\0') {
        return 0;
    }
    kn = strlen(key);
    if (kn >= 6 && strcmp(key + kn - 6, "dir.ff") == 0) {
        return 1;
    }
    return strncmp(key, "tiles/", 6) == 0;
}

static int blob_slot_empty(void)
{
    int i, ncap;
    void *nb;

    for (i = 0; i < g_nblob; i++) {
        if (g_blob[i].rel[0] == '\0') {
            return i;
        }
    }
    ncap = g_nblob == 0 ? DINK_BLOB_INIT : g_nblob * 2;
    if (ncap < DINK_BLOB_INIT || ncap < g_nblob) {
        return -1;
    }
    nb = realloc(g_blob, (size_t)ncap * sizeof(*g_blob));
    if (nb == NULL) {
        return -1;
    }
    g_blob = nb;
    memset(g_blob + g_nblob, 0, (size_t)(ncap - g_nblob) * sizeof(*g_blob));
    i = g_nblob;
    g_nblob = ncap;
    return i;
}

static void rel_key(char *dst, size_t dstsz, const char *rel)
{
    size_t i, o = 0;

    if (dst == NULL || dstsz < 2) {
        return;
    }
    dst[0] = '\0';
    if (rel == NULL) {
        return;
    }
    for (i = 0; rel[i] != '\0' && o + 1 < dstsz; i++) {
        char c = rel[i];

        if (c == '\\') {
            c = '/';
        }
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        dst[o++] = c;
    }
    dst[o] = '\0';
}

int dink_disc_opens(void)
{
    return g_disc_opens;
}

void dink_disc_note_open(void)
{
    g_disc_opens++;
}

void dink_blob_clear(void)
{
    int i;

    for (i = 0; i < g_nblob; i++) {
        free(g_blob[i].data);
        g_blob[i].data = NULL;
        g_blob[i].n = 0;
        g_blob[i].cls = 0;
        g_blob[i].age_out = 0;
        g_blob[i].rel[0] = '\0';
    }
    g_disc_opens = 0;
}

size_t dink_blob_bytes(void)
{
    size_t t = 0;
    int i;

    for (i = 0; i < g_nblob; i++) {
        if (g_blob[i].rel[0] != '\0' && g_blob[i].data != NULL) {
            t += g_blob[i].n;
        }
    }
    return t;
}

int dink_blob_slot(int i, const char **rel, size_t *n)
{
    int k, live = 0;

    if (i < 0) {
        return -1;
    }
    for (k = 0; k < g_nblob; k++) {
        if (g_blob[k].rel[0] == '\0' || g_blob[k].data == NULL) {
            continue;
        }
        if (live == i) {
            if (rel != NULL) {
                *rel = g_blob[k].rel;
            }
            if (n != NULL) {
                *n = g_blob[k].n;
            }
            return 0;
        }
        live++;
    }
    return -1;
}

void dink_blob_set_cls(const char *rel, int cls, int age_out)
{
    char key[DINK_FS_PATH_MAX];
    int i;

    if (rel == NULL) {
        return;
    }
    rel_key(key, sizeof(key), rel);
    for (i = 0; i < g_nblob; i++) {
        if (g_blob[i].rel[0] != '\0' && strcmp(g_blob[i].rel, key) == 0) {
            g_blob[i].cls = cls;
            g_blob[i].age_out = age_out;
            return;
        }
    }
}

int dink_blob_get_cls(const char *rel, int *cls, int *age_out)
{
    char key[DINK_FS_PATH_MAX];
    int i;

    if (rel == NULL) {
        return -1;
    }
    rel_key(key, sizeof(key), rel);
    for (i = 0; i < g_nblob; i++) {
        if (g_blob[i].rel[0] != '\0' && strcmp(g_blob[i].rel, key) == 0) {
            if (cls != NULL) {
                *cls = g_blob[i].cls;
            }
            if (age_out != NULL) {
                *age_out = g_blob[i].age_out;
            }
            return 0;
        }
    }
    return -1;
}

int dink_blob_put(const char *rel, uint8_t *data, size_t n)
{
    char key[DINK_FS_PATH_MAX];
    int empty;

    if (rel == NULL || rel[0] == '\0' || data == NULL || n == 0) {
        return -1;
    }
    rel_key(key, sizeof(key), rel);
    empty = blob_slot_empty();
    if (empty < 0) {
        return -1;
    }
    snprintf(g_blob[empty].rel, sizeof(g_blob[empty].rel), "%s", key);
    g_blob[empty].data = data;
    g_blob[empty].n = n;
    g_blob[empty].cls = 0;
    g_blob[empty].age_out = 0;
    residency_touch(g_blob[empty].rel);
    return 0;
}

int dink_blob_try_drop(const char *rel)
{
    char key[DINK_FS_PATH_MAX];
    int i;

    if (rel == NULL || rel[0] == '\0') {
        return 0;
    }
    rel_key(key, sizeof(key), rel);
    for (i = 0; i < g_nblob; i++) {
        if (g_blob[i].rel[0] != '\0' && strcmp(g_blob[i].rel, key) == 0) {
            printf("blob drop %s %u\n", g_blob[i].rel,
                   (unsigned)g_blob[i].n);
            free(g_blob[i].data);
            g_blob[i].data = NULL;
            g_blob[i].n = 0;
            g_blob[i].cls = 0;
            g_blob[i].age_out = 0;
            g_blob[i].rel[0] = '\0';
            return 0;
        }
    }
    return 0;
}

int dink_blob_get(const char *rel, const uint8_t **ptr, size_t *n)
{
    char key[DINK_FS_PATH_MAX];
    FILE *fp;
    uint8_t *raw = NULL;
    size_t got = 0;
    int i, empty, rc;

    if (ptr == NULL || n == NULL || rel == NULL || rel[0] == '\0') {
        return -1;
    }
    *ptr = NULL;
    *n = 0;
    rel_key(key, sizeof(key), rel);
    for (i = 0; i < g_nblob; i++) {
        if (g_blob[i].rel[0] != '\0' && strcmp(g_blob[i].rel, key) == 0 &&
            g_blob[i].data != NULL) {
            *ptr = g_blob[i].data;
            *n = g_blob[i].n;
            residency_touch(g_blob[i].rel);
            return 0;
        }
    }
    empty = blob_slot_empty();
    if (empty < 0) {
        return -1;
    }
    fp = dink_fopen(rel, "rb");
    if (fp == NULL) {
        return -1;
    }
    /* Drop Prev before the slurp alloc. Post-read drop is too late: KOS
     * already sbrk-failed the 1 MiB doubling buffer (map 409 seq 63). */
    {
        size_t need = 0;

        if (residency_swap_open() && !residency_is_always(key) &&
            blob_needs_room(key) && dink_fp_size(fp, &need) == 0 &&
            residency_make_room_keep(need, key) != 0) {
            printf("mem refuse pool=file_blob need=%u have=%u cap=%u\n",
                   (unsigned)need, (unsigned)dink_blob_bytes(),
                   (unsigned)DINK_MEM_BLOB_PEAK);
            fclose(fp);
            return -1;
        }
    }
    {
        char outer[sizeof(g_io_ctx)];

        memcpy(outer, g_io_ctx, sizeof(outer));
        dink_io_ctx(rel);
        rc = dink_fread_all(fp, &raw, &got);
        dink_io_ctx(outer);
    }
    fclose(fp);
    if (rc != 0 || raw == NULL) {
        free(raw);
        return -1;
    }
    if (residency_swap_open() && !residency_is_always(key) &&
        dink_blob_bytes() + got > (size_t)DINK_MEM_BLOB_PEAK) {
        if (blob_needs_room(key)) {
            (void)residency_make_room_keep(got, key);
        }
        if (dink_blob_bytes() + got > (size_t)DINK_MEM_BLOB_PEAK) {
            printf("mem refuse pool=file_blob need=%u have=%u cap=%u\n",
                   (unsigned)got, (unsigned)dink_blob_bytes(),
                   (unsigned)DINK_MEM_BLOB_PEAK);
            free(raw);
            return -1;
        }
    }
    g_disc_opens++;
    snprintf(g_blob[empty].rel, sizeof(g_blob[empty].rel), "%s", key);
    g_blob[empty].data = raw;
    g_blob[empty].n = got;
    g_blob[empty].cls = 0;
    g_blob[empty].age_out = 0;
    residency_touch(g_blob[empty].rel);
    *ptr = raw;
    *n = got;
    return 0;
}

int dink_slurp_rel(const char *rel, uint8_t **out, size_t *n)
{
    const uint8_t *p;
    size_t got;
    uint8_t *copy;

    if (out == NULL || n == NULL) {
        return -1;
    }
    *out = NULL;
    *n = 0;
    if (dink_blob_get(rel, &p, &got) != 0 || p == NULL) {
        return -1;
    }
    copy = (uint8_t *)malloc(got ? got : 1);
    if (copy == NULL) {
        return -1;
    }
    if (got > 0) {
        memcpy(copy, p, got);
    }
    *out = copy;
    *n = got;
    return 0;
}

/* Accept path if dink.dat is here or in a child named dink (any case). */
static int dink_fs_try_root(const char *path)
{
    const uint8_t *blob;
    size_t n;
    char child[DINK_FS_PATH_MAX];

    if (path == NULL || path[0] == '\0' || !dink_fs_exists_dir(path)) {
        return -1;
    }
    snprintf(g_root, sizeof(g_root), "%s", path);
    blob = NULL;
    n = 0;
    if (dink_blob_get("dink.dat", &blob, &n) == 0) {
        return 0;
    }
    if (resolve_comp(path, "dink", child, sizeof(child)) == 0) {
        snprintf(g_root, sizeof(g_root), "%s", child);
        blob = NULL;
        n = 0;
        if (dink_blob_get("dink.dat", &blob, &n) == 0) {
            return 0;
        }
    }
    g_root[0] = '\0';
    return -1;
}
