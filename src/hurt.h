/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_HURT_H
#define DINKCAST_HURT_H

/* FreeDink hurt_thing: damage < 1 → 0; num = damage - defense;
 * num < 1 → 0; num == 0 → 50% chance of 1. Does not store hp. */
int hurt_roll(int damage, int defense);

#endif
