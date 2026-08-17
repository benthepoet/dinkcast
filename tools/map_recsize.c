/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"
#include "mapscr.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    FILE *fp;
    long sz;
    int recs, rem;
    struct World w;

    if (dink_fs_init() != 0) {
        fprintf(stderr, "no DINK_DATA / root\n");
        return 1;
    }
    fp = dink_fopen("map.dat", "rb");
    if (fp == NULL) {
        fprintf(stderr, "map.dat missing\n");
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }
    sz = ftell(fp);
    fclose(fp);
    if (map_file_records(sz, &recs, &rem) != 0) {
        return 1;
    }
    printf("file_size %ld\n", sz);
    printf("record_size %d\n", DINK_MAP_RECSIZE);
    printf("records %d remainder %d\n", recs, rem);
    if (rem != 0) {
        fprintf(stderr, "map.dat trailer %d (expected 0)\n", rem);
        return 1;
    }
    if (world_load(&w) != 0) {
        return 1;
    }
    printf("max_loc %d\n", (int)world_max_loc(&w));
    if (world_max_loc(&w) > recs) {
        fprintf(stderr, "max loc exceeds record count\n");
        return 1;
    }
    return 0;
}
