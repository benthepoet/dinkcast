/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hurt.h"

#include <stdlib.h>

int hurt_roll(int damage, int defense)
{
    int num;

    if (damage < 1) {
        return 0;
    }
    num = damage - defense;
    if (num < 1) {
        num = 0;
    }
    if (num == 0) {
        if ((rand() % 2) + 1 == 1) {
            num = 1;
        }
    }
    return num;
}
