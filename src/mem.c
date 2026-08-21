/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mem.h"

#include "fs.h"

#include <stdio.h>
#include <string.h>

static size_t g_peak;

static int always_rel(const char *rel)
{
    static const char *k[] = {
        "graphics/dink/idle/",
        "graphics/dink/walk/",
        "graphics/dink/push/",
        "graphics/dink/hit/",
        "graphics/inter/text-box/",
        "graphics/inter/arrow/",
        "dink.ini",
        "dink.dat",
        NULL,
    };
    int i;

    if (rel == NULL || rel[0] == '\0') {
        return 0;
    }
    for (i = 0; k[i] != NULL; i++) {
        if (strncmp(rel, k[i], strlen(k[i])) == 0 || strcmp(rel, k[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void mem_note_peak(size_t n)
{
    if (n > g_peak) {
        g_peak = n;
    }
}

void mem_log(const char *tag, size_t cpu_pixels, int frames, size_t ts_rgb,
             int sheets)
{
    size_t blob, always = 0, screen = 0, n;
    int i, nblob = 0;
    const char *rel;
    const char *where = tag != NULL ? tag : "?";

    blob = dink_blob_bytes();
    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        nblob++;
        if (always_rel(rel)) {
            always += n;
        } else {
            screen += n;
        }
    }
    mem_note_peak(blob + cpu_pixels + ts_rgb + (size_t)DINK_MEM_ATLAS_BSS);
    printf("mem %s file_blob=%u n=%d cpu_pixels=%u frames=%d ts_rgb=%u "
           "sheets=%d atlas_bss=%u peak=%u always=%u screen=%u prev=0 "
           "sticky_in_cpu=%u\n",
           where, (unsigned)blob, nblob, (unsigned)cpu_pixels, frames,
           (unsigned)ts_rgb, sheets, (unsigned)DINK_MEM_ATLAS_BSS,
           (unsigned)g_peak, (unsigned)always, (unsigned)screen,
           (unsigned)cpu_pixels);
    if (blob > (size_t)DINK_MEM_BLOB_PEAK) {
        printf("14.5: needed pool=file_blob bytes=%u cap=%u\n", (unsigned)blob,
               (unsigned)DINK_MEM_BLOB_PEAK);
    }
    if (cpu_pixels > (size_t)DINK_MEM_CPU_PIXELS) {
        printf("14.5: needed pool=cpu_pixels bytes=%u cap=%u\n",
               (unsigned)cpu_pixels, (unsigned)DINK_MEM_CPU_PIXELS);
    }
    if (ts_rgb > (size_t)DINK_MEM_TS_RGB) {
        printf("14.5: needed pool=ts_rgb bytes=%u cap=%u\n", (unsigned)ts_rgb,
               (unsigned)DINK_MEM_TS_RGB);
    }
}
