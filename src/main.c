/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Bite 0.2: 640×480 RGB565 color field. No game data, no PVR scene yet.
 */
#include <stdint.h>
#include <stdio.h>

#include "boot.h"
#include "fs.h"

#ifdef _arch_dreamcast
#include <kos.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    vid_set_mode(DM_640x480, PM_RGB565);
    vid_clear(DINK_BOOT_R, DINK_BOOT_G, DINK_BOOT_B);
    printf("%s\n", DINK_BOOT_MSG);
    if (dink_fs_init() == 0) {
        printf("dink_fs_root %s\n", dink_fs_root());
    } else {
        printf("dink_fs_root unresolved\n");
    }
    fflush(stdout);

    for (;;) {
        vid_waitvbl();
    }
    return 0;
}

#else

/* Host cannot produce the ELF; keep a compile-check of the boot constants. */
int dinkcast_host_boot_rgb565(void)
{
    return (int)DINK_BOOT_RGB565;
}

#endif
