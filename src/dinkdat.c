/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkdat.h"

#include "fs.h"

#include <stdio.h>

int dink_dat_size(int64_t *bytes)
{
    FILE *fp;
    long sz;

    if (bytes == NULL) {
        return -1;
    }
    *bytes = -1;
    fp = dink_fopen("dink.dat", "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    fclose(fp);
    if (sz < 0) {
        return -1;
    }
    *bytes = (int64_t)sz;
    return 0;
}
