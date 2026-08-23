/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hard.h"

#include "fs.h"
#include "le.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *g_fp;
static uint8_t *g_rec[DINK_HARD_TILES];
static uint16_t g_def[DINK_BTILE_MAX];
static int g_def_ok;
static int g_screenlock;

int hard_screenlock_get(void)
{
    return g_screenlock;
}

void hard_screenlock_set(int on)
{
    g_screenlock = on ? 1 : 0;
}

int hard_parse_defaults(const uint8_t *p, size_t n, struct HardMap *out)
{
    size_t off;
    int i;

    if (p == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    off = (size_t)DINK_HARD_TILES * (size_t)DINK_HARD_REC;
    if (off + (size_t)DINK_BTILE_MAX * 4u > n) {
        return -1;
    }
    for (i = 0; i < DINK_BTILE_MAX; i++) {
        int32_t v;
        if (le_i32(p, n, off, &v) != 0) {
            return -1;
        }
        out->btile_default[i] = (uint16_t)(v < 0 ? 0 : v);
        off += 4;
    }
    out->ready = 1;
    return 0;
}

void hard_free(struct HardMap *h)
{
    if (h == NULL) {
        return;
    }
    h->ready = 0;
}

static int hard_ensure_rec(int hid)
{
    uint8_t *p;
    long off;

    if (hid < 0 || hid >= DINK_HARD_TILES || g_fp == NULL) {
        return -1;
    }
    if (g_rec[hid] != NULL) {
        return 0;
    }
    p = (uint8_t *)malloc((size_t)DINK_HARD_REC);
    if (p == NULL) {
        return -1;
    }
    off = (long)hid * (long)DINK_HARD_REC;
    if (dink_pread(g_fp, off, p, (size_t)DINK_HARD_REC) != 0) {
        free(p);
        return -1;
    }
    g_rec[hid] = p;
    return 0;
}

int hard_load(struct HardMap *out)
{
    uint8_t *tail;
    size_t ntail;
    long off;
    int i;

    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (g_fp != NULL && g_def_ok) {
        memcpy(out->btile_default, g_def, sizeof(g_def));
        out->ready = 1;
        return 0;
    }
    printf("hard load\n");
    if (g_fp == NULL) {
        g_fp = dink_fopen("hard.dat", "rb");
        if (g_fp == NULL) {
            return -1;
        }
        dink_disc_note_open();
    }
    off = (long)DINK_HARD_TILES * (long)DINK_HARD_REC;
    ntail = (size_t)DINK_BTILE_MAX * 4u;
    tail = (uint8_t *)malloc(ntail);
    if (tail == NULL) {
        return -1;
    }
    if (dink_pread(g_fp, off, tail, ntail) != 0) {
        free(tail);
        return -1;
    }
    for (i = 0; i < DINK_BTILE_MAX; i++) {
        int32_t v;

        if (le_i32(tail, ntail, (size_t)i * 4u, &v) != 0) {
            free(tail);
            return -1;
        }
        g_def[i] = (uint16_t)(v < 0 ? 0 : v);
    }
    free(tail);
    g_def_ok = 1;
    memcpy(out->btile_default, g_def, sizeof(g_def));
    out->ready = 1;
    printf("hard load defaults ok\n");
    return 0;
}

int hard_id_for_tile(const struct HardMap *h, int32_t square_full_idx0,
                     int32_t althard)
{
    if (althard > 0) {
        return (int)althard;
    }
    if (h == NULL || square_full_idx0 < 0 || square_full_idx0 >= DINK_BTILE_MAX) {
        return 0;
    }
    return (int)h->btile_default[square_full_idx0];
}

int hard_sample(const struct HardMap *h, int hid, int lx, int ly)
{
    size_t off;
    int x, y;

    if (h == NULL || hid < 0 || hid >= DINK_HARD_TILES) {
        return 0;
    }
    if (lx < 0 || ly < 0 || lx >= 50 || ly >= 50) {
        return 1;
    }
    if (hard_ensure_rec(hid) != 0 || g_rec[hid] == NULL) {
        return 0;
    }
    /* Disk: for x in 0..50, for y in 0..50: hm[x][y] */
    x = lx;
    y = ly;
    off = (size_t)x * (size_t)DINK_HARD_PX + (size_t)y;
    if (off >= (size_t)DINK_HARD_REC) {
        return 0;
    }
    return g_rec[hid][off] != 0;
}

void hard_mask_free(struct HardMask *m)
{
    if (m == NULL) {
        return;
    }
    free(m->pix);
    m->pix = NULL;
}

int hard_stamp_tiles(const struct HardMap *h, const struct MapScreen *scr,
                     struct HardMask *out)
{
    int i, lx, ly;

    if (h == NULL || scr == NULL || out == NULL) {
        return -1;
    }
    hard_mask_free(out);
    out->pix = (uint8_t *)calloc((size_t)DINK_PLAY_W * (size_t)DINK_PLAY_H, 1);
    if (out->pix == NULL) {
        return -1;
    }
    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        int hid = hard_id_for_tile(h, scr->t[i].square_full_idx0, scr->t[i].althard);

        if (hid > 0 && hid < DINK_HARD_TILES && hard_ensure_rec(hid) != 0) {
            printf("hard rec fail hid=%d\n", hid);
        }
    }
    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        int hid = hard_id_for_tile(h, scr->t[i].square_full_idx0, scr->t[i].althard);
        int tx = (i % 12) * DINK_TILE_PX;
        int ty = (i / 12) * DINK_TILE_PX;

        for (ly = 0; ly < DINK_TILE_PX; ly++) {
            for (lx = 0; lx < DINK_TILE_PX; lx++) {
                if (hard_sample(h, hid, lx, ly)) {
                    out->pix[(ty + ly) * DINK_PLAY_W + (tx + lx)] = 1;
                }
            }
        }
    }
    return 0;
}

