/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host: shipped boot.h constants match the plan color field. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/boot.h"

int main(void)
{
    if (DINK_BOOT_WIDTH != 640 || DINK_BOOT_HEIGHT != 480) {
        fprintf(stderr, "FAIL resolution\n");
        return 1;
    }
    if (DINK_BOOT_R != 0x5A || DINK_BOOT_G != 0x3A || DINK_BOOT_B != 0x1A) {
        fprintf(stderr, "FAIL color\n");
        return 1;
    }
    if (strcmp(DINK_BOOT_MSG, "dinkcast boot ok") != 0) {
        fprintf(stderr, "FAIL msg\n");
        return 1;
    }
    if (DINK_BOOT_RGB565 != DINK_RGB565(0x5A, 0x3A, 0x1A)) {
        fprintf(stderr, "FAIL rgb565\n");
        return 1;
    }
    printf("OK boot %dx%d rgb565=0x%04x\n",
           DINK_BOOT_WIDTH, DINK_BOOT_HEIGHT, (unsigned)DINK_BOOT_RGB565);
    return 0;
}
