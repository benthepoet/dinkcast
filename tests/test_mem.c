/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ff.h"
#include "fs.h"
#include "mem.h"
#include "residency.h"

#include <stdio.h>

int main(void)
{
    const uint8_t *p;
    size_t n, before, after;
    const char *pig = "graphics/animals/pig/dir.ff";

    mem_log("test", 100, 2, 50, 1);

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL fs\n");
        return 1;
    }
    residency_swap_begin();
    if (dink_blob_get(pig, &p, &n) != 0) {
        fprintf(stderr, "FAIL pig pack\n");
        return 1;
    }
    residency_touch(pig);
    residency_swap_begin();
    before = dink_blob_bytes();
    if (residency_drop_one_prev() != 0) {
        fprintf(stderr, "FAIL drop prev\n");
        return 1;
    }
    after = dink_blob_bytes();
    if (after >= before) {
        fprintf(stderr, "FAIL prev still %u\n", (unsigned)after);
        return 1;
    }
    /* make_room drops Prev before a slurp that would exceed the cap. */
    if (dink_blob_get(pig, &p, &n) != 0) {
        fprintf(stderr, "FAIL pig reload\n");
        return 1;
    }
    residency_touch(pig);
    residency_swap_begin();
    before = dink_blob_bytes();
    /* need just over the remaining cap so a Prev pack must go. */
    if (residency_make_room(DINK_MEM_BLOB_PEAK - before + 1) != 0) {
        fprintf(stderr, "FAIL make_room after drop have=%u\n",
                (unsigned)dink_blob_bytes());
        return 1;
    }
    after = dink_blob_bytes();
    if (after >= before) {
        fprintf(stderr, "FAIL make_room kept %u\n", (unsigned)after);
        return 1;
    }
    printf("OK test_mem\n");
    return 0;
}
