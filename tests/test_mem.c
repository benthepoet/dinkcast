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
    /* Tilesheet slurp must drop Prev dir.ff the same way. Map 376
     * ts41 is 161 KB; village packs already sit on the cap. */
    {
        static const char *packs[] = {
            "graphics/struct/home/dir.ff",
            "graphics/effects/magic/dir.ff",
            "graphics/people/mom/dir.ff",
            "graphics/people/knight/silver/dir.ff",
            "graphics/people/girl/dir.ff",
            "graphics/struct/landmark/dir.ff",
            "graphics/people/oldman/dir.ff",
            "graphics/people/peasant2/dir.ff",
            "graphics/struct/outinn/dir.ff",
            "graphics/lands/trees/dir.ff",
            "graphics/struct/castle/dir.ff",
            NULL,
        };
        const uint8_t *ts;
        size_t tsn;
        int i, over = 0;

        for (i = 0; packs[i] != NULL; i++) {
            if (dink_blob_get(packs[i], &p, &n) == 0) {
                residency_touch(packs[i]);
            }
            if (dink_blob_bytes() + 161080u > (size_t)DINK_MEM_BLOB_PEAK) {
                over = 1;
                break;
            }
        }
        residency_swap_begin();
        before = dink_blob_bytes();
        if (before + 161080u <= (size_t)DINK_MEM_BLOB_PEAK) {
            fprintf(stderr, "FAIL ts41 fill too small have=%u over=%d\n",
                    (unsigned)before, over);
            return 1;
        }
        if (dink_blob_get("tiles/ts41.bmp", &ts, &tsn) != 0 || ts == NULL ||
            tsn < 54) {
            fprintf(stderr, "FAIL ts41 slurp have=%u over=%d\n",
                    (unsigned)dink_blob_bytes(), over);
            return 1;
        }
        if (dink_blob_bytes() > (size_t)DINK_MEM_BLOB_PEAK) {
            fprintf(stderr, "FAIL ts41 left blob %u over cap\n",
                    (unsigned)dink_blob_bytes());
            return 1;
        }
        if (over && before + 161080u > (size_t)DINK_MEM_BLOB_PEAK &&
            dink_blob_bytes() >= before) {
            fprintf(stderr, "FAIL ts41 did not drop Prev have=%u\n",
                    (unsigned)dink_blob_bytes());
            return 1;
        }
    }
    printf("OK test_mem\n");
    return 0;
}
