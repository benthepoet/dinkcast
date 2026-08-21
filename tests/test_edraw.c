/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"
#include "ff.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "start_map.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct World w;
    struct MapScreen scr;
    struct SeqInfo *seqs;
    struct EdGfx g[DINK_EDGFX_MAX];
    int rec, n = 0, act = 0, i;

    if (dink_fs_init() != 0 || world_load(&w) != 0) {
        fprintf(stderr, "FAIL world\n");
        return 1;
    }
    rec = (int)w.loc[DINK_START_PLAYER_MAP];
    if (map_load_record(rec, &scr) != 0) {
        fprintf(stderr, "FAIL map\n");
        return 1;
    }
    for (i = 1; i <= 100; i++) {
        if (scr.sprite[i].active) {
            act++;
        }
    }
    if (!editor_sprite_draw(&scr.sprite[1], DINK_VISION_DEFAULT) ||
        editor_sprite_draw(&scr.sprite[21], DINK_VISION_DEFAULT) ||
        editor_sprite_draw(&scr.sprite[25], DINK_VISION_DEFAULT) ||
        editor_sprite_draw(&scr.sprite[44], DINK_VISION_DEFAULT)) {
        fprintf(stderr, "FAIL vision/type filter\n");
        return 1;
    }
    if (editor_sprite_rank_y(&scr.sprite[51]) != 248 ||
        editor_sprite_rank_y(&scr.sprite[1]) != (int)scr.sprite[1].y) {
        fprintf(stderr, "FAIL que rank %d %d\n",
                editor_sprite_rank_y(&scr.sprite[51]),
                editor_sprite_rank_y(&scr.sprite[1]));
        return 1;
    }
    seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
    if (seqs == NULL || ini_load(seqs, DINK_MAX_SEQ) != 0) {
        fprintf(stderr, "FAIL ini\n");
        free(seqs);
        return 1;
    }
    if (edraw_load_screen(scr.sprite, seqs, g, &n, 0) != 0 || n < 17) {
        fprintf(stderr, "FAIL edraw n=%d act=%d\n", n, act);
        edraw_free(g, n);
        free(seqs);
        return 1;
    }
    if (edraw_find(g, n, 31, 22) == NULL && edraw_find(g, n, 31, 1) == NULL) {
        /* start house uses seq 31 planks; any 31 frame is enough */
        int has31 = 0;
        for (i = 0; i < n; i++) {
            if (g[i].seq == 31) {
                has31 = 1;
            }
        }
        if (!has31) {
            fprintf(stderr, "FAIL no seq 31 gfx\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    for (i = 0; i < n; i++) {
        if (g[i].seq == 70 || g[i].seq == 427 || g[i].seq == 158) {
            fprintf(stderr, "FAIL loaded fire/hole seq %d\n", g[i].seq);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    if (edraw_find(g, n, 86, 1) == NULL) {
        fprintf(stderr, "FAIL no fireplace seq 86\n");
        edraw_free(g, n);
        free(seqs);
        return 1;
    }
    if (edraw_find(g, n, 351, 1) == NULL || edraw_find(g, n, 353, 1) == NULL ||
        edraw_find(g, n, 357, 1) == NULL || edraw_find(g, n, 359, 1) == NULL) {
        fprintf(stderr, "FAIL no mom walk seq 351/353/357/359\n");
        edraw_free(g, n);
        free(seqs);
        return 1;
    }
    if (edraw_find(g, n, 164, 1) != NULL) {
        fprintf(stderr, "FAIL house loaded corpse seq 164 (mom hp)\n");
        edraw_free(g, n);
        free(seqs);
        return 1;
    }
    {
        char dir[160];
        const char *sl = strrchr(seqs[164].prefix, '/');

        dir[0] = '\0';
        if (sl != NULL) {
            snprintf(dir, sizeof(dir), "%.*s/dir.ff",
                     (int)(sl - seqs[164].prefix), seqs[164].prefix);
        }
        if (dir[0] != '\0' && ff_is_cached(dir)) {
            fprintf(stderr, "FAIL house pinned %s\n", dir);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    if (seqs[331].nframes != 0) {
        fprintf(stderr, "FAIL girl nframes already %d\n", seqs[331].nframes);
        edraw_free(g, n);
        free(seqs);
        return 1;
    }
    edraw_load_seq(g, &n, seqs, 331);
    if (edraw_find(g, n, 331, 1) == NULL) {
        fprintf(stderr, "FAIL create_sprite seq 331 frame 1\n");
        edraw_free(g, n);
        free(seqs);
        return 1;
    }
    {
        int loads1, n2 = n;

        loads1 = ff_disc_loads();
        if (edraw_ensure_frame(g, &n2, seqs, 93, 1) == 0) {
            fprintf(stderr, "FAIL ensure opened uncached pig pack\n");
            edraw_free(g, n2);
            free(seqs);
            return 1;
        }
        if (ff_disc_loads() != loads1) {
            fprintf(stderr, "FAIL ensure_frame disc %d -> %d\n", loads1,
                    ff_disc_loads());
            edraw_free(g, n2);
            free(seqs);
            return 1;
        }
    }
    {
        struct SpriteFrame *plank = edraw_find(g, n, 31, 22);

        if (plank != NULL && (plank->cx != 79 || plank->cy != 88)) {
            fprintf(stderr, "FAIL wall center %d %d\n", plank->cx, plank->cy);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    {
        struct MapScreen pig;
        int prec = (int)w.loc[407];

        if (prec < 1 || map_load_record(prec, &pig) != 0) {
            fprintf(stderr, "FAIL pig map loc=%d\n", prec);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (pig.sprite[23].seq != 93 || pig.sprite[23].alt_l != 128 ||
            pig.sprite[23].alt_r != 185 || pig.sprite[18].alt_r != 65 ||
            pig.sprite[62].type != DINK_SPR_TYPE_INVISIBLE) {
            fprintf(stderr, "FAIL pig alt/type 23=%d,%d 18r=%d t62=%d\n",
                    (int)pig.sprite[23].alt_l, (int)pig.sprite[23].alt_r,
                    (int)pig.sprite[18].alt_r, (int)pig.sprite[62].type);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (editor_sprite_draw(&pig.sprite[62], DINK_VISION_DEFAULT) ||
            editor_sprite_draw(&pig.sprite[60], DINK_VISION_DEFAULT)) {
            fprintf(stderr, "FAIL pig type2 drawn\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        {
            struct EdGfx *pg = edraw_gfx_alloc();
            int pn = 0;

            if (pg == NULL ||
                edraw_load_screen(pig.sprite, seqs, pg, &pn, 0) != 0) {
                fprintf(stderr, "FAIL pig edraw\n");
                edraw_gfx_release(pg);
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            if (seqs[164].prefix[0] != '\0' &&
                edraw_find(pg, pn, 164, 1) == NULL) {
                fprintf(stderr, "FAIL pig no seq 164 preload\n");
                edraw_gfx_release(pg);
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            {
                char dir[160];
                const char *sl = strrchr(seqs[164].prefix, '/');

                dir[0] = '\0';
                if (sl != NULL) {
                    snprintf(dir, sizeof(dir), "%.*s/dir.ff",
                             (int)(sl - seqs[164].prefix), seqs[164].prefix);
                }
                if (dir[0] != '\0' && ff_is_cached(dir)) {
                    fprintf(stderr, "FAIL pig left %s pinned\n", dir);
                    edraw_gfx_release(pg);
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
            }
            edraw_gfx_release(pg);
        }
    }
    {
        struct MapScreen duck;
        int drec = (int)w.loc[441];
        struct EdGfx *dg = edraw_gfx_alloc();
        int dn = 0;

        if (drec < 1 || map_load_record(drec, &duck) != 0 || dg == NULL) {
            fprintf(stderr, "FAIL duck map loc=%d\n", drec);
            edraw_gfx_release(dg);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(duck.sprite, seqs, dg, &dn, 2) != 0) {
            fprintf(stderr, "FAIL duck edraw vis2\n");
            edraw_gfx_release(dg);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[111].prefix[0] != '\0' && edraw_find(dg, dn, 111, 1) == NULL) {
            fprintf(stderr, "FAIL duck no headless seq 111\n");
            edraw_gfx_release(dg);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        edraw_gfx_release(dg);
    }
    {
        int loads1, loads2, n2 = n;

        loads1 = ff_disc_loads();
        if (edraw_load_screen(scr.sprite, seqs, g, &n2, 0) != 0) {
            fprintf(stderr, "FAIL edraw reload\n");
            edraw_free(g, n2);
            free(seqs);
            return 1;
        }
        loads2 = ff_disc_loads();
        if (loads2 != loads1) {
            fprintf(stderr, "FAIL edraw reuse disc %d -> %d\n", loads1, loads2);
            edraw_free(g, n2);
            free(seqs);
            return 1;
        }
        n = n2;
    }
    printf("edraw unique %d actives %d\n", n, act);
    edraw_free(g, n);
    free(seqs);
    printf("OK test_edraw\n");
    return 0;
}
