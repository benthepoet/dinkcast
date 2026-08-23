/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_RESIDENCY_H
#define DINKCAST_RESIDENCY_H

#include <stddef.h>

/* §1.2.1 Always: named prefixes, never evict. Not a size threshold. */
int residency_is_always(const char *rel);
/* Seq 164 explode pixels stay after the pack is dropped. */
int residency_is_sticky_seq(int seq);
/* 1 while between swap_begin and swap_end (play load may refuse over cap). */
int residency_swap_open(void);

void residency_swap_begin(void);
void residency_touch(const char *rel);
/* Drop packs that are neither Always, Screen, nor Prev. */
void residency_swap_end(void);
/* 14.4b: drop the largest Prev dir.ff (not tiles / hard.dat).
 * Screen sprite-pack or tilesheet fopen uses this when Always+Screen+Prev
 * would exceed file_blob. */
int residency_drop_one_prev(void);
/* Drop Prev dir.ff until file_blob + need fits the cap. 0 if room. */
int residency_make_room(size_t need);

/* Force Always until unpin. Named Always prefixes ignore unpin. */
void residency_pin_always(const char *rel);
void residency_unpin(const char *rel);

size_t residency_bytes_always(void);
size_t residency_bytes_screen(void);
size_t residency_bytes_prev(void);

#endif
