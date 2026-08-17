/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_file.h"

#include "fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dinkc_story_rel(const char *name, char *rel, size_t relsz)
{
    char stem[32];
    size_t i, n;
    const char *dot;

    if (name == NULL || rel == NULL || relsz < 16 || name[0] == '\0') {
        return -1;
    }
    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
        return -1;
    }
    if (strstr(name, "..") != NULL) {
        return -1;
    }
    n = strlen(name);
    if (n >= sizeof(stem)) {
        return -1;
    }
    memcpy(stem, name, n + 1);
    dot = strrchr(stem, '.');
    if (dot != NULL && dot != stem) {
        stem[dot - stem] = '\0';
    }
    if (stem[0] == '\0') {
        return -1;
    }
    for (i = 0; stem[i] != '\0'; i++) {
        unsigned char c = (unsigned char)stem[i];

        if (!(isalnum(c) || c == '-' || c == '_')) {
            return -1;
        }
    }
    if (snprintf(rel, relsz, "story/%s.c", stem) >= (int)relsz) {
        return -1;
    }
    return 0;
}

int dinkc_load(const char *name, char **out, size_t *n)
{
    char rel[80];
    FILE *fp;
    long sz;
    char *raw;

    if (out != NULL) {
        *out = NULL;
    }
    if (n != NULL) {
        *n = 0;
    }
    if (dinkc_story_rel(name, rel, sizeof(rel)) != 0) {
        printf("dinkc bad name %s\n", name != NULL ? name : "");
        return -1;
    }
    fp = dink_fopen(rel, "rb");
    if (fp == NULL) {
        printf("dinkc miss %s\n", rel);
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        printf("dinkc miss %s\n", rel);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        printf("dinkc miss %s\n", rel);
        return -1;
    }
    if (sz > DINKC_FILE_CAP) {
        fclose(fp);
        printf("dinkc too big %s %ld\n", rel, sz);
        return -1;
    }
    raw = (char *)malloc((size_t)sz + 1);
    if (raw == NULL) {
        fclose(fp);
        return -1;
    }
    if (sz > 0 && fread(raw, 1, (size_t)sz, fp) != (size_t)sz) {
        free(raw);
        fclose(fp);
        printf("dinkc miss %s\n", rel);
        return -1;
    }
    fclose(fp);
    raw[sz] = '\0';
    if (out != NULL) {
        *out = raw;
    } else {
        free(raw);
    }
    if (n != NULL) {
        *n = (size_t)sz;
    }
    printf("dinkc load %s %ld\n", rel, sz);
    return 0;
}

void dinkc_free(char *p)
{
    free(p);
}
