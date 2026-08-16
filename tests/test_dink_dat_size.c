/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dinkdat.h"
#include "fs.h"

static void die(const char *m)
{
    fprintf(stderr, "FAIL %s\n", m);
    exit(1);
}

int main(void)
{
    char tmp[] = "/tmp/dinkcast-dat-XXXXXX";
    char p[512];
    FILE *f;
    int64_t sz = -1;

    if (mkdtemp(tmp) == NULL) {
        die("mkdtemp");
    }
    unsetenv("DINK_DATA");
    dink_fs_set_probe_roots("/no", "/no", tmp);
    if (dink_fs_init() != 0) {
        die("init");
    }
    if (dink_dat_size(&sz) == 0) {
        die("missing should fail");
    }
    snprintf(p, sizeof(p), "%s/Dink.dat", tmp);
    f = fopen(p, "wb");
    if (f == NULL) {
        die("write");
    }
    fwrite("Smallwood", 1, 9, f);
    fclose(f);
    if (dink_dat_size(&sz) != 0 || sz != 9) {
        die("size");
    }
    printf("OK test_dink_dat_size %lld\n", (long long)sz);
    return 0;
}
