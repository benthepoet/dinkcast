/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKDAT_H
#define DINKCAST_DINKDAT_H

#include <stdint.h>

/* Bite 1.2: open dink.dat (case-insensitive), return byte size. */
int dink_dat_size(int64_t *bytes);

#endif
