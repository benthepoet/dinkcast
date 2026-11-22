/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_MEM_H
#define DINKCAST_MEM_H

#include <stddef.h>

#define DINK_MEM_BLOB_PEAK (4718592u) /* 4.5 MiB */
#define DINK_MEM_CPU_PIXELS (2097152u) /* 2.0 MiB */
#define DINK_MEM_TS_RGB (1310720u) /* 1.25 MiB */
#define DINK_MEM_ATLAS_BSS (262144u) /* 512×512 PAL8 (RGB565 fallback allocates) */
/* Plan §1.2 hard_cache: this screen's hard.dat tile records (<= 96). */
#define DINK_MEM_HARD_CACHE (262144u) /* 256 KiB */
/* 14.3: stable-pool delta after a warm two-screen ping-pong. */
#define DINK_MEM_LEAK_DELTA (4096u)
#define DINK_MEM_LEAK_WARM (4)
#define DINK_MEM_LEAK_N (20)

/* 14.4b: Always/Screen/Prev. mem refuse over file_blob cap during swap. */
void mem_note_peak(size_t n);
/* Tiles: the kept RGB565 fallback atlas (512 KB) once allocated. */
void mem_set_atlas_extra(size_t n);
/* hard.dat tile records held (hard_cache_bytes). Set before mem_log. */
void mem_set_hard_cache(size_t n);
/* Diagnostic: "vram <tag> free=N used=D" where D is the drop since the
 * previous note (which upload owns which VRAM). */
void mem_vram_note(const char *tag);
void mem_log(const char *tag, size_t cpu_pixels, int frames, size_t ts_rgb,
             int sheets);
/* Milliseconds. Host CLOCK_MONOTONIC; Dreamcast timer_ms_gettime64. */
unsigned mem_now_ms(void);
/* Title restart: 14.3 swap_n / warm snapshot so the next 20 crossings log. */
void mem_swap_reset(void);

#endif
