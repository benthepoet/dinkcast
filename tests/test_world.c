/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "le.h"
#include "mapscr.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *m)
{
    fprintf(stderr, "FAIL %s\n", m);
    exit(1);
}

int main(void)
{
    uint8_t buf[DINK_DAT_SIZE];
    struct World w;
    int recs, rem;
    int s0, c;

    memset(buf, 0, sizeof(buf));
    memcpy(buf, "Smallwood", 9);
    /* loc[1]=42, music[1]=7, indoor[1]=1 via LE writes */
    buf[DINK_DAT_IDENT + 4] = 42;
    buf[DINK_DAT_IDENT + DINK_WORLD_SLOTS * 4 + 4] = 7;
    buf[DINK_DAT_IDENT + DINK_WORLD_SLOTS * 8 + 4] = 1;
    if (world_parse_mem(buf, sizeof(buf), &w) != 0) {
        die("parse");
    }
    if (w.loc[1] != 42 || w.music[1] != 7 || w.indoor[1] != 1) {
        die("slot1");
    }
    if (world_loc_nonzero_count(&w) != 1 || world_max_loc(&w) != 42) {
        die("counts");
    }
    if (map_file_records(31280L * 644, &recs, &rem) != 0 || recs != 644 ||
        rem != 0) {
        die("recsize");
    }
    tile_split(128 + 3, &s0, &c);
    if (s0 != 1 || c != 3) {
        die("tile_split");
    }
    tile_split(0, &s0, &c);
    if (s0 != 0 || c != 0) {
        die("tile0");
    }
    printf("OK test_world\n");
    return 0;
}
