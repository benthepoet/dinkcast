/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_file.h"
#include "dinkc_lex.h"
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
    const char *src =
        "// hi\n"
        "void talk(void) { if (&story == 2 && x != 0) say(\"hi\", 1); }\n";
    struct DinkcLex lx;
    struct DinkcTok t;
    int n;
    char *buf = NULL;
    size_t bn = 0;

    n = dinkc_lex_count(src, strlen(src));
    expect(n > 20, "fixture count");
    dinkc_lex_init(&lx, src, strlen(src));
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_IDENT, "void");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_IDENT, "talk");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_LPAREN, "(");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_IDENT, "void2");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_RPAREN, ")");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_LBRACE, "{");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_IDENT, "if");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_LPAREN, "(2");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_VAR, "&story");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_EQEQ, "==");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_NUMBER, "2");
    expect(dinkc_lex_next(&lx, &t) == 0 && t.kind == DINKC_ANDAND, "&&");
    expect(dinkc_lex_count("\"oops", 5) == 1, "unterm string token");
    expect(dinkc_lex_count("&s2-map", 7) == 1, "hyphen var");

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    expect(dinkc_load("s1-h1-m", &buf, &bn) == 0, "load mom");
    n = dinkc_lex_count(buf, bn);
    expect(n > 50, "mom tokens");
    printf("dinkc lex s1-h1-m ntok=%d\n", n);
    dinkc_free(buf);

    printf("OK test_dinkc_lex\n");
    return 0;
}
