/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_LEX_H
#define DINKCAST_DINKC_LEX_H

#include <stddef.h>

enum DinkcKind {
    DINKC_EOF = 0,
    DINKC_ERR,
    DINKC_IDENT,
    DINKC_NUMBER,
    DINKC_STRING,
    DINKC_VAR,
    DINKC_PLUS,
    DINKC_MINUS,
    DINKC_STAR,
    DINKC_SLASH,
    DINKC_EQ,
    DINKC_EQEQ,
    DINKC_NE,
    DINKC_LT,
    DINKC_GT,
    DINKC_LE,
    DINKC_GE,
    DINKC_ANDAND,
    DINKC_OROR,
    DINKC_LPAREN,
    DINKC_RPAREN,
    DINKC_LBRACE,
    DINKC_RBRACE,
    DINKC_COMMA,
    DINKC_SEMI,
    DINKC_COLON,
    DINKC_OTHER
};

struct DinkcTok {
    enum DinkcKind kind;
    const char *p;
    size_t n;
    int line;
};

struct DinkcLex {
    const char *src;
    size_t len;
    size_t i;
    int line;
};

void dinkc_lex_init(struct DinkcLex *lx, const char *src, size_t n);
/* 0 = token, 1 = EOF, -1 = error (kind ERR). */
int dinkc_lex_next(struct DinkcLex *lx, struct DinkcTok *out);
/* Tokens until EOF; -1 if any ERR. */
int dinkc_lex_count(const char *src, size_t n);

#endif
