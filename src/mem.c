/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mem.h"

#include "fs.h"
#include "residency.h"

#include <stdio.h>

static size_t g_peak;

void mem_note_peak(size_t n)
{
    if (n > g_peak) {
        g_peak = n;
    }
}

void mem_log(const char *tag, size_t cpu_pixels, int frames, size_t ts_rgb,
             int sheets)
{
    size_t blob, always, screen, prev, n;
    int i, nblob = 0;
    const char *rel;
    const char *where = tag != NULL ? tag : "?";

    blob = dink_blob_bytes();
    always = residency_bytes_always();
    screen = residency_bytes_screen();
    prev = residency_bytes_prev();
    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        nblob++;
    }
    mem_note_peak(blob + cpu_pixels + ts_rgb + (size_t)DINK_MEM_ATLAS_BSS);
    printf("mem %s file_blob=%u n=%d cpu_pixels=%u frames=%d ts_rgb=%u "
           "sheets=%d atlas_bss=%u peak=%u always=%u screen=%u prev=%u "
           "sticky_in_cpu=%u\n",
           where, (unsigned)blob, nblob, (unsigned)cpu_pixels, frames,
           (unsigned)ts_rgb, sheets, (unsigned)DINK_MEM_ATLAS_BSS,
           (unsigned)g_peak, (unsigned)always, (unsigned)screen,
           (unsigned)prev, (unsigned)cpu_pixels);
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
