/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_CMD_H
#define DINKCAST_DINKC_CMD_H

struct Player;

void dinkc_cmd_bind_player(struct Player *p);
/* 0 = unknown, 1 = ran. *yield: 0 continue, 1 say_stop, 2 choice. */
int dinkc_cmd(const char *name, int *args, int nargs, const char *str,
              int *yield, int *ret);

#endif
