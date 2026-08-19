/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_file.h"

#include "fs.h"

#include <ctype.h>
#include <stdint.h>
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
    uint8_t *raw = NULL;
    size_t sz = 0;
    char *txt;

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
    if (dink_slurp_rel(rel, &raw, &sz) != 0) {
        printf("dinkc miss %s\n", rel);
        return -1;
    }
    if (sz > (size_t)DINKC_FILE_CAP) {
        free(raw);
        printf("dinkc too big %s %u\n", rel, (unsigned)sz);
        return -1;
    }
    txt = (char *)realloc(raw, sz + 1);
    if (txt == NULL) {
        free(raw);
        return -1;
    }
    txt[sz] = '\0';
    if (out != NULL) {
        *out = txt;
    } else {
        free(txt);
    }
    if (n != NULL) {
        *n = sz;
    }
    printf("dinkc load %s %u\n", rel, (unsigned)sz);
    return 0;
}

void dinkc_free(char *p)
{
    free(p);
}
