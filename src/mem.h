/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_MEM_H
#define DINKCAST_MEM_H

#include <stddef.h>

#define DINK_MEM_BLOB_PEAK (4718592u) /* 4.5 MiB */
#define DINK_MEM_CPU_PIXELS (2097152u) /* 2.0 MiB */
#define DINK_MEM_TS_RGB (1310720u) /* 1.25 MiB */
#define DINK_MEM_ATLAS_BSS (524288u) /* 512×512 RGB565 */

/* 14.4b: Always/Screen/Prev. mem refuse over file_blob cap during swap. */
void mem_note_peak(size_t n);
void mem_log(const char *tag, size_t cpu_pixels, int frames, size_t ts_rgb,
             int sheets);

#endif
