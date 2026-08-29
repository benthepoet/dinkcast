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

#ifdef _arch_dreamcast
#include <kos.h>
#include <kos/mutex.h>
#endif

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

    if (g_root[0] == '\0' || rel == NULL || mode == NULL) {
        return NULL;
    }
    cd_lock();
    if (g_distill[0] != '\0') {
        fp = fopen_under(g_distill, rel, mode);
        if (fp != NULL) {
            cd_unlock();
            return fp;
        }
    }
    fp = fopen_under(g_root, rel, mode);
    cd_unlock();
    return fp;
}

#define DINK_CD_CHUNK 8192u
#define DINK_SLURP_MAX (4u * 1024u * 1024u)

void dink_cd_yield(void)
{
    /* Do not yield mid-file. A 2 MiB fread with thd_pass every 8 KiB
     * leaves the GD-ROM command sitting while other threads run. */
}

void dink_cd_settle(void)
{
#ifdef _arch_dreamcast
    /* Do not vid_waitvbl here: vblank may not fire during load. */
    thd_sleep(20);
#endif
}

int dink_fread_n(FILE *fp, uint8_t *dst, size_t n)
{
    size_t got = 0, chunk, nrd;

    if (fp == NULL || dst == NULL) {
        return -1;
    }
    cd_lock();
    while (got < n) {
        chunk = n - got;
        if (chunk > DINK_CD_CHUNK) {
            chunk = DINK_CD_CHUNK;
        }
        nrd = fread(dst + got, 1, chunk, fp);
        if (nrd == 0) {
            cd_unlock();
            return -1;
        }
        got += nrd;
    }
    cd_unlock();
    return 0;
}

int dink_pread(FILE *fp, long off, uint8_t *dst, size_t n)
{
    size_t got = 0, chunk, nrd;

    if (fp == NULL || dst == NULL) {
        return -1;
    }
    cd_lock();
    if (fseek(fp, off, SEEK_SET) != 0) {
        cd_unlock();
        return -1;
    }
    while (got < n) {
        chunk = n - got;
        if (chunk > DINK_CD_CHUNK) {
            chunk = DINK_CD_CHUNK;
        }
        nrd = fread(dst + got, 1, chunk, fp);
        if (nrd == 0) {
            cd_unlock();
            return -1;
        }
        got += nrd;
    }
    cd_unlock();
    return 0;
}

static int file_size(FILE *fp, size_t *n)
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

    if (fp == NULL || out == NULL || n == NULL) {
        return -1;
    }
    /* fstat, not SEEK_END. Doubling 32 KiB → 1 MiB for a 594 KB pack
     * sbrk-failed on 409 while Prev still held 408. */
    if (file_size(fp, &known) == 0) {
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
        }
        cd_unlock();
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
    }
    cd_unlock();
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
            blob_needs_room(key) && file_size(fp, &need) == 0 &&
            residency_make_room_keep(need, key) != 0) {
            printf("mem refuse pool=file_blob need=%u have=%u cap=%u\n",
                   (unsigned)need, (unsigned)dink_blob_bytes(),
                   (unsigned)DINK_MEM_BLOB_PEAK);
            fclose(fp);
            return -1;
        }
    }
    rc = dink_fread_all(fp, &raw, &got);
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
