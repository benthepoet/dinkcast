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

int editor_draw_behind(int bg_a, int rank_a, int slot_a, int bg_b, int rank_b,
                       int slot_b)
{
    int la = bg_a ? 0 : 1;
    int lb = bg_b ? 0 : 1;

    if (la != lb) {
        return la < lb;
    }
    if (rank_a != rank_b) {
        return rank_a < rank_b;
    }
    return slot_a < slot_b;
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
        /* FreeDink load_screen_to: speed +92, base_walk +96,
         * base_idle +100, base_attack +104, base_hit +108, timing +112. */
        if (le_i32(p, n, off + 92, &out->sprite[i].speed) != 0 ||
            le_i32(p, n, off + 96, &out->sprite[i].base_walk) != 0 ||
            le_i32(p, n, off + 100, &out->sprite[i].base_idle) != 0 ||
            le_i32(p, n, off + 104, &out->sprite[i].base_attack) != 0 ||
            le_i32(p, n, off + 108, &out->sprite[i].base_hit) != 0 ||
            le_i32(p, n, off + 112, &out->sprite[i].timing) != 0) {
            return -1;
        }
        /* FreeDink: que +116, hard +120, vision +188. */
        if (le_i32(p, n, off + 116, &out->sprite[i].que) != 0) {
            return -1;
        }
        if (le_i32(p, n, off + 120, &out->sprite[i].hard) != 0) {
            return -1;
        }
        /* FreeDink rect alt after hard (+124). */
        if (le_i32(p, n, off + 124, &out->sprite[i].alt_l) != 0 ||
            le_i32(p, n, off + 128, &out->sprite[i].alt_t) != 0 ||
            le_i32(p, n, off + 132, &out->sprite[i].alt_r) != 0 ||
            le_i32(p, n, off + 136, &out->sprite[i].alt_b) != 0) {
            return -1;
        }
        if (le_i32(p, n, off + 188, &out->sprite[i].vision) != 0) {
            return -1;
        }
        /* is_warp +140 … parm_seq +156 (load_screen_to). */
        if (le_i32(p, n, off + 140, &out->sprite[i].is_warp) != 0 ||
            le_i32(p, n, off + 144, &out->sprite[i].warp_map) != 0 ||
            le_i32(p, n, off + 148, &out->sprite[i].warp_x) != 0 ||
            le_i32(p, n, off + 152, &out->sprite[i].warp_y) != 0 ||
            le_i32(p, n, off + 156, &out->sprite[i].parm_seq) != 0) {
            return -1;
        }
        /* base_die +160 … touch_damage +196. vision is +188 (above). */
        if (le_i32(p, n, off + 160, &out->sprite[i].base_die) != 0 ||
            le_i32(p, n, off + 164, &out->sprite[i].gold) != 0 ||
            le_i32(p, n, off + 168, &out->sprite[i].hitpoints) != 0 ||
            le_i32(p, n, off + 172, &out->sprite[i].strength) != 0 ||
            le_i32(p, n, off + 176, &out->sprite[i].defense) != 0 ||
            le_i32(p, n, off + 180, &out->sprite[i].exp) != 0 ||
            le_i32(p, n, off + 184, &out->sprite[i].sound) != 0 ||
            le_i32(p, n, off + 192, &out->sprite[i].nohit) != 0 ||
            le_i32(p, n, off + 196, &out->sprite[i].touch_damage) != 0) {
            return -1;
        }
        memcpy(out->sprite[i].script, p + off + 40, 13);
        out->sprite[i].script[13] = '\0';
        off += 220;
    }
    memcpy(out->script, p + DINK_MAP_SCRIPT_OFF, 20);
    out->script[20] = '\0';
    return 0;
}

int map_load_record(int rec, struct MapScreen *out)
{
    static FILE *fp;
    uint8_t *raw;
    long hold;

    if (rec < 1 || out == NULL) {
        return -1;
    }
    /* Keep map.dat open. Re-fopen of this 20 MiB ISO file hangs /cd. */
    if (fp == NULL) {
        fp = dink_fopen("map.dat", "rb");
        if (fp == NULL) {
            return -1;
        }
        dink_disc_note_open();
    }
    hold = (long)DINK_MAP_RECSIZE * (long)(rec - 1);
    raw = (uint8_t *)malloc(DINK_MAP_RECSIZE);
    if (raw == NULL) {
        return -1;
    }
    if (dink_pread(fp, hold, raw, DINK_MAP_RECSIZE) != 0) {
        free(raw);
        return -1;
    }
    if (map_parse_mem(raw, DINK_MAP_RECSIZE, out) != 0) {
        free(raw);
        return -1;
    }
    free(raw);
    return 0;
}
