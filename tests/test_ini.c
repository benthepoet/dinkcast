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
        "load_sequence graphics\\inside\\innwalls\\walls\\inn- 31 NOTANIM\n";

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
    if (strcmp(seqs[31].prefix, "graphics/inside/innwalls/walls/inn-") != 0) {
        fprintf(stderr, "FAIL seq31 NOTANIM prefix %s\n", seqs[31].prefix);
        return 1;
    }
    printf("OK test_ini\n");
    return 0;
}
