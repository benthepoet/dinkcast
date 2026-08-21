/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "residency.h"

#include "ff.h"
#include "fs.h"

#include <stdio.h>
#include <string.h>

enum {
    RES_OTHER = 0,
    RES_ALWAYS = 1,
    RES_SCREEN = 2,
    RES_PREV = 3
};

int residency_is_always(const char *rel)
{
    static const char *k[] = {
        "graphics/dink/idle/",
        "graphics/dink/walk/",
        "graphics/dink/push/",
        "graphics/dink/hit/",
        "graphics/dink/sword/",
        "graphics/dink/bow/",
        "graphics/inter/text-box/",
        "graphics/inter/arrow/",
        "graphics/inter/menu/",
        "dink.ini",
        "dink.dat",
        "story/",
        NULL,
    };
    int i;

    if (rel == NULL || rel[0] == '\0') {
        return 0;
    }
    for (i = 0; k[i] != NULL; i++) {
        size_t n = strlen(k[i]);

        if (strncmp(rel, k[i], n) == 0 || strcmp(rel, k[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int g_open; /* stays set after first swap_begin so tiles after
                    * edraw swap_end still mem-refuse over file_blob cap */

int residency_is_sticky_seq(int seq)
{
    return seq == 164;
}

int residency_swap_open(void)
{
    return g_open;
}

void residency_touch(const char *rel)
{
    int cls, age;

    if (rel == NULL || rel[0] == '\0') {
        return;
    }
    if (residency_is_always(rel)) {
        dink_blob_set_cls(rel, RES_ALWAYS, 0);
        return;
    }
    /* Armed preload_seq pins until DISARM. Do not demote those to Screen. */
    if (dink_blob_get_cls(rel, &cls, &age) == 0 && cls == RES_ALWAYS) {
        return;
    }
    dink_blob_set_cls(rel, RES_SCREEN, 0);
}

void residency_swap_begin(void)
{
    const char *rel;
    size_t n;
    int i, cls, age;

    g_open = 1;

    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        dink_blob_get_cls(rel, &cls, &age);
        if (residency_is_always(rel) || cls == RES_ALWAYS) {
            dink_blob_set_cls(rel, RES_ALWAYS, 0);
            continue;
        }
        if (cls == RES_PREV) {
            dink_blob_set_cls(rel, RES_PREV, 1);
        } else if (cls == RES_SCREEN) {
            dink_blob_set_cls(rel, RES_PREV, 0);
        }
    }
}

void residency_swap_end(void)
{
    char key[DINK_FS_PATH_MAX];
    const char *rel;
    size_t n;
    int i, cls, age, found;

    do {
        found = 0;
        key[0] = '\0';
        for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
            dink_blob_get_cls(rel, &cls, &age);
            if (residency_is_always(rel) || cls == RES_ALWAYS) {
                continue;
            }
            if (cls == RES_SCREEN || (cls == RES_PREV && !age)) {
                continue;
            }
            if (age && cls != RES_SCREEN) {
                snprintf(key, sizeof(key), "%s", rel);
                found = 1;
                break;
            }
        }
        if (found) {
            printf("residency drop %s\n", key);
            ff_cache_release(key);
            dink_blob_try_drop(key);
        }
    } while (found);
}

static size_t sum_cls(int want, int age_prev_only)
{
    const char *rel;
    size_t n, t = 0;
    int i, cls, age;

    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        dink_blob_get_cls(rel, &cls, &age);
        if (residency_is_always(rel) || cls == RES_ALWAYS) {
            cls = RES_ALWAYS;
        }
        if (want == RES_ALWAYS && cls == RES_ALWAYS) {
            t += n;
        } else if (want == RES_SCREEN && cls == RES_SCREEN) {
            t += n;
        } else if (want == RES_PREV && cls == RES_PREV && !age_prev_only) {
            t += n;
        }
    }
    return t;
}

size_t residency_bytes_always(void)
{
    return sum_cls(RES_ALWAYS, 0);
}

void residency_pin_always(const char *rel)
{
    if (rel == NULL || rel[0] == '\0') {
        return;
    }
    dink_blob_set_cls(rel, RES_ALWAYS, 0);
}

void residency_unpin(const char *rel)
{
    if (rel == NULL || rel[0] == '\0') {
        return;
    }
    if (residency_is_always(rel)) {
        return;
    }
    dink_blob_set_cls(rel, RES_SCREEN, 0);
}

size_t residency_bytes_screen(void)
{
    return sum_cls(RES_SCREEN, 0);
}

size_t residency_bytes_prev(void)
{
    return sum_cls(RES_PREV, 0);
}
