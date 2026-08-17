/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_file.h"
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    char rel[80];
    char *buf = NULL;
    size_t n = 0;

    expect(dinkc_story_rel("s1-h1-m", rel, sizeof(rel)) == 0, "rel");
    expect(strcmp(rel, "story/s1-h1-m.c") == 0, rel);
    expect(dinkc_story_rel("S1-H1-M.c", rel, sizeof(rel)) == 0, "strip ext");
    expect(strcmp(rel, "story/S1-H1-M.c") == 0, rel);
    expect(dinkc_story_rel("../x", rel, sizeof(rel)) != 0, "..");
    expect(dinkc_story_rel("a/b", rel, sizeof(rel)) != 0, "slash");
    expect(dinkc_story_rel("", rel, sizeof(rel)) != 0, "empty");

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    expect(dinkc_load("s1-h1-m", &buf, &n) == 0, "load mom");
    expect(buf != NULL && n > 20 && n <= DINKC_FILE_CAP, "size");
    expect(strstr(buf, "void") != NULL, "has void");
    dinkc_free(buf);
    expect(dinkc_load("no-such-script-zz", &buf, &n) != 0, "miss");
    expect(buf == NULL, "miss null");

    printf("OK test_dinkc_file\n");
    return 0;
}
