/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PAD_H
#define DINKCAST_PAD_H

#include <stdint.h>

/* Portable bits used by host tests and the DC mapper. */
#define DINK_PAD_A (1u << 0)
#define DINK_PAD_START (1u << 1)
#define DINK_PAD_LEAVE (DINK_PAD_A | DINK_PAD_START)

/* Missing controller (have == 0) never leaves the title. */
int pad_title_wants_leave(int have_controller, uint32_t buttons);

#ifdef _arch_dreamcast
/* Maple port 0. Returns 0 if a controller is present; -1 if missing. */
int pad_poll_port0(uint32_t *out_buttons);
#endif

#endif
