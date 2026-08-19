/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"
#include "ff.h"
#include "fs.h"
#include "hard.h"
#include "ini.h"
#include "mapscr.h"
#include "tiles.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const uint8_t *p = NULL;
    size_t n = 0;
    int o0, o1, o2;

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    {
        /* 64-slot overflow must not free a borrowed dir.ff (ff_load_rel). */
        struct FfFile ff;
        const uint8_t *bp = NULL, *fp = NULL;
        size_t bn = 0, fln = 0, sn;
        uint8_t snap[32];
        int i, uniq, o_hold;
        char rel[64];
        /* Pinned tiles/packs only — smaller story files would be
         * evicted first and hide a borrowed-dir.ff free. */
        static const char *const extra[] = {
            "hard.dat",
            "graphics/struct/Castle/dir.ff",
            "graphics/lands/Trees/Treefire/dir.ff",
            "graphics/foes/Dragon/dir.ff",
            "graphics/inter/Menu/dir.ff",
            "graphics/People/Boatman/dir.ff",
            "graphics/Effects/splode/dir.ff",
            "graphics/struct/outinn/dir.ff",
            "graphics/struct/Home/dir.ff",
            "graphics/foes/Slayers/Walk/dir.ff",
            "graphics/foes/goblin/Soldier/attack/dir.ff",
            "graphics/Effects/Magic/dir.ff",
            "graphics/Dink/sword/walk/dir.ff",
            "graphics/People/Knight/Blue/dir.ff",
            "graphics/foes/goblin/Horns/walk/dir.ff",
            "graphics/Dink/bow/walk/dir.ff",
            "graphics/People/Mom/dir.ff",
            "graphics/foes/goblin/Soldier/walk/dir.ff",
            "graphics/People/Knight/Gold/dir.ff",
            "graphics/People/Knight/Red/dir.ff",
            "graphics/struct/Island/dir.ff",
            "graphics/Dink/bow/hit/dir.ff",
            "graphics/foes/Stonegnt/Attack/dir.ff",
            "graphics/lands/Trees/dir.ff",
        };

        memset(&ff, 0, sizeof(ff));
        if (ff_load_rel("graphics/Effects/Shadows/dir.ff", &ff) != 0) {
            fprintf(stderr, "FAIL small pack\n");
            return 1;
        }
        if (ff_find(&ff, "shadw-01.bmp", &fp, &fln) != 0 || fp == NULL ||
            fln < 54) {
            fprintf(stderr, "FAIL small pack find\n");
            ff_free(&ff);
            return 1;
        }
        sn = fln < sizeof(snap) ? fln : sizeof(snap);
        memcpy(snap, fp, sn);
        uniq = 1;
        for (i = 1; i <= 41; i++) {
            snprintf(rel, sizeof(rel), "tiles/ts%02d.bmp", i);
            if (dink_blob_get(rel, &bp, &bn) == 0 && bp != NULL && bn >= 54) {
                uniq++;
            }
        }
        for (i = 0; i < (int)(sizeof extra / sizeof extra[0]); i++) {
            if (dink_blob_get(extra[i], &bp, &bn) == 0 && bp != NULL &&
                bn > 0) {
                uniq++;
            }
        }
        if (uniq < 65) {
            fprintf(stderr, "FAIL unique blobs %d want >=65\n", uniq);
            ff_free(&ff);
            return 1;
        }
        if (ff_find(&ff, "shadw-01.bmp", &fp, &fln) != 0 || fln < sn ||
            memcmp(fp, snap, sn) != 0) {
            fprintf(stderr, "FAIL early pack UAF/corrupt\n");
            ff_free(&ff);
            return 1;
        }
        o_hold = dink_disc_opens();
        if (dink_blob_get("graphics/Effects/Shadows/dir.ff", &bp, &bn) != 0 ||
            bp != ff.data || bn != ff.n) {
            fprintf(stderr, "FAIL early pack blob after overflow\n");
            ff_free(&ff);
            return 1;
        }
        if (dink_disc_opens() != o_hold) {
            fprintf(stderr, "FAIL early pack reopened %d -> %d\n", o_hold,
                    dink_disc_opens());
            ff_free(&ff);
            return 1;
        }
        printf("blob unique %d early pack hold opens %d\n", uniq, o_hold);
        ff_free(&ff);
    }
    o0 = dink_disc_opens();
    if (dink_blob_get("graphics/dink/idle/dir.ff", &p, &n) != 0 || p == NULL ||
        n < 54) {
        fprintf(stderr, "FAIL idle blob\n");
        return 1;
    }
    printf("ff ok graphics/dink/idle/dir.ff %u\n", (unsigned)n);
    if (dink_blob_get("graphics/dink/walk/dir.ff", &p, &n) != 0 || p == NULL ||
        n < 54) {
        fprintf(stderr, "FAIL walk blob\n");
        return 1;
    }
    printf("ff ok graphics/dink/walk/dir.ff %u\n", (unsigned)n);
    if (dink_blob_get("graphics/dink/idle/dir.ff", &p, &n) != 0) {
        fprintf(stderr, "FAIL idle repeat\n");
        return 1;
    }
    o1 = dink_disc_opens();
    if (o1 - o0 != 2) {
        fprintf(stderr, "FAIL disc_opens idle/walk %d -> %d want +2\n", o0, o1);
        return 1;
    }
    {
        struct World w;
        struct MapScreen scr;
        int rec;

        if (world_load(&w) != 0) {
            fprintf(stderr, "FAIL world\n");
            return 1;
        }
        rec = (int)w.loc[1];
        o0 = dink_disc_opens();
        if (rec < 1 || map_load_record(rec, &scr) != 0) {
            fprintf(stderr, "FAIL map1\n");
            return 1;
        }
        if (map_load_record(rec, &scr) != 0) {
            fprintf(stderr, "FAIL map1 again\n");
            return 1;
        }
        o1 = dink_disc_opens();
        if (o1 - o0 != 1) {
            fprintf(stderr, "FAIL map.dat opens %d want 1\n", o1 - o0);
            return 1;
        }
        {
            struct TileAtlas a;
            int opens;

            memset(&a, 0, sizeof(a));
            if (tiles_build_atlas(&scr, &a) != 0 ||
                a.used != DINK_SCREEN_TILES || a.rgb565 == NULL) {
                fprintf(stderr, "FAIL atlas used=%d\n", a.used);
                return 1;
            }
            opens = dink_disc_opens();
            tiles_free(&a);
            memset(&a, 0, sizeof(a));
            if (tiles_build_atlas(&scr, &a) != 0 ||
                a.used != DINK_SCREEN_TILES) {
                fprintf(stderr, "FAIL atlas rebuild\n");
                return 1;
            }
            if (dink_disc_opens() != opens) {
                fprintf(stderr, "FAIL atlas reopen %d -> %d\n", opens,
                        dink_disc_opens());
                tiles_free(&a);
                return 1;
            }
            printf("atlas used %d disc_opens_hold %d\n", a.used, opens);
            tiles_free(&a);
        }
        {
            struct HardMap hm;
            struct HardMask mask;
            int opens;

            memset(&hm, 0, sizeof(hm));
            memset(&mask, 0, sizeof(mask));
            if (hard_load(&hm) != 0 || !hm.ready) {
                fprintf(stderr, "FAIL hard_load\n");
                return 1;
            }
            opens = dink_disc_opens();
            if (hard_load(&hm) != 0 || dink_disc_opens() != opens) {
                fprintf(stderr, "FAIL hard reopen %d -> %d\n", opens,
                        dink_disc_opens());
                return 1;
            }
            if (hard_stamp_tiles(&hm, &scr, &mask) != 0) {
                fprintf(stderr, "FAIL hard stamp\n");
                hard_mask_free(&mask);
                return 1;
            }
            if (hard_stamp_tiles(&hm, &scr, &mask) != 0 ||
                dink_disc_opens() != opens) {
                fprintf(stderr, "FAIL hard stamp reopen %d -> %d\n", opens,
                        dink_disc_opens());
                hard_mask_free(&mask);
                return 1;
            }
            printf("hard stamp disc_opens_hold %d\n", opens);
            hard_mask_free(&mask);
        }
        {
            struct SeqInfo *seqs;
            struct EdGfx *g;
            int ned = 0;

            seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
            g = edraw_gfx_alloc();
            if (seqs == NULL || g == NULL || ini_load(seqs, DINK_MAX_SEQ) != 0) {
                fprintf(stderr, "FAIL ini/edg\n");
                edraw_gfx_release(g);
                free(seqs);
                return 1;
            }
            if (edraw_load_screen(scr.sprite, seqs, g, &ned) != 0 || ned < 1) {
                fprintf(stderr, "FAIL edraw n=%d\n", ned);
                edraw_gfx_release(g);
                free(seqs);
                return 1;
            }
            o0 = dink_disc_opens();
            if (edraw_load_screen(scr.sprite, seqs, g, &ned) != 0) {
                fprintf(stderr, "FAIL edraw again\n");
                edraw_gfx_release(g);
                free(seqs);
                return 1;
            }
            o1 = dink_disc_opens();
            if (o1 != o0) {
                fprintf(stderr, "FAIL edraw repeat opens %d -> %d\n", o0, o1);
                edraw_gfx_release(g);
                free(seqs);
                return 1;
            }
            o2 = ned;
            printf("edraw unique %d disc_opens_hold %d\n", o2, o1);
            edraw_gfx_release(g);
            free(seqs);
        }
    }
    printf("disc_opens total %d\n", dink_disc_opens());
    printf("OK test_io_once\n");
    return 0;
}
