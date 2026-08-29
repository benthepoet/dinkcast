/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"
#include "dinkc_cmd.h"
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    int yld = 0, ret = 0;
    int args[5] = {8, 8000, 0, 0, 0};

    expect(audio_playsound(8, 8000, 0, 0, 0) == 0, "play before init");
    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    expect(audio_init() == 0, "init");
    expect(audio_slot_count() > 0, "loaded some");
    expect(audio_slot_loaded(8), "swing slot");
    expect(audio_slot_loaded(9), "punch slot");
    expect(audio_playsound(8, 8000, 0, 0, 0) != 0, "play swing");
    expect(audio_playsound(99, 22050, 0, 0, 0) == 0, "unknown slot");
    expect(dinkc_cmd("playsound", args, 5, NULL, NULL, &yld, &ret) == 1, "cmd");
    expect(ret != 0, "cmd bank");
    args[0] = 99;
    ret = 1;
    expect(dinkc_cmd("playsound", args, 5, NULL, NULL, &yld, &ret) == 1, "cmd miss");
    expect(ret == 0, "miss bank 0");
    expect(audio_sfx_bytes() > 0 && audio_sfx_bytes() < 2u * 1024u * 1024u, "bytes");
    printf("OK test_audio slots=%d bytes=%zu\n", audio_slot_count(), audio_sfx_bytes());
    audio_shutdown();
    return 0;
}
