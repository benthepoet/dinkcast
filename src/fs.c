/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef DINK_DATA_DEFAULT
#define DINK_DATA_DEFAULT ""
#endif

static char g_root[DINK_FS_PATH_MAX];
static const char *g_pc = "/pc/dink";
static const char *g_cd = "/cd/dink";
static const char *g_fallback = DINK_DATA_DEFAULT;

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

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
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

    g_root[0] = '\0';
    if (dink_fs_exists_dir(g_pc)) {
        snprintf(g_root, sizeof(g_root), "%s", g_pc);
        return 0;
    }
    if (dink_fs_exists_dir(g_cd)) {
        snprintf(g_root, sizeof(g_root), "%s", g_cd);
        return 0;
    }
    env = getenv("DINK_DATA");
    if (env != NULL && dink_fs_exists_dir(env)) {
        snprintf(g_root, sizeof(g_root), "%s", env);
        return 0;
    }
    if (dink_fs_exists_dir(g_fallback)) {
        snprintf(g_root, sizeof(g_root), "%s", g_fallback);
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
        if (resolve_comp(cur, tok, next, sizeof(next)) != 0) {
            return NULL;
        }
        memcpy(cur, next, strlen(next) + 1);
        tok = strtok_r(NULL, "/", &save);
    } while (tok != NULL);

    return fopen(cur, mode);
}
