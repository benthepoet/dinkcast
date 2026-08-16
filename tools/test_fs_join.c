/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs.h"

static void die(const char *m)
{
    fprintf(stderr, "FAIL %s\n", m);
    exit(1);
}

static void write_file(const char *path, const char *s)
{
    FILE *f = fopen(path, "w");

    if (f == NULL) {
        die(path);
    }
    fputs(s, f);
    fclose(f);
}

int main(void)
{
    char buf[DINK_FS_PATH_MAX];
    char tmp[] = "/tmp/dinkcast-fs-XXXXXX";
    char pc[256], cd[256], fb[256], p[512];
    FILE *fp;

    if (dink_fs_join(buf, sizeof(buf), "/cd/dink", "story/foo.c") != 0) {
        die("join");
    }
    if (strcmp(buf, "/cd/dink/story/foo.c") != 0) {
        die(buf);
    }
    if (dink_fs_join(buf, sizeof(buf), "/cd/dink/", "\\Tiles\\Ts01.bmp") != 0) {
        die("join slashes");
    }
    if (strcmp(buf, "/cd/dink/Tiles/Ts01.bmp") != 0) {
        fprintf(stderr, "got %s\n", buf);
        die("slash normalize");
    }

    if (mkdtemp(tmp) == NULL) {
        die("mkdtemp");
    }
    snprintf(pc, sizeof(pc), "%s/pc", tmp);
    snprintf(cd, sizeof(cd), "%s/cd", tmp);
    snprintf(fb, sizeof(fb), "%s/fb", tmp);
    if (mkdir(fb, 0755) != 0) {
        die("mkdir fb");
    }
    snprintf(p, sizeof(p), "%s/Dink.dat", fb);
    write_file(p, "x");
    snprintf(p, sizeof(p), "%s/STORY", fb);
    if (mkdir(p, 0755) != 0) {
        die("mkdir STORY");
    }
    snprintf(p, sizeof(p), "%s/STORY/START.C", fb);
    write_file(p, "void main(void){}\n");

    unsetenv("DINK_DATA");
    dink_fs_set_probe_roots(pc, cd, fb);
    if (dink_fs_init() != 0) {
        die("init fallback");
    }
    if (strcmp(dink_fs_root(), fb) != 0) {
        die("root fallback");
    }

    if (mkdir(cd, 0755) != 0) {
        die("mkdir cd");
    }
    dink_fs_set_probe_roots(pc, cd, fb);
    if (dink_fs_init() != 0 || strcmp(dink_fs_root(), cd) != 0) {
        die("probe cd before fallback");
    }

    if (mkdir(pc, 0755) != 0) {
        die("mkdir pc");
    }
    dink_fs_set_probe_roots(pc, cd, fb);
    if (dink_fs_init() != 0 || strcmp(dink_fs_root(), pc) != 0) {
        die("probe pc first");
    }

    dink_fs_set_probe_roots("/no-pc", "/no-cd", fb);
    if (dink_fs_init() != 0) {
        die("init fb fopen");
    }
    fp = dink_fopen("dink.dat", "rb");
    if (fp == NULL) {
        die("fopen case dink.dat -> Dink.dat");
    }
    fclose(fp);
    fp = dink_fopen("story/start.c", "rb");
    if (fp == NULL) {
        die("fopen 8.3/case story/start.c");
    }
    fclose(fp);

    printf("OK test_fs_join\n");
    return 0;
}
