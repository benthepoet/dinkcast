/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mem.h"

#include <stdio.h>

int main(void)
{
    mem_log("test", 100, 2, 50, 1);
    printf("OK test_mem\n");
    return 0;
}
