/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_PAD_H
#define DINKCAST_PAD_H

#include <stdint.h>

/* Portable bits used by host tests and the DC mapper. */
#define DINK_PAD_A (1u << 0)
#define DINK_PAD_B (1u << 6)
#define DINK_PAD_X (1u << 7)
#define DINK_PAD_Y (1u << 8)
#define DINK_PAD_L (1u << 9)
#define DINK_PAD_START (1u << 1)
#define DINK_PAD_LEAVE (DINK_PAD_A | DINK_PAD_START)
#define DINK_PAD_UP (1u << 2)
#define DINK_PAD_DOWN (1u << 3)
#define DINK_PAD_LEFT (1u << 4)
#define DINK_PAD_RIGHT (1u << 5)

/* Missing controller (have == 0) never leaves the title. */
int pad_title_wants_leave(int have_controller, uint32_t buttons);
/* Keypad dir 1-9, or 0 if no D-pad. */
int pad_dir_from_buttons(uint32_t buttons);
/* 1 if bit went 0→1 this poll. */
int pad_just_pressed(uint32_t prev, uint32_t now, uint32_t bit);

#ifdef _arch_dreamcast
/* Maple port 0. Returns 0 if a controller is present; -1 if missing. */
int pad_poll_port0(uint32_t *out_buttons);
#endif

#endif
