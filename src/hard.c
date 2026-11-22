/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hard.h"

#include "fs.h"
#include "le.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *g_fp;
static uint8_t *g_rec[DINK_HARD_TILES];
/* A record that failed to read stays failed while this screen uses it:
 * one disc attempt per screen, not one per pixel (review §2.2). */
static uint8_t g_fail[DINK_HARD_TILES];
static size_t g_rec_bytes;
static int g_rec_n;
static int g_rec_reads;
static int g_test_open_fail;
static int g_open_tries;
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

void hard_fp_release(void)
{
    if (g_fp == NULL) {
        return;
    }
    fclose(g_fp);
    g_fp = NULL;
    printf("hard fp close\n");
}

static int hard_fp_ensure(void)
{
    if (g_fp != NULL) {
        return 0;
    }
    g_open_tries++;
    if (g_test_open_fail) {
        printf("hard fp open fail (test)\n");
        return -1;
    }
    g_fp = dink_fopen("hard.dat", "rb");
    if (g_fp == NULL) {
        printf("hard fp open fail\n");
        return -1;
    }
    dink_disc_note_open();
    printf("hard fp open\n");
    return 0;
}

static int hard_ensure_rec(int hid)
{
    uint8_t *p;
    long off;
    char outer[64];

    if (hid < 0 || hid >= DINK_HARD_TILES) {
        return -1;
    }
    if (g_rec[hid] != NULL) {
        return 0;
    }
    if (g_fail[hid]) {
        return -1;
    }
    if (hard_fp_ensure() != 0) {
        return -1;
    }
    p = (uint8_t *)malloc((size_t)DINK_HARD_REC);
    if (p == NULL) {
        printf("hard rec fail hid=%d (malloc)\n", hid);
        return -1;
    }
    off = (long)hid * (long)DINK_HARD_REC;
    printf("hard rec hid=%d\n", hid);
    dink_io_ctx_save(outer, sizeof(outer));
    {
        char ctx[24];

        snprintf(ctx, sizeof(ctx), "hard hid=%d", hid);
        dink_io_ctx(ctx);
    }
    if (dink_pread(g_fp, off, p, (size_t)DINK_HARD_REC) != 0) {
        dink_io_ctx(outer);
        free(p);
        g_fail[hid] = 1;
        printf("hard rec fail hid=%d\n", hid);
        return -1;
    }
    dink_io_ctx(outer);
    g_rec[hid] = p;
    g_rec_bytes += (size_t)DINK_HARD_REC;
    g_rec_n++;
    g_rec_reads++;
    return 0;
}

void hard_cache_new_visit(void)
{
    memset(g_fail, 0, sizeof(g_fail));
}

int hard_cache_reads(void)
{
    return g_rec_reads;
}

size_t hard_cache_bytes(void)
{
    return g_rec_bytes;
}

int hard_cache_recs(void)
{
    return g_rec_n;
}

int hard_rec_failed(int hid)
{
    return hid >= 0 && hid < DINK_HARD_TILES && g_fail[hid];
}

void hard_test_open_fail(int on)
{
    g_test_open_fail = on;
}

int hard_test_open_tries(void)
{
    return g_open_tries;
}

void hard_test_mark_failed(int hid)
{
    if (hid >= 0 && hid < DINK_HARD_TILES && g_rec[hid] == NULL) {
        g_fail[hid] = 1;
    }
}

