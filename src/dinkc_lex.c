/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_lex.h"

#include <ctype.h>
#include <string.h>

void dinkc_lex_init(struct DinkcLex *lx, const char *src, size_t n)
{
    if (lx == NULL) {
        return;
    }
    lx->src = src != NULL ? src : "";
    lx->len = src != NULL ? n : 0;
    lx->i = 0;
    lx->line = 1;
}

static int peek(const struct DinkcLex *lx)
{
    if (lx->i >= lx->len) {
        return 0;
    }
    return (unsigned char)lx->src[lx->i];
}

static int peek2(const struct DinkcLex *lx)
{
    if (lx->i + 1 >= lx->len) {
        return 0;
    }
    return (unsigned char)lx->src[lx->i + 1];
}

static int eat(struct DinkcLex *lx)
{
    int c = peek(lx);

    if (c == 0 && lx->i >= lx->len) {
        return 0;
    }
    lx->i++;
    if (c == '\n') {
        lx->line++;
    }
    return c;
}

static void skip_ws(struct DinkcLex *lx)
{
    for (;;) {
        int c = peek(lx);

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            eat(lx);
            continue;
        }
        if (c == '/' && peek2(lx) == '/') {
            eat(lx);
            eat(lx);
            while (peek(lx) != 0 && peek(lx) != '\n') {
                eat(lx);
            }
            continue;
        }
        return;
    }
}

static int ident_start(int c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int ident_cont(int c)
{
    /* FreeDink get_word is space-split; &s2-map is one token. */
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

int dinkc_lex_next(struct DinkcLex *lx, struct DinkcTok *out)
{
    int c;

    if (lx == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    skip_ws(lx);
    out->line = lx->line;
    out->p = lx->src + lx->i;
    if (lx->i >= lx->len) {
        out->kind = DINKC_EOF;
        return 1;
    }
    c = peek(lx);
    if (ident_start(c)) {
        eat(lx);
        while (ident_cont(peek(lx))) {
            eat(lx);
        }
        out->kind = DINKC_IDENT;
        out->n = (size_t)((lx->src + lx->i) - out->p);
        return 0;
    }
    if (c == '&' && ident_start(peek2(lx))) {
        eat(lx);
        eat(lx);
        while (ident_cont(peek(lx))) {
            eat(lx);
        }
        out->kind = DINKC_VAR;
        out->n = (size_t)((lx->src + lx->i) - out->p);
        return 0;
    }
    if (isdigit((unsigned char)c)) {
        eat(lx);
        while (isdigit((unsigned char)peek(lx))) {
            eat(lx);
        }
        out->kind = DINKC_NUMBER;
        out->n = (size_t)((lx->src + lx->i) - out->p);
        return 0;
    }
    if (c == '"') {
        eat(lx);
        while (peek(lx) != 0 && peek(lx) != '"' && peek(lx) != '\n') {
            eat(lx);
        }
        if (peek(lx) != '"') {
            out->kind = DINKC_ERR;
            out->n = (size_t)((lx->src + lx->i) - out->p);
            return -1;
        }
        eat(lx);
        out->kind = DINKC_STRING;
        out->n = (size_t)((lx->src + lx->i) - out->p);
        return 0;
    }
    eat(lx);
    switch (c) {
    case '+':
        out->kind = DINKC_PLUS;
        break;
    case '-':
        out->kind = DINKC_MINUS;
        break;
    case '*':
        out->kind = DINKC_STAR;
        break;
    case '/':
        out->kind = DINKC_SLASH;
        break;
    case '(':
        out->kind = DINKC_LPAREN;
        break;
    case ')':
        out->kind = DINKC_RPAREN;
        break;
    case '{':
        out->kind = DINKC_LBRACE;
        break;
    case '}':
        out->kind = DINKC_RBRACE;
        break;
    case ',':
        out->kind = DINKC_COMMA;
        break;
    case ';':
        out->kind = DINKC_SEMI;
        break;
    case ':':
        out->kind = DINKC_COLON;
        break;
    case '=':
        if (peek(lx) == '=') {
            eat(lx);
            out->kind = DINKC_EQEQ;
        } else {
            out->kind = DINKC_EQ;
        }
        break;
    case '!':
        if (peek(lx) == '=') {
            eat(lx);
            out->kind = DINKC_NE;
        } else {
            out->kind = DINKC_ERR;
            out->n = 1;
            return -1;
        }
        break;
    case '<':
        if (peek(lx) == '=') {
            eat(lx);
            out->kind = DINKC_LE;
        } else {
            out->kind = DINKC_LT;
        }
        break;
    case '>':
        if (peek(lx) == '=') {
            eat(lx);
            out->kind = DINKC_GE;
        } else {
            out->kind = DINKC_GT;
        }
        break;
    case '&':
        if (peek(lx) == '&') {
            eat(lx);
            out->kind = DINKC_ANDAND;
        } else {
            out->kind = DINKC_ERR;
            out->n = 1;
            return -1;
        }
        break;
    case '|':
        if (peek(lx) == '|') {
            eat(lx);
            out->kind = DINKC_OROR;
        } else {
            out->kind = DINKC_ERR;
            out->n = 1;
            return -1;
        }
        break;
    default:
        out->kind = DINKC_ERR;
        out->n = 1;
        return -1;
    }
    out->n = (size_t)((lx->src + lx->i) - out->p);
    return 0;
}

int dinkc_lex_count(const char *src, size_t n)
{
    struct DinkcLex lx;
    struct DinkcTok t;
    int rc, ntok = 0;

    dinkc_lex_init(&lx, src, n);
    for (;;) {
        rc = dinkc_lex_next(&lx, &t);
        if (rc == 1) {
            return ntok;
        }
        if (rc < 0) {
            return -1;
        }
        ntok++;
    }
}
