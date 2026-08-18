/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "edraw.h"
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
    if (edraw_load_screen(scr.sprite, seqs, g, &n) != 0 || n < 8) {
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
    {
        struct SpriteFrame *plank = edraw_find(g, n, 31, 22);

        if (plank != NULL && (plank->cx != 79 || plank->cy != 88)) {
            fprintf(stderr, "FAIL wall center %d %d\n", plank->cx, plank->cy);
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
