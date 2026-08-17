/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ini.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    struct SeqInfo seqs[DINK_MAX_SEQ];
    const char *txt =
        "; comment\n"
        "load_sequence_now graphics\\dink\\idle\\ds-i4- 14 250 30 71 -11 -9 11 9\n"
        "load_sequence graphics\\dink\\walk\\ds-w2- 72 43 37 69 -13 -9 13 9\n"
        "load_sequence graphics\\inside\\innwalls\\walls\\inn- 31 NOTANIM\n"
        "SET_SPRITE_INFO 31 22 79 88 -75 1 21 12\n"
        "Set_Sprite_Info 31 22 79 88 -94 -36 21 19\n"
        "set_frame_special 106 3 1\n"
        "LOAD_SEQUENCE graphics\\dink\\walk\\ds-w8- 78 43 37 69 -13 -9 13 9\n";

    if (ini_parse_mem(txt, strlen(txt), seqs, DINK_MAX_SEQ) != 0) {
        fprintf(stderr, "FAIL parse\n");
        return 1;
    }
    if (strcmp(seqs[14].prefix, "graphics/dink/idle/ds-i4-") != 0) {
        fprintf(stderr, "FAIL prefix %s\n", seqs[14].prefix);
        return 1;
    }
    if (seqs[14].delay != 250 || seqs[14].cx != 30 || seqs[14].cy != 71) {
        fprintf(stderr, "FAIL seq14 meta\n");
        return 1;
    }
    if (seqs[72].cx != 37 || seqs[72].cy != 69) {
        fprintf(stderr, "FAIL seq72\n");
        return 1;
    }
    if (seqs[78].cx != 37 || seqs[78].prefix[0] == '\0') {
        fprintf(stderr, "FAIL LOAD_SEQUENCE case seq78\n");
        return 1;
    }
    if (strcmp(seqs[31].prefix, "graphics/inside/innwalls/walls/inn-") != 0) {
        fprintf(stderr, "FAIL seq31 NOTANIM prefix %s\n", seqs[31].prefix);
        return 1;
    }
    {
        int cx, cy, hl, ht, hr, hb;

        ini_frame_geom(&seqs[31], 31, 22, 100, 100, &cx, &cy, &hl, &ht, &hr,
                       &hb);
        if (cx != 79 || cy != 88 || hl != -94 || ht != -36 || hr != 21 ||
            hb != 19) {
            fprintf(stderr, "FAIL SET_SPRITE_INFO last-wins %d %d %d %d %d %d\n",
                    cx, cy, hl, ht, hr, hb);
            return 1;
        }
    }
    if (!ini_frame_special(106, 3) || ini_frame_special(106, 1)) {
        fprintf(stderr, "FAIL SET_FRAME_SPECIAL\n");
        return 1;
    }
    printf("OK test_ini\n");
    return 0;
}
