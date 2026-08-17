/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"
#include "world.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    struct World w;

    if (dink_fs_init() != 0) {
        fprintf(stderr, "no DINK_DATA / root\n");
        return 1;
    }
    if (world_load(&w) != 0) {
        fprintf(stderr, "world_load failed\n");
        return 1;
    }
    printf("loc_nonzero %d\n", world_loc_nonzero_count(&w));
    printf("loc[1] %d\n", (int)w.loc[1]);
    printf("music[1] %d\n", (int)w.music[1]);
    printf("indoor[1] %d\n", (int)w.indoor[1]);
    printf("max_loc %d\n", (int)world_max_loc(&w));
    return 0;
}
