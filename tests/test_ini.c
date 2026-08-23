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
        "set_frame_frame 14 5 14 3\n"
        "set_frame_frame 14 6 14 2\n"
        "set_frame_delay 14 5 250\n"
        "set_frame_delay 14 6 250\n"
        "set_frame_frame 111 5 -1\n"
        "LOAD_SEQUENCE graphics\\dink\\walk\\ds-w8- 78 43 37 69 -13 -9 13 9\n"
        "load_sequence graphics\\items\\food\\food- 421 12 20 -13 -19 21 7\n";

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
    {
        int cx, cy, hl, ht, hr, hb;

        ini_frame_geom(&seqs[421], 421, 1, 40, 40, &cx, &cy, &hl, &ht, &hr,
                       &hb);
        if (hl != -19 || hr != 7 || ht != -(40 / 10) || hb != 40 / 10) {
            fprintf(stderr, "FAIL food 421 box %d %d %d %d\n", hl, ht, hr, hb);
            return 1;
        }
    }
    if (!ini_frame_special(106, 3) || ini_frame_special(106, 1)) {
        fprintf(stderr, "FAIL SET_FRAME_SPECIAL\n");
        return 1;
    }
    {
        int dseq = 0, dfr = 0;

        if (ini_resolve_frame(14, 5, &dseq, &dfr) != 0 || dseq != 14 ||
            dfr != 3) {
            fprintf(stderr, "FAIL alias 14.5 -> %d %d\n", dseq, dfr);
            return 1;
        }
        if (ini_resolve_frame(14, 6, &dseq, &dfr) != 0 || dseq != 14 ||
            dfr != 2) {
            fprintf(stderr, "FAIL alias 14.6\n");
            return 1;
        }
        if (ini_resolve_frame(111, 5, &dseq, &dfr) != 1) {
            fprintf(stderr, "FAIL terminator 111.5\n");
            return 1;
        }
        if (ini_frame_delay(14, 5, 50) != 250 ||
            ini_frame_delay(14, 1, 250) != 250) {
            fprintf(stderr, "FAIL frame delay\n");
            return 1;
        }
        if (ini_seq_len(14, 4) != 6) {
            fprintf(stderr, "FAIL idle seq len %d\n", ini_seq_len(14, 4));
            return 1;
        }
        if (ini_seq_len(111, 4) != 4) {
            fprintf(stderr, "FAIL punch seq len %d\n", ini_seq_len(111, 4));
            return 1;
        }
    }
    {
        const char *line =
            "load_sequence_now graphics\\dink\\sword\\walk\\d-sw1- 71 43 64 69 "
            "-14 -10 14 10";

        if (ini_apply_line(line, seqs, DINK_MAX_SEQ) < 1) {
            fprintf(stderr, "FAIL apply_line\n");
            return 1;
        }
        if (strstr(seqs[71].prefix, "sword") == NULL) {
            fprintf(stderr, "FAIL apply_line prefix %s\n", seqs[71].prefix);
            return 1;
        }
        if (seqs[14].delay != 250) {
            fprintf(stderr, "FAIL apply_line wiped seq14\n");
            return 1;
        }
    }
    {
        int cx1, cy1, hl1, ht1, hr1, hb1;
        int cx2, cy2, hl2, ht2, hr2, hb2;
        const char *pig74 =
            "load_sequence_now graphics\\dink\\walk\\ds-w4- 74 35 38 72";

        if (ini_apply_line(pig74, seqs, DINK_MAX_SEQ) < 1) {
            fprintf(stderr, "FAIL pig74 apply\n");
            return 1;
        }
        if (seqs[74].delay != 35 || seqs[74].cx != 38 || seqs[74].cy != 72 ||
            seqs[74].hr != 0) {
            fprintf(stderr, "FAIL pig74 meta d=%d cx=%d cy=%d hr=%d\n",
                    seqs[74].delay, seqs[74].cx, seqs[74].cy, seqs[74].hr);
            return 1;
        }
        ini_frame_geom(&seqs[74], 74, 1, 80, 100, &cx1, &cy1, &hl1, &ht1, &hr1,
                       &hb1);
        ini_frame_geom(&seqs[74], 74, 2, 50, 90, &cx2, &cy2, &hl2, &ht2, &hr2,
                       &hb2);
        if (cx1 != 38 || cy1 != 72 || cx2 != 38 || cy2 != 72) {
            fprintf(stderr, "FAIL pig74 center %d,%d vs %d,%d\n", cx1, cy1, cx2,
                    cy2);
            return 1;
        }
        (void)hl1;
        (void)ht1;
        (void)hr1;
        (void)hb1;
        (void)hl2;
        (void)ht2;
        (void)hr2;
        (void)hb2;
    }
    printf("OK test_ini\n");
    return 0;
}
