/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fs.h"
#include "ini.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    struct SeqInfo *seqs;
    int id = 14;

    if (argc > 1) {
        id = atoi(argv[1]);
    }
    if (dink_fs_init() != 0) {
        fprintf(stderr, "no DINK_DATA\n");
        return 1;
    }
    seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
    if (seqs == NULL || ini_load(seqs, DINK_MAX_SEQ) != 0) {
        fprintf(stderr, "ini_load failed\n");
        free(seqs);
        return 1;
    }
    if (id < 1 || id >= DINK_MAX_SEQ || seqs[id].prefix[0] == '\0') {
        fprintf(stderr, "no seq %d\n", id);
        free(seqs);
        return 1;
    }
    seqs[id].nframes = ini_count_ff_frames(seqs[id].prefix);
    printf("seq %d prefix %s delay %d cx %d cy %d frames %d\n", id,
           seqs[id].prefix, seqs[id].delay, seqs[id].cx, seqs[id].cy,
           seqs[id].nframes);
    free(seqs);
    return 0;
}