void hard_cache_drop_all(void)
{
    int i;

    for (i = 0; i < DINK_HARD_TILES; i++) {
        free(g_rec[i]);
        g_rec[i] = NULL;
        g_fail[i] = 0;
    }
    g_rec_bytes = 0;
    g_rec_n = 0;
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
    if (g_def_ok) {
        memcpy(out->btile_default, g_def, sizeof(g_def));
        out->ready = 1;
        return 0;
    }
    printf("hard load\n");
    if (hard_fp_ensure() != 0) {
        return -1;
    }
    off = (long)DINK_HARD_TILES * (long)DINK_HARD_REC;
    ntail = (size_t)DINK_BTILE_MAX * 4u;
    tail = (uint8_t *)malloc(ntail);
    if (tail == NULL) {
        return -1;
    }
    dink_io_ctx("hard tail");
    if (dink_pread(g_fp, off, tail, ntail) != 0) {
        dink_io_ctx(NULL);
        free(tail);
        return -1;
    }
    dink_io_ctx(NULL);
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
    static uint8_t need[DINK_HARD_TILES];
    int i, lx, ly, nneed = 0, nread = 0, nfail = 0, ndrop = 0, noio = 0;

    if (h == NULL || scr == NULL || out == NULL) {
        return -1;
    }
    hard_mask_free(out);
    out->pix = (uint8_t *)calloc((size_t)DINK_PLAY_W * (size_t)DINK_PLAY_H, 1);
    if (out->pix == NULL) {
        return -1;
    }
    if (!h->ready) {
        /* No hard.dat defaults (load_screen / boot already logged the
         * failure): tiles are walkable and no record read is tried. */
        printf("hard stamp no hard.dat: tiles walkable\n");
        return 0;
    }
    memset(need, 0, sizeof(need));
    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        int hid = hard_id_for_tile(h, scr->t[i].square_full_idx0, scr->t[i].althard);

        if (hid >= 0 && hid < DINK_HARD_TILES && !need[hid]) {
            need[hid] = 1;
            nneed++;
            if (g_rec[hid] == NULL && !g_fail[hid] && !noio) {
                nread++;
                /* No handle or no RAM: stop trying this stamp (one
                 * attempt, not one per tile); those tiles stay walkable. */
                if (hard_ensure_rec(hid) != 0 && !g_fail[hid]) {
                    noio = 1;
                }
            }
            if (g_fail[hid]) {
                nfail++;
            }
        }
    }
    /* Screen class: keep only the records this screen's tiles use, so the
     * cache is at most 96 records and a draw_hard_map re-stamp in play
     * never reads the disc. A failure is forgotten with its record, and
     * load_screen forgets all of them (hard_cache_new_visit). */
    for (i = 0; i < DINK_HARD_TILES; i++) {
        if (need[i]) {
            continue;
        }
        if (g_rec[i] != NULL) {
            free(g_rec[i]);
            g_rec[i] = NULL;
            g_rec_bytes -= (size_t)DINK_HARD_REC;
            g_rec_n--;
            ndrop++;
        }
        g_fail[i] = 0;
    }
    if (nread > 0 || ndrop > 0 || nfail > 0 || noio) {
        printf("hard cache recs=%d bytes=%u read=%d drop=%d fail=%d noio=%d\n",
               g_rec_n, (unsigned)g_rec_bytes, nread, ndrop, nfail, noio);
    }
    (void)nneed;
    for (i = 0; i < DINK_SCREEN_TILES; i++) {
        int hid = hard_id_for_tile(h, scr->t[i].square_full_idx0, scr->t[i].althard);
        int tx = (i % 12) * DINK_TILE_PX;
        int ty = (i / 12) * DINK_TILE_PX;
        const uint8_t *rec;

        if (hid < 0 || hid >= DINK_HARD_TILES) {
            continue;
        }
        rec = g_rec[hid];
        /* A record whose read failed is stamped solid: FreeDink would have
         * it, and a wall walked through is worse than one blocked tile. A
         * record we could not even try (no handle, no RAM) stays walkable
         * and is tried again at the next stamp, never once per pixel. */
        if (rec == NULL && !g_fail[hid]) {
            continue;
        }
        for (ly = 0; ly < DINK_TILE_PX; ly++) {
            for (lx = 0; lx < DINK_TILE_PX; lx++) {
                if (rec == NULL ||
                    rec[(size_t)lx * (size_t)DINK_HARD_PX + (size_t)ly] != 0) {
                    out->pix[(ty + ly) * DINK_PLAY_W + (tx + lx)] = 1;
                }
            }
        }
    }
    return 0;
}

int hard_stamp_without_pixels(int type)
{
    return type == DINK_SPR_TYPE_INVISIBLE;
}

int hard_stamp_editor_slot(int type, int live)
{
    /* Type 1 is fill_hard_sprites: spr[].active. sp_active(0) drops it.
     * Type 0/2 is fill_back_sprites from the editor row. */
    if (type == 1) {
        return live ? 1 : 0;
    }
    return 1;
}

void hard_stamp_editor(struct HardMask *m, int x, int y, int type, int hid,
                       int have_pixels, int hl, int ht, int hr, int hb)
{
    if (!have_pixels && !hard_stamp_without_pixels(type)) {
        return;
    }
    hard_stamp_box(m, x, y, hl, ht, hr, hb, hid);
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
