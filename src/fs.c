/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _arch_dreamcast
#include <kos.h>
#endif

#ifndef DINK_DATA_DEFAULT
#define DINK_DATA_DEFAULT ""
#endif

static char g_root[DINK_FS_PATH_MAX];
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

FILE *dink_fopen(const char *rel, const char *mode)
{
    char cur[DINK_FS_PATH_MAX];
    char next[DINK_FS_PATH_MAX];
    char relnorm[DINK_FS_PATH_MAX];
    char *save = NULL;
    char *tok;

    if (g_root[0] == '\0' || rel == NULL || mode == NULL) {
        return NULL;
    }
    snprintf(relnorm, sizeof(relnorm), "%s", rel);
    slashes(relnorm);
    snprintf(cur, sizeof(cur), "%s", g_root);

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

#define DINK_CD_CHUNK 8192u
#define DINK_SLURP_MAX (4u * 1024u * 1024u)

void dink_cd_yield(void)
{
#ifdef _arch_dreamcast
    thd_pass();
#endif
}

void dink_cd_settle(void)
{
#ifdef _arch_dreamcast
    /* Let GD-ROM/ISO go idle before the next fopen. Back-to-back
     * dir.ff then ts02.bmp is the house-door hang (stamp ok, no atlas). */
    vid_waitvbl();
    vid_waitvbl();
#endif
}

int dink_fread_n(FILE *fp, uint8_t *dst, size_t n)
{
    size_t got = 0, chunk, nrd;

    if (fp == NULL || dst == NULL) {
        return -1;
    }
    while (got < n) {
        chunk = n - got;
        if (chunk > DINK_CD_CHUNK) {
            chunk = DINK_CD_CHUNK;
        }
        nrd = fread(dst + got, 1, chunk, fp);
        if (nrd == 0) {
            return -1;
        }
        got += nrd;
        dink_cd_yield();
    }
    return 0;
}

int dink_fread_all(FILE *fp, uint8_t **out, size_t *n)
{
    uint8_t *p;
    size_t cap, got, nrd;

    if (fp == NULL || out == NULL || n == NULL) {
        return -1;
    }
    cap = 32u * 1024u;
    p = (uint8_t *)malloc(cap);
    if (p == NULL) {
        return -1;
    }
    got = 0;
    for (;;) {
        if (got + DINK_CD_CHUNK > cap) {
            uint8_t *np;
            size_t ncap = cap * 2u;

            if (ncap > DINK_SLURP_MAX || ncap < cap) {
                free(p);
                return -1;
            }
            np = (uint8_t *)realloc(p, ncap);
            if (np == NULL) {
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
        dink_cd_yield();
    }
    *out = p;
    *n = got;
    return 0;
}

int dink_slurp_rel(const char *rel, uint8_t **out, size_t *n)
{
    FILE *fp;
    int rc;

    if (out == NULL || n == NULL) {
        return -1;
    }
    *out = NULL;
    *n = 0;
    fp = dink_fopen(rel, "rb");
    if (fp == NULL) {
        return -1;
    }
    rc = dink_fread_all(fp, out, n);
    fclose(fp);
    dink_cd_settle();
    if (rc != 0) {
        free(*out);
        *out = NULL;
        *n = 0;
    }
    return rc;
}

/* Accept path if dink.dat is here or in a child named dink (any case). */
static int dink_fs_try_root(const char *path)
{
    FILE *fp;
    char child[DINK_FS_PATH_MAX];

    if (path == NULL || path[0] == '\0' || !dink_fs_exists_dir(path)) {
        return -1;
    }
    snprintf(g_root, sizeof(g_root), "%s", path);
    fp = dink_fopen("dink.dat", "rb");
    if (fp != NULL) {
        fclose(fp);
        return 0;
    }
    if (resolve_comp(path, "dink", child, sizeof(child)) == 0) {
        snprintf(g_root, sizeof(g_root), "%s", child);
        fp = dink_fopen("dink.dat", "rb");
        if (fp != NULL) {
            fclose(fp);
            return 0;
        }
    }
    g_root[0] = '\0';
    return -1;
}
