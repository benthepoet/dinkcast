/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_SAVE_H
#define DINKCAST_SAVE_H

#include <stddef.h>
#include <stdint.h>

struct Player;

#define DINK_SAVE_SLOTS 10
#define DINK_SAVE_MAX 8192
#define DINK_SAVE_VERSION 1u
#define DINK_SAVE_MAGIC 0x76534344u /* 'DCsv' LE */

/* Plan 17.1 compact blob. Not a PC savegame file. */
int save_pack(uint8_t *dst, size_t cap, size_t *n, const struct Player *pl);
int save_unpack(const uint8_t *src, size_t n, struct Player *pl);

int save_game_slot(int slot, const struct Player *pl);
int save_load_slot(int slot, struct Player *pl);
int save_game_exist(int slot);
void save_set_info(const char *info);
const char *save_get_info(void);
/* FreeDink load_game_small / &savegameinfo. Slot 1–10. */
void save_info_line(int slot, char *dst, size_t n);

void save_set_dir(const char *dir);
const char *save_host_path(int slot, char *dst, size_t n);

#endif
