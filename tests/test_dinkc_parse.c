/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_file.h"
#include "dinkc_parse.h"
#include "fs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    const char *fx =
        "void main(void) {\n"
        "int &x = 1;\n"
        "if (&x == 1) { say(\"hi\", 1); } else return;\n"
        "goto there;\n"
        "there:\n"
        "}\n"
        "void talk() { choice_start(); \"A\" \"B\" choice_end(); }\n";
    struct DinkcProg pr;
    char err[128];
    DIR *d;
    char dir[512];
    struct dirent *de;
    int fails = 0, nfile = 0;

    expect(dinkc_parse(fx, strlen(fx), &pr, err, sizeof(err)) == 0, err);
    expect(pr.nproc == 2, "2 procs");

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    snprintf(dir, sizeof(dir), "%s/Story", dink_fs_root());
    d = opendir(dir);
    if (d == NULL) {
        snprintf(dir, sizeof(dir), "%s/story", dink_fs_root());
        d = opendir(dir);
    }
    expect(d != NULL, "Story dir");
    while ((de = readdir(d)) != NULL) {
        size_t ln = strlen(de->d_name);
        char rel[80], *buf = NULL;
        size_t bn = 0;
        char stem[64];

        if (ln < 3) {
            continue;
        }
        if (strcasecmp(de->d_name + ln - 2, ".c") != 0) {
            continue;
        }
        memcpy(stem, de->d_name, ln - 2);
        stem[ln - 2] = '\0';
        nfile++;
        if (dinkc_load(stem, &buf, &bn) != 0) {
            fprintf(stderr, "FAIL load %s\n", stem);
            fails++;
            continue;
        }
        if (dinkc_parse(buf, bn, &pr, err, sizeof(err)) != 0) {
            fprintf(stderr, "FAIL parse %s: %s\n", stem, err);
            fails++;
        }
        dinkc_free(buf);
        (void)rel;
    }
    closedir(d);
    printf("dinkc parse files=%d fails=%d\n", nfile, fails);
    expect(fails == 0 && nfile > 50, "stock parse");
    printf("OK test_dinkc_parse\n");
    return 0;
}
