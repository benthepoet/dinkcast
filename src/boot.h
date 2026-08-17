/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_BOOT_H
#define DINKCAST_BOOT_H

/* Bite 0.2 color field — Dink-brown #5A3A1A, 640×480 RGB565. */
#define DINK_BOOT_WIDTH 640
#define DINK_BOOT_HEIGHT 480
#define DINK_BOOT_R 0x5A
#define DINK_BOOT_G 0x3A
#define DINK_BOOT_B 0x1A
#define DINK_BOOT_MSG "dinkcast boot ok"

#define DINK_RGB565(r, g, b) \
    ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

#define DINK_BOOT_RGB565 DINK_RGB565(DINK_BOOT_R, DINK_BOOT_G, DINK_BOOT_B)

enum GameState {
    GAME_STATE_TITLE = 0,
    GAME_STATE_LOADING = 1
};

#endif
