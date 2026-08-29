/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "residency.h"

#include "ff.h"
#include "fs.h"
#include "mem.h"

#include <stdio.h>
#include <string.h>

enum {
    RES_OTHER = 0,
    RES_ALWAYS = 1,
    RES_SCREEN = 2,
    RES_PREV = 3
};

#define HOLD_BANKS 2
#define HOLD_MAX 24

static char g_hold[HOLD_BANKS][HOLD_MAX][DINK_FS_PATH_MAX];
static int g_nhold[HOLD_BANKS];

int residency_is_always(const char *rel)
{
    static const char *k[] = {
        "graphics/dink/idle/",
        "graphics/dink/walk/",
        "graphics/dink/push/",
        "graphics/dink/hit/",
        "graphics/dink/die/",
        "graphics/dink/sword/",
        "graphics/dink/bow/",
        "graphics/inter/text-box/",
        "graphics/inter/arrow/",
        "graphics/inter/menu/",
        "graphics/inter/status/",
        "graphics/inter/numbers/",
        "graphics/inter/health/",
        "graphics/inter/level#/",
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
        /* ARM-held packs (treefire/splode) stay Screen across swaps;
         * demoting them to Prev let drop_one_prev release the pack the
         * preload was meant to keep (GD-ROM re-read at impact). */
        if (residency_is_held(rel)) {
            dink_blob_set_cls(rel, RES_SCREEN, 0);
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

static int rel_is_dir_ff(const char *rel)
{
    size_t n;

    if (rel == NULL) {
        return 0;
    }
    n = strlen(rel);
    return n >= 6 && strcmp(rel + n - 6, "dir.ff") == 0;
}

int residency_drop_one_prev(void)
{
    char key[DINK_FS_PATH_MAX];
    const char *rel;
    size_t n, best_n = 0;
    int i, cls, found = 0;

    key[0] = '\0';
    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        dink_blob_get_cls(rel, &cls, NULL);
        if (!rel_is_dir_ff(rel) || cls != RES_PREV) {
            continue;
        }
        /* Defense in depth: swap_begin keeps held packs Screen, but never
         * let a Prev drop release an ARM-held pack. */
        if (residency_is_held(rel)) {
            continue;
        }
        if (n > best_n) {
            snprintf(key, sizeof(key), "%s", rel);
            best_n = n;
            found = 1;
        }
    }
    if (!found) {
        return -1;
    }
    printf("residency drop prev %s\n", key);
    ff_cache_release(key);
    dink_blob_try_drop(key);
    return 0;
}

static int rel_is_tile_or_hard(const char *rel)
{
    if (rel == NULL) {
        return 1;
    }
    if (strncmp(rel, "tiles/", 6) == 0) {
        return 1;
    }
    if (strstr(rel, "hard.dat") != NULL) {
        return 1;
    }
    return 0;
}

void residency_hold(int bank, const char *rel)
{
    int i;

    if (bank < 0 || bank >= HOLD_BANKS || rel == NULL || rel[0] == '\0') {
        return;
    }
    for (i = 0; i < g_nhold[bank]; i++) {
        if (strcmp(g_hold[bank][i], rel) == 0) {
            return;
        }
    }
    if (g_nhold[bank] >= HOLD_MAX) {
        printf("residency hold full bank=%d rel=%s\n", bank, rel);
        return;
    }
    snprintf(g_hold[bank][g_nhold[bank]], sizeof(g_hold[bank][0]), "%s", rel);
    g_nhold[bank]++;
}

void residency_hold_clear(int bank)
{
    if (bank < 0 || bank >= HOLD_BANKS) {
        return;
    }
    memset(g_hold[bank], 0, sizeof(g_hold[bank]));
    g_nhold[bank] = 0;
}

int residency_is_held(const char *rel)
{
    int b, i;

    if (rel == NULL || rel[0] == '\0') {
        return 0;
    }
    for (b = 0; b < HOLD_BANKS; b++) {
        for (i = 0; i < g_nhold[b]; i++) {
            if (strcmp(g_hold[b][i], rel) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int residency_drop_one_screen(const char *keep)
{
    char key[DINK_FS_PATH_MAX];
    const char *rel;
    size_t n, best_n = 0;
    int i, cls, found = 0;

    key[0] = '\0';
    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        dink_blob_get_cls(rel, &cls, NULL);
        if (!rel_is_dir_ff(rel) || cls != RES_SCREEN) {
            continue;
        }
        if (rel_is_tile_or_hard(rel)) {
            continue;
        }
        if (keep != NULL && keep[0] != '\0' && strcmp(rel, keep) == 0) {
            continue;
        }
        if (residency_is_held(rel)) {
            continue;
        }
        if (n > best_n) {
            snprintf(key, sizeof(key), "%s", rel);
            best_n = n;
            found = 1;
        }
    }
    if (!found) {
        return -1;
    }
    printf("residency drop screen %s\n", key);
    ff_cache_release(key);
    dink_blob_try_drop(key);
    return 0;
}

/* Largest held pack as a last resort. Holds are best-effort: keeping
 * treefire/splode must never wedge the current screen (trees pack
 * refused -> bare trunks). Dropping a held pack re-reads it from GD-ROM
 * at impact, which beats missing screen graphics. */
static int residency_drop_one_held(const char *keep)
{
    char key[DINK_FS_PATH_MAX];
    const char *rel;
    size_t n, best_n = 0;
    int i, found = 0;

    key[0] = '\0';
    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        if (!rel_is_dir_ff(rel) || !residency_is_held(rel)) {
            continue;
        }
        if (keep != NULL && keep[0] != '\0' && strcmp(rel, keep) == 0) {
            continue;
        }
        if (n > best_n) {
            snprintf(key, sizeof(key), "%s", rel);
            best_n = n;
            found = 1;
        }
    }
    if (!found) {
        return -1;
    }
    printf("residency drop held %s\n", key);
    ff_cache_release(key);
    dink_blob_try_drop(key);
    return 0;
}

int residency_make_room(size_t need)
{
    return residency_make_room_keep(need, NULL);
}

int residency_make_room_keep(size_t need, const char *keep)
{
    while (dink_blob_bytes() + need > (size_t)DINK_MEM_BLOB_PEAK) {
        if (residency_drop_one_prev() == 0) {
            continue;
        }
        if (residency_drop_one_screen(keep) == 0) {
            continue;
        }
        if (residency_drop_one_held(keep) == 0) {
            continue;
        }
        return -1;
    }
    return 0;
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
