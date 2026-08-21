/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mem.h"

#include "fs.h"
#include "residency.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _arch_dreamcast
#include <arch/timer.h>
#include <dc/pvr.h>
#endif

static size_t g_peak;
static int g_swap_n;
static size_t g_warm_blob;
static size_t g_warm_always;
static size_t g_warm_vram;

unsigned mem_now_ms(void)
{
#ifdef _arch_dreamcast
    return (unsigned)timer_ms_gettime64();
#else
    struct timespec t;

    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        return 0;
    }
    return (unsigned)((unsigned long)t.tv_sec * 1000ul +
                      (unsigned long)t.tv_nsec / 1000000ul);
#endif
}

void mem_note_peak(size_t n)
{
    if (n > g_peak) {
        g_peak = n;
    }
}

static size_t vram_free_now(void)
{
#ifdef _arch_dreamcast
    return (size_t)pvr_mem_available();
#else
    return 0;
#endif
}

void mem_log(const char *tag, size_t cpu_pixels, int frames, size_t ts_rgb,
             int sheets)
{
    size_t blob, always, screen, prev, n, vram;
    int i, nblob = 0;
    const char *rel;
    const char *where = tag != NULL ? tag : "?";

    blob = dink_blob_bytes();
    always = residency_bytes_always();
    screen = residency_bytes_screen();
    prev = residency_bytes_prev();
    vram = vram_free_now();
    for (i = 0; dink_blob_slot(i, &rel, &n) == 0; i++) {
        nblob++;
    }
    mem_note_peak(blob + cpu_pixels + ts_rgb + (size_t)DINK_MEM_ATLAS_BSS);
    if (strcmp(where, "swap") == 0) {
        g_swap_n++;
        if (g_swap_n == DINK_MEM_LEAK_WARM) {
            g_warm_blob = blob;
            g_warm_always = always;
            g_warm_vram = vram;
        }
    }
    printf("mem %s file_blob=%u n=%d cpu_pixels=%u frames=%d ts_rgb=%u "
           "sheets=%d atlas_bss=%u peak=%u always=%u screen=%u prev=%u "
           "sticky_in_cpu=%u vram_free=%u swap_n=%d\n",
           where, (unsigned)blob, nblob, (unsigned)cpu_pixels, frames,
           (unsigned)ts_rgb, sheets, (unsigned)DINK_MEM_ATLAS_BSS,
           (unsigned)g_peak, (unsigned)always, (unsigned)screen,
           (unsigned)prev, (unsigned)cpu_pixels, (unsigned)vram, g_swap_n);
    if (strcmp(where, "swap") == 0 && g_swap_n == DINK_MEM_LEAK_N) {
        long db = (long)blob - (long)g_warm_blob;
        long da = (long)always - (long)g_warm_always;
        long dv = (long)vram - (long)g_warm_vram;

        printf("leak 20 file_blob_delta=%ld always_delta=%ld "
               "vram_free_delta=%ld cap=%u (from swap %d)\n",
               db, da, dv, (unsigned)DINK_MEM_LEAK_DELTA, DINK_MEM_LEAK_WARM);
    }
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
