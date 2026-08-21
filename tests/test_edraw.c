/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"
#include "ff.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "residency.h"
#include "start_map.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void seq_dir(const struct SeqInfo *seq, char *dir, size_t n)
{
    const char *sl;

    dir[0] = '\0';
    if (seq == NULL || seq->prefix[0] == '\0' || n < 8) {
        return;
    }
    sl = strrchr(seq->prefix, '/');
    if (sl == NULL) {
        return;
    }
    snprintf(dir, n, "%.*s/dir.ff", (int)(sl - seq->prefix), seq->prefix);
}

static int die_frames_ok(struct EdGfx *g, int n, struct SeqInfo *seqs)
{
    int nfr, f;

    if (seqs[164].prefix[0] == '\0') {
        return 1;
    }
    nfr = ini_seq_len(164, seqs[164].nframes);
    if (nfr < 2) {
        return 0;
    }
    for (f = 1; f <= nfr; f++) {
        if (edraw_find(g, n, 164, f) == NULL) {
            return 0;
        }
    }
    return 1;
}

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

        seq_dir(&seqs[164], dir, sizeof(dir));
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
            char dir[160];
            int loads1, loads2, nfr;

            seq_dir(&seqs[164], dir, sizeof(dir));
            if (edraw_load_screen(pig.sprite, seqs, g, &n, 0) != 0) {
                fprintf(stderr, "FAIL pig edraw\n");
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            nfr = ini_seq_len(164, seqs[164].nframes);
            if (seqs[164].prefix[0] != '\0' &&
                (!die_frames_ok(g, n, seqs) || nfr < 2)) {
                fprintf(stderr, "FAIL pig seq 164 incomplete nfr=%d\n", nfr);
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            if (dir[0] != '\0' && ff_is_cached(dir)) {
                fprintf(stderr, "FAIL pig left %s pinned\n", dir);
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            loads1 = ff_disc_loads();
            if (edraw_load_screen(scr.sprite, seqs, g, &n, 0) != 0) {
                fprintf(stderr, "FAIL house after pig\n");
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            loads2 = ff_disc_loads();
            if (loads2 != loads1) {
                fprintf(stderr, "FAIL house after pig reopened disc %d -> %d\n",
                        loads1, loads2);
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            if (seqs[164].prefix[0] != '\0' && !die_frames_ok(g, n, seqs)) {
                fprintf(stderr, "FAIL house dropped seq 164 after pig\n");
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            if (dir[0] != '\0' && ff_is_cached(dir)) {
                fprintf(stderr, "FAIL house recached %s\n", dir);
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
        }
    }
    {
        struct MapScreen duck;
        int drec = (int)w.loc[441];
        char dir[160];
        int loads1, loads2;

        seq_dir(&seqs[164], dir, sizeof(dir));
        if (drec < 1 || map_load_record(drec, &duck) != 0) {
            fprintf(stderr, "FAIL duck map loc=%d\n", drec);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        loads1 = ff_disc_loads();
        if (edraw_load_screen(duck.sprite, seqs, g, &n, 2) != 0) {
            fprintf(stderr, "FAIL duck edraw vis2\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[111].prefix[0] != '\0' && edraw_find(g, n, 111, 1) == NULL) {
            fprintf(stderr, "FAIL duck no headless seq 111\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[164].prefix[0] != '\0' && !die_frames_ok(g, n, seqs)) {
            fprintf(stderr, "FAIL duck dropped seq 164\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (dir[0] != '\0' && ff_is_cached(dir)) {
            fprintf(stderr, "FAIL duck recached %s\n", dir);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        loads2 = ff_disc_loads();
        if (loads2 <= loads1) {
            fprintf(stderr, "FAIL duck opened no new packs %d -> %d\n", loads1,
                    loads2);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
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
    {
        struct MapScreen a, b;
        char walls[160];
        int ra = (int)w.loc[439], rb = (int)w.loc[440];

        seq_dir(&seqs[31], walls, sizeof(walls));
        if (ra < 1 || rb < 1 || map_load_record(ra, &a) != 0 ||
            map_load_record(rb, &b) != 0) {
            fprintf(stderr, "FAIL 439/440 map\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(a.sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL 439 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(b.sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL 440 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (walls[0] != '\0' && ff_is_cached(walls)) {
            fprintf(stderr, "FAIL house walls still pinned after 439+440\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    {
        struct MapScreen ethel;
        int erec = (int)w.loc[2];

        if (erec < 1 || map_load_record(erec, &ethel) != 0) {
            fprintf(stderr, "FAIL ethel map loc=%d\n", erec);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(ethel.sprite, seqs, g, &n, 1) != 0) {
            fprintf(stderr, "FAIL ethel edraw vis1\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[117].prefix[0] != '\0' && edraw_find(g, n, 117, 1) == NULL) {
            fprintf(stderr, "FAIL ethel no duck death seq 117 (table full?)\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[123].prefix[0] != '\0' && edraw_find(g, n, 123, 1) == NULL) {
            fprintf(stderr, "FAIL ethel no flying-head seq 123\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        {
            int s, n2, nfill = n;
            char dir[160];

            for (s = 1; s < DINK_MAX_SEQ && nfill < DINK_EDGFX_MAX; s++) {
                if (seqs[s].prefix[0] == '\0' || residency_is_sticky_seq(s) ||
                    s == 63) {
                    continue;
                }
                if (strstr(seqs[s].prefix, "oldman") != NULL) {
                    continue;
                }
                seq_dir(&seqs[s], dir, sizeof(dir));
                if (dir[0] != '\0' && residency_is_always(dir)) {
                    continue;
                }
                edraw_load_seq(g, &nfill, seqs, s);
            }
            n = nfill;
            if (seqs[231].prefix[0] != '\0' &&
                edraw_find(g, n, 231, 2) == NULL) {
                seq_dir(&seqs[231], dir, sizeof(dir));
                if (dir[0] == '\0' || !ff_is_cached(dir)) {
                    fprintf(stderr, "FAIL 14.4c oldman pack not cached\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                n2 = n;
                if (edraw_ensure_frame(g, &n2, seqs, 231, 2) != 0 ||
                    edraw_find(g, n2, 231, 2) == NULL) {
                    fprintf(stderr, "FAIL 14.4c ensure screen class n=%d\n", n);
                    edraw_free(g, n2);
                    free(seqs);
                    return 1;
                }
                if (seqs[164].prefix[0] != '\0' &&
                    edraw_find(g, n2, 164, 1) == NULL) {
                    fprintf(stderr, "FAIL 14.4c evicted sticky 164\n");
                    edraw_free(g, n2);
                    free(seqs);
                    return 1;
                }
                if (seqs[117].prefix[0] != '\0' &&
                    edraw_find(g, n2, 117, 1) == NULL) {
                    fprintf(stderr, "FAIL 14.4c lost duck death 117\n");
                    edraw_free(g, n2);
                    free(seqs);
                    return 1;
                }
                if (seqs[123].prefix[0] != '\0' &&
                    edraw_find(g, n2, 123, 1) == NULL) {
                    fprintf(stderr, "FAIL 14.4c lost flying-head 123\n");
                    edraw_free(g, n2);
                    free(seqs);
                    return 1;
                }
                n = n2;
            }
        }
    }
    printf("edraw unique %d actives %d\n", n, act);
    edraw_free(g, n);
    free(seqs);
    printf("OK test_edraw\n");
    return 0;
}
