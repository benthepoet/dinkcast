/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Bite 14.3: ping-pong two village screens 20 times. file_blob / always /
 * ts_rgb must not climb after a warm swap. cpu_pixels may follow the screen.
 */
#include "edraw.h"
#include "ff.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "mem.h"
#include "residency.h"
#include "tiles.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long labs_delta(size_t a, size_t b)
{
    return a > b ? (long)(a - b) : (long)(b - a);
}

int main(void)
{
    struct World w;
    struct MapScreen *scr;
    struct SeqInfo *seqs;
    struct EdGfx *g;
    struct TileAtlas atlas;
    int rec[2], map[2] = {439, 441};
    int n = 0, i, rec_i, loads_warm = -1, loads_end;
    size_t blob_warm = 0, always_warm = 0, ts_warm = 0;
    size_t blob_end = 0, always_end = 0, ts_end = 0, cpu_end = 0;
    unsigned t0, t1, swap_ms, swap_ms_max = 0;

    if (dink_fs_init() != 0 || world_load(&w) != 0) {
        fprintf(stderr, "FAIL world\n");
        return 1;
    }
    rec[0] = (int)w.loc[map[0]];
    rec[1] = (int)w.loc[map[1]];
    if (rec[0] < 1 || rec[1] < 1) {
        fprintf(stderr, "FAIL loc %d=%d %d=%d\n", map[0], rec[0], map[1],
                rec[1]);
        return 1;
    }
    scr = malloc(sizeof(*scr));
    seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
    g = edraw_gfx_alloc();
    if (scr == NULL || seqs == NULL || g == NULL ||
        ini_load(seqs, DINK_MAX_SEQ) != 0) {
        fprintf(stderr, "FAIL alloc/ini\n");
        free(scr);
        free(seqs);
        edraw_gfx_release(g);
        return 1;
    }
    memset(&atlas, 0, sizeof(atlas));
    for (i = 0; i < DINK_MEM_LEAK_N; i++) {
        rec_i = rec[i % 2];
        t0 = mem_now_ms();
        if (map_load_record(rec_i, scr) != 0) {
            fprintf(stderr, "FAIL map rec=%d i=%d\n", rec_i, i);
            tiles_free(&atlas);
            edraw_gfx_release(g);
            free(seqs);
            free(scr);
            return 1;
        }
        if (edraw_load_screen(scr->sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL edraw rec=%d i=%d\n", rec_i, i);
            tiles_free(&atlas);
            edraw_gfx_release(g);
            free(seqs);
            free(scr);
            return 1;
        }
        tiles_free(&atlas);
        memset(&atlas, 0, sizeof(atlas));
        if (tiles_build_atlas(scr, &atlas) != 0) {
            fprintf(stderr, "FAIL tiles rec=%d i=%d\n", rec_i, i);
            tiles_free(&atlas);
            edraw_gfx_release(g);
            free(seqs);
            free(scr);
            return 1;
        }
        t1 = mem_now_ms();
        swap_ms = t1 - t0;
        if (swap_ms > swap_ms_max) {
            swap_ms_max = swap_ms;
        }
        printf("swap_ms %u\n", swap_ms);
        mem_log("swap", edraw_cpu_bytes(g, n), n, tiles_cache_bytes(),
                tiles_cache_sheets());
        if (i + 1 == DINK_MEM_LEAK_WARM) {
            blob_warm = dink_blob_bytes();
            always_warm = residency_bytes_always();
            ts_warm = tiles_cache_bytes();
            loads_warm = ff_disc_loads();
        }
        if (i + 1 == DINK_MEM_LEAK_N) {
            blob_end = dink_blob_bytes();
            always_end = residency_bytes_always();
            ts_end = tiles_cache_bytes();
            cpu_end = edraw_cpu_bytes(g, n);
            loads_end = ff_disc_loads();
        }
    }
    tiles_free(&atlas);
    edraw_gfx_release(g);
    free(seqs);
    free(scr);
    if (loads_warm < 0) {
        fprintf(stderr, "FAIL no warm sample\n");
        return 1;
    }
    printf("leak 20 file_blob_delta=%ld always_delta=%ld ts_rgb_delta=%ld "
           "cpu_pixels_end=%u disc_loads_warm=%d disc_loads_end=%d "
           "swap_ms_max=%u cap=%u\n",
           (long)blob_end - (long)blob_warm, (long)always_end - (long)always_warm,
           (long)ts_end - (long)ts_warm, (unsigned)cpu_end, loads_warm,
           loads_end, swap_ms_max, (unsigned)DINK_MEM_LEAK_DELTA);
    if (labs_delta(blob_end, blob_warm) > (long)DINK_MEM_LEAK_DELTA) {
        fprintf(stderr, "FAIL file_blob climbed %u -> %u\n",
                (unsigned)blob_warm, (unsigned)blob_end);
        return 1;
    }
    if (labs_delta(always_end, always_warm) > (long)DINK_MEM_LEAK_DELTA) {
        fprintf(stderr, "FAIL always climbed %u -> %u\n",
                (unsigned)always_warm, (unsigned)always_end);
        return 1;
    }
    if (labs_delta(ts_end, ts_warm) > (long)DINK_MEM_LEAK_DELTA) {
        fprintf(stderr, "FAIL ts_rgb climbed %u -> %u\n", (unsigned)ts_warm,
                (unsigned)ts_end);
        return 1;
    }
    if (loads_end != loads_warm) {
        fprintf(stderr, "FAIL disc_loads climbed %d -> %d\n", loads_warm,
                loads_end);
        return 1;
    }
    printf("OK test_leak\n");
    return 0;
}
