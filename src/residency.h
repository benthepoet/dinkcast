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

size_t residency_bytes_always(void);
size_t residency_bytes_screen(void);
size_t residency_bytes_prev(void);

#endif