void hard_stamp_box(struct HardMask *m, int x, int y, int hl, int ht, int hr,
                    int hb, int hid)
{
    int px, py, x0, y0, x1, y1;

    if (m == NULL || m->pix == NULL) {
        return;
    }
    x0 = x + hl - DINK_PLAY_LEFT;
    y0 = y + ht - DINK_PLAY_TOP;
    x1 = x + hr - DINK_PLAY_LEFT;
    y1 = y + hb - DINK_PLAY_TOP;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    /* FreeDink add_hardness: [left, right) × [top, bottom), hitmap[xx-20][yy]. */
    if (x1 > DINK_PLAY_W) {
        x1 = DINK_PLAY_W;
    }
    if (y1 > DINK_PLAY_H) {
        y1 = DINK_PLAY_H;
    }
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            m->pix[py * DINK_PLAY_W + px] = (uint8_t)(hid < 1 ? 1 : hid);
        }
    }
}

int hard_get(const struct HardMask *m, int sx, int sy)
{
    int px, py;

    if (m == NULL || m->pix == NULL) {
        return 0;
    }
    px = sx - DINK_PLAY_LEFT;
    py = sy - DINK_PLAY_TOP;
    /* FreeDink get_hard: screenlock clamps before the OOB→0 test
     * (diag off the border would skip hardness). */
    if (g_screenlock) {
        if (px < 0) {
            px = 0;
        } else if (px > DINK_PLAY_W - 1) {
            px = DINK_PLAY_W - 1;
        }
        if (py < 0) {
            py = 0;
        } else if (py > DINK_PLAY_H - 1) {
            py = DINK_PLAY_H - 1;
        }
    }
    if (px < 0 || py < 0 || px >= DINK_PLAY_W || py >= DINK_PLAY_H) {
        return 0;
    }
    return (int)m->pix[py * DINK_PLAY_W + px];
}

int hard_get_play(const struct HardMask *m, const struct MapScreen *scr,
                  int sx, int sy, int *warp_ed)
{
    int v = hard_get(m, sx, sy);
    int ed;

    if (warp_ed != NULL) {
        *warp_ed = 0;
    }
    if (scr == NULL || v <= 100) {
        return v;
    }
    ed = v - 100;
    if (ed < 1 || ed > 100 || scr->sprite[ed].is_warp == 0) {
        return v;
    }
    if (warp_ed != NULL) {
        *warp_ed = ed;
    }
    return 0;
}

int hard_box_blocked(const struct HardMask *m, int x, int y, int hl, int ht,
                     int hr, int hb)
{
    int px, py, x0, y0, x1, y1;

    if (m == NULL || m->pix == NULL) {
        return 0;
    }
    x0 = x + hl - DINK_PLAY_LEFT;
    y0 = y + ht - DINK_PLAY_TOP;
    x1 = x + hr - DINK_PLAY_LEFT;
    y1 = y + hb - DINK_PLAY_TOP;
    if (x0 < 0 || y0 < 0 || x1 >= DINK_PLAY_W || y1 >= DINK_PLAY_H) {
        return 1;
    }
    for (py = y0; py <= y1; py++) {
        for (px = x0; px <= x1; px++) {
            if (m->pix[py * DINK_PLAY_W + px]) {
                return 1;
            }
        }
    }
    return 0;
}
