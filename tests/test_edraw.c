/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"
#include "ff.h"
#include "fs.h"
#include "ini.h"
#include "mapscr.h"
#include "mem.h"
#include "residency.h"
#include "start_map.h"
#include "tiles.h"
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
    /* Official pie table stays vis 0 / hard 0. Do not invent a vis-2 hide. */
    if (!scr.sprite[22].active || (int)scr.sprite[22].seq != 87 ||
        (int)scr.sprite[22].frame != 9 || (int)scr.sprite[22].type != 1 ||
        (int)scr.sprite[22].vision != 0 || (int)scr.sprite[22].hard != 0 ||
        (int)scr.sprite[22].x != 319 || (int)scr.sprite[22].y != 243) {
        fprintf(stderr, "FAIL pie table editor 22\n");
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
    /* Brain 6 current+next: do not slurp all 26 of 161 (1.7 MB). */
    if (!scr.sprite[33].active || (int)scr.sprite[33].seq != 161 ||
        (int)scr.sprite[33].frame != 4 || (int)scr.sprite[33].brain != 6 ||
        (int)scr.sprite[33].vision != 1 || !scr.sprite[41].active ||
        (int)scr.sprite[41].seq != 161 || (int)scr.sprite[41].frame != 14 ||
        (int)scr.sprite[41].brain != 6 || (int)scr.sprite[41].vision != 1) {
        fprintf(stderr, "FAIL house fire atomics not 161/4 and 161/14\n");
        edraw_free(g, n);
        free(seqs);
        return 1;
    }
    {
        int n161 = 0, nfr161, nxt4, nxt14;
        size_t pix;

        if (edraw_load_screen(scr.sprite, seqs, g, &n, 1) != 0) {
            fprintf(stderr, "FAIL house vis 1 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_find(g, n, 161, 4) == NULL ||
            edraw_find(g, n, 161, 14) == NULL) {
            fprintf(stderr, "FAIL house vis 1 missing 161/4 or 161/14\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        nxt4 = edraw_loop_next_frame(seqs, 161, 4);
        nxt14 = edraw_loop_next_frame(seqs, 161, 14);
        if (edraw_find(g, n, 161, nxt4) == NULL ||
            edraw_find(g, n, 161, nxt14) == NULL) {
            fprintf(stderr, "FAIL house vis 1 missing 161 next %d/%d\n", nxt4,
                    nxt14);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        for (i = 0; i < n; i++) {
            if (g[i].seq == 161) {
                n161++;
            }
        }
        nfr161 = ini_seq_len(161, seqs[161].nframes);
        if (nfr161 >= 10 && n161 >= nfr161) {
            fprintf(stderr, "FAIL house vis 1 loaded all %d of 161\n", n161);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        pix = edraw_cpu_bytes(g, n);
        if (pix >= DINK_MEM_CPU_PIXELS) {
            fprintf(stderr, "FAIL house vis 1 cpu_pixels %zu\n", pix);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(scr.sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL house vis 0 after fire current+next\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        {
            int has31 = 0;

            for (i = 0; i < n; i++) {
                if (g[i].seq == 31) {
                    has31 = 1;
                }
            }
            if (!has31) {
                fprintf(stderr, "FAIL house lost seq 31 after vis 1\n");
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
        }
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
        /* S1-MH-M create_sprite seq 56: play-path skip, load_frame opens. */
        n2 = n;
        if (edraw_ensure_frame(g, &n2, seqs, 56, 1) == 0) {
            fprintf(stderr, "FAIL ensure opened uncached bottle pack\n");
            edraw_free(g, n2);
            free(seqs);
            return 1;
        }
        edraw_load_frame(g, &n, seqs, 56, 1);
        if (edraw_find(g, n, 56, 1) == NULL) {
            fprintf(stderr, "FAIL load_frame seq 56 bottle\n");
            edraw_free(g, n);
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
            {
                struct MapScreen out;
                struct FfFile *ff = NULL;
                int orec = (int)w.loc[439];

                if (orec < 1 || map_load_record(orec, &out) != 0) {
                    fprintf(stderr, "FAIL 439 before pig\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                if (edraw_load_screen(out.sprite, seqs, g, &n, 0) != 0) {
                    fprintf(stderr, "FAIL 439 edraw before pig\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                (void)ff_cached("graphics/effects/seed/dir.ff", &ff);
                (void)ff_cached("graphics/dink/seed/dir.ff", &ff);
                residency_pin_always("graphics/effects/seed/dir.ff");
                residency_pin_always("graphics/dink/seed/dir.ff");
                if (edraw_load_screen(pig.sprite, seqs, g, &n, 0) != 0) {
                    fprintf(stderr, "FAIL pig edraw\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
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
            {
                int i, extra = 0, walk1 = 0;
                static const int pd[4] = {1, 3, 7, 9};

                if (seqs[187].prefix[0] != '\0' &&
                    edraw_find(g, n, 187, 1) == NULL) {
                    fprintf(stderr, "FAIL pig blood 187 not loaded\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                for (i = 1; i <= 100; i++) {
                    int base, d;

                    if ((int)pig.sprite[i].type != 1 ||
                        (int)pig.sprite[i].brain != 4) {
                        continue;
                    }
                    base = (int)pig.sprite[i].base_walk;
                    for (d = 0; d < 4; d++) {
                        int sq = base + pd[d];
                        int nfr;

                        if (sq < 1 || sq >= DINK_MAX_SEQ ||
                            seqs[sq].prefix[0] == '\0') {
                            continue;
                        }
                        if (edraw_find(g, n, sq, 1) != NULL) {
                            walk1++;
                        }
                        nfr = ini_seq_len(sq, seqs[sq].nframes);
                        if (nfr >= 2 && edraw_find(g, n, sq, 2) != NULL) {
                            extra++;
                        }
                    }
                }
                if (edraw_find(g, n, 41, 1) == NULL) {
                    fprintf(stderr, "FAIL pig seq 41 after house-439-407\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                if (walk1 < 1) {
                    fprintf(stderr, "FAIL pig walk frame 1 missing\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                if (extra > 0) {
                    fprintf(stderr, "FAIL pig dumped walk extras %d\n", extra);
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
            }
            {
                struct MapScreen out;
                int orec = (int)w.loc[439];

                if (orec < 1 || map_load_record(orec, &out) != 0) {
                    fprintf(stderr, "FAIL 439 after pig map\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                loads1 = ff_disc_loads();
                if (edraw_load_screen(out.sprite, seqs, g, &n, 0) != 0) {
                    fprintf(stderr, "FAIL 439 after pig\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                loads2 = ff_disc_loads();
                if (loads2 != loads1) {
                    fprintf(stderr, "FAIL 439 after pig reopened disc %d -> %d\n",
                            loads1, loads2);
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
            }
            if (edraw_load_screen(scr.sprite, seqs, g, &n, 0) != 0) {
                fprintf(stderr, "FAIL house after pig\n");
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
            if (edraw_find(g, n, 31, 22) == NULL &&
                edraw_find(g, n, 31, 1) == NULL) {
                int has31 = 0, hi;

                for (hi = 0; hi < n; hi++) {
                    if (g[hi].seq == 31) {
                        has31 = 1;
                    }
                }
                if (!has31) {
                    fprintf(stderr, "FAIL house seq 31 after pig (164 stole)\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
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
        if (seqs[231].prefix[0] != '\0' && edraw_find(g, n, 231, 1) == NULL) {
            fprintf(stderr, "FAIL ethel oldman 231 frame 1 missing\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        {
            int s, n2, nfill = n;
            char dir[160];

            for (s = 1; s < DINK_MAX_SEQ && nfill < DINK_EDGFX_MAX; s++) {
                if (seqs[s].prefix[0] == '\0' || residency_is_sticky_seq(s) ||
                    s == 63 || s == 173) {
                    continue;
                }
                if (strstr(seqs[s].prefix, "oldman") != NULL ||
                    strstr(seqs[s].prefix, "duck/death") != NULL) {
                    continue;
                }
                seq_dir(&seqs[s], dir, sizeof(dir));
                if (dir[0] == '\0' || !ff_is_cached(dir)) {
                    continue;
                }
                if (residency_is_always(dir)) {
                    continue;
                }
                edraw_load_frame(g, &nfill, seqs, s, 1);
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
            if (seqs[173].prefix[0] != '\0') {
                char bdir[160];
                int n2 = n, nfr;

                seq_dir(&seqs[173], bdir, sizeof(bdir));
                edraw_load_seq(g, &n, seqs, 173);
                if (bdir[0] == '\0' || !ff_is_cached(bdir)) {
                    fprintf(stderr, "FAIL 173 pack not cached after full table\n");
                    edraw_free(g, n);
                    free(seqs);
                    return 1;
                }
                nfr = ini_seq_len(173, seqs[173].nframes);
                if (nfr < 2) {
                    nfr = 6;
                }
                if (edraw_ensure_frame(g, &n2, seqs, 173, nfr) != 0 ||
                    edraw_find(g, n2, 173, nfr) == NULL) {
                    fprintf(stderr, "FAIL 173 smash ensure fr=%d n=%d\n", nfr, n);
                    edraw_free(g, n2);
                    free(seqs);
                    return 1;
                }
                n = n2;
            }
        }
        {
            struct MapScreen out;
            int orec = (int)w.loc[439];

            if (orec < 1 || map_load_record(orec, &out) != 0) {
                fprintf(stderr, "FAIL 439 after ethel\n");
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
            if (edraw_load_screen(out.sprite, seqs, g, &n, 0) != 0) {
                fprintf(stderr, "FAIL 439 edraw after ethel\n");
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
        }
        if (edraw_load_screen(scr.sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL house after ethel outdoor\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_find(g, n, 31, 22) == NULL &&
            edraw_find(g, n, 31, 1) == NULL) {
            int has31 = 0, hi;

            for (hi = 0; hi < n; hi++) {
                if (g[hi].seq == 31) {
                    has31 = 1;
                }
            }
            if (!has31) {
                fprintf(stderr, "FAIL house seq 31 after ethel\n");
                edraw_free(g, n);
                free(seqs);
                return 1;
            }
        }
        if (seqs[164].prefix[0] != '\0' && !die_frames_ok(g, n, seqs)) {
            fprintf(stderr, "FAIL house dropped seq 164 after ethel\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    {
        struct MapScreen gate;
        int grec = (int)w.loc[408];

        if (grec < 1 || map_load_record(grec, &gate) != 0) {
            fprintf(stderr, "FAIL 408 gate map\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(gate.sprite, seqs, g, &n, 1) != 0) {
            fprintf(stderr, "FAIL 408 vis1 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[293].prefix[0] != '\0' && edraw_find(g, n, 293, 1) == NULL) {
            fprintf(stderr, "FAIL 408 vis1 no guard seq 293\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    /* 408→376 is the play path after Load (no 409 in between). ts41
     * path + seq 32 trees must survive village file_blob. */
    {
        struct MapScreen wiz;
        struct TileAtlas atlas;
        int wrec = (int)w.loc[376];

        if (wrec < 1 || map_load_record(wrec, &wiz) != 0) {
            fprintf(stderr, "FAIL 376 after 408 map\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(wiz.sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL 376 after 408 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[32].prefix[0] != '\0' && edraw_find(g, n, 32, 1) == NULL &&
            edraw_find(g, n, 32, 9) == NULL &&
            edraw_find(g, n, 32, 2) == NULL) {
            fprintf(stderr, "FAIL 376 after 408 no tree seq 32\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        memset(&atlas, 0, sizeof(atlas));
        if (tiles_build_atlas(&wiz, &atlas) != 0 ||
            atlas.used != DINK_SCREEN_TILES) {
            fprintf(stderr, "FAIL 376 after 408 atlas used=%d\n", atlas.used);
            tiles_free(&atlas);
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        tiles_free(&atlas);
    }
    {
        struct MapScreen ethel_out;
        int erec = (int)w.loc[409];

        if (erec < 1 || map_load_record(erec, &ethel_out) != 0) {
            fprintf(stderr, "FAIL 409 ethel outdoor map\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(ethel_out.sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL 409 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[63].prefix[0] != '\0' && edraw_find(g, n, 63, 6) == NULL &&
            edraw_find(g, n, 63, 1) == NULL) {
            fprintf(stderr, "FAIL 409 no house seq 63 after 408\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    if (seqs[167].prefix[0] != '\0' && seqs[563].prefix[0] != '\0') {
        struct MapScreen wiz;
        int wrec = (int)w.loc[376];
        int f, n2, unused167 = 0, i, have167;

        if (wrec < 1 || map_load_record(wrec, &wiz) != 0) {
            fprintf(stderr, "FAIL 376 wizard map\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_load_screen(wiz.sprite, seqs, g, &n, 0) != 0) {
            fprintf(stderr, "FAIL 376 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        edraw_load_frame(g, &n, seqs, 167, 1);
        for (f = 2; f <= 14; f++) {
            (void)edraw_ensure_frame(g, &n, seqs, 167, f);
        }
        edraw_load_frame(g, &n, seqs, 563, 1);
        have167 = edraw_find(g, n, 167, 1) != NULL;
        edraw_live_begin(g, n, seqs);
        edraw_live_touch(g, n, 563, 1);
        for (i = 0; i < n; i++) {
            if (g[i].seq == 167 && !g[i].live) {
                unused167++;
            }
        }
        if (have167 && unused167 < 1) {
            fprintf(stderr, "FAIL wizard 167 still live after begin\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (have167) {
            edraw_live_touch(g, n, 167, 1);
        }
        n2 = n;
        if (edraw_ensure_frame(g, &n2, seqs, 563, 6) != 0 ||
            edraw_find(g, n2, 563, 6) == NULL) {
            fprintf(stderr, "FAIL wizard 563/6 after Screen live remake\n");
            edraw_free(g, n2);
            free(seqs);
            return 1;
        }
        if (have167 && edraw_find(g, n2, 167, 1) == NULL) {
            fprintf(stderr, "FAIL wizard evicted live explode 167/1\n");
            edraw_free(g, n2);
            free(seqs);
            return 1;
        }
        n = n2;
    }
    /* 14.4c created crowd: remake Screen live then load_frame so unused
     * fire frames can evict for create_sprite (not a seq-id pin). */
    if (seqs[157].prefix[0] != '\0' && seqs[221].prefix[0] != '\0') {
        struct MapScreen yard;
        int yrec = (int)w.loc[439];
        int f, unused_fire = 0, have_fire;

        if (yrec < 1 || map_load_record(yrec, &yard) != 0) {
            fprintf(stderr, "FAIL 439 fire-crowd map\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        /* S1-H1-O create_sprite 221/5 and 257/2 — mark before load so
         * maiden/blue fopen happens while Prev still has slack. */
        edraw_mark_need(221, 5);
        if (seqs[257].prefix[0] != '\0') {
            edraw_mark_need(257, 2);
        }
        if (edraw_load_screen(yard.sprite, seqs, g, &n, 1) != 0) {
            fprintf(stderr, "FAIL 439 vis 1 edraw\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (edraw_find(g, n, 221, 5) == NULL &&
            edraw_find(g, n, 221, 1) == NULL) {
            fprintf(stderr, "FAIL crowd 221 after mark-first\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (seqs[257].prefix[0] != '\0' && edraw_find(g, n, 257, 2) == NULL &&
            edraw_find(g, n, 257, 1) == NULL) {
            fprintf(stderr, "FAIL crowd 257 blue maiden after mark-first\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        for (f = 1; f <= 12; f++) {
            edraw_load_frame(g, &n, seqs, 157, f);
        }
        have_fire = edraw_find(g, n, 157, 2) != NULL;
        edraw_live_begin(g, n, seqs);
        edraw_live_touch(g, n, 157, 1);
        for (i = 0; i < n; i++) {
            if (g[i].seq == 157 && g[i].frame != 1 && !g[i].live) {
                unused_fire++;
            }
        }
        if (have_fire && unused_fire < 1) {
            fprintf(stderr, "FAIL fire 157 still live after begin\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        edraw_load_frame(g, &n, seqs, 221, 1);
        if (edraw_find(g, n, 221, 1) == NULL) {
            fprintf(stderr, "FAIL crowd 221/1 after Screen live remake\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
        if (have_fire && edraw_find(g, n, 157, 1) == NULL) {
            fprintf(stderr, "FAIL evicted live fire 157/1\n");
            edraw_free(g, n);
            free(seqs);
            return 1;
        }
    }
    printf("edraw unique %d actives %d\n", n, act);
    edraw_free(g, n);
    free(seqs);
    printf("OK test_edraw\n");
    return 0;
}
