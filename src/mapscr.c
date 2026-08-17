/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mapscr.h"

#include "fs.h"
#include "le.h"

#include <stdlib.h>
#include <string.h>

void tile_split(int32_t square_full_idx0, int *sheet0, int *cell)
{
    if (sheet0 != NULL) {
        *sheet0 = (int)(square_full_idx0 / 128);
    }
    if (cell != NULL) {
        *cell = (int)(square_full_idx0 % 128);
    }
}

int editor_sprite_on_vision(const struct EditorSprite *s, int vision)
{
    if (s == NULL || !s->active) {
        return 0;
    }
    if (s->vision == 0 || s->vision == vision) {
        return 1;
    }
    return 0;
}

int editor_sprite_draw(const struct EditorSprite *s, int vision)
{
    if (!editor_sprite_on_vision(s, vision)) {
        return 0;
    }
    if (s->type == DINK_SPR_TYPE_INVISIBLE) {
        return 0;
    }
    return 1;
}

int editor_sprite_rank_y(const struct EditorSprite *s)
{
    if (s == NULL) {
        return 0;
    }
    if (s->que != 0) {
        return (int)s->que;
    }
    return (int)s->y;
}

int map_file_records(int64_t file_bytes, int *out_count, int *out_rem)
{
    if (file_bytes < 0 || out_count == NULL || out_rem == NULL) {
        return -1;
    }
    *out_count = (int)(file_bytes / DINK_MAP_RECSIZE);
    *out_rem = (int)(file_bytes % DINK_MAP_RECSIZE);
    return 0;
}

int map_parse_mem(const uint8_t *p, size_t n, struct MapScreen *out)
{
    size_t off;
    int i;

    if (p == NULL || out == NULL || n < DINK_MAP_RECSIZE) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    off = 20; /* unused name */
    for (i = 0; i < 97; i++) {
        if (le_i32(p, n, off, &out->t[i].square_full_idx0) != 0) {
            return -1;
        }
        if (le_i32(p, n, off + 8, &out->t[i].althard) != 0) {
            return -1;
        }
        off += 80;
    }
    off = 8020;
    for (i = 0; i < 101; i++) {
        if (le_i32(p, n, off, &out->sprite[i].x) != 0 ||
            le_i32(p, n, off + 4, &out->sprite[i].y) != 0 ||
            le_i32(p, n, off + 8, &out->sprite[i].seq) != 0 ||
            le_i32(p, n, off + 12, &out->sprite[i].frame) != 0 ||
            le_i32(p, n, off + 16, &out->sprite[i].type) != 0 ||
            le_i32(p, n, off + 20, &out->sprite[i].size) != 0) {
            return -1;
        }
        out->sprite[i].active = p[off + 24] ? 1 : 0;
        if (le_i32(p, n, off + 36, &out->sprite[i].brain) != 0) {
            return -1;
        }
        /* FreeDink: que +116, hard +120, vision +188. */
        if (le_i32(p, n, off + 116, &out->sprite[i].que) != 0) {
            return -1;
        }
        if (le_i32(p, n, off + 120, &out->sprite[i].hard) != 0) {
            return -1;
        }
        if (le_i32(p, n, off + 188, &out->sprite[i].vision) != 0) {
            return -1;
        }
        memcpy(out->sprite[i].script, p + off + 40, 13);
        out->sprite[i].script[13] = '\0';
        off += 220;
    }
    memcpy(out->script, p + 30204, 20);
    out->script[20] = '\0';
    return 0;
}

int map_load_record(int rec, struct MapScreen *out)
{
    FILE *fp;
    uint8_t *raw;
    long hold;

    if (rec < 1 || out == NULL) {
        return -1;
    }
    fp = dink_fopen("map.dat", "rb");
    if (fp == NULL) {
        return -1;
    }
    hold = (long)DINK_MAP_RECSIZE * (long)(rec - 1);
    if (fseek(fp, hold, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    raw = (uint8_t *)malloc(DINK_MAP_RECSIZE);
    if (raw == NULL) {
        fclose(fp);
        return -1;
    }
    if (fread(raw, 1, DINK_MAP_RECSIZE, fp) != DINK_MAP_RECSIZE) {
        free(raw);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (map_parse_mem(raw, DINK_MAP_RECSIZE, out) != 0) {
        free(raw);
        return -1;
    }
    free(raw);
    return 0;
}
