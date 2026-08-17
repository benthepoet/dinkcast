/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_parse.h"

#include "dinkc_lex.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

struct P {
    struct DinkcLex lx;
    struct DinkcTok t;
    int eof;
    char *err;
    size_t errsz;
    struct DinkcProg *out;
};

static int tok_is(const struct DinkcTok *t, const char *w)
{
    size_t i, n;

    if (t->kind != DINKC_IDENT || w == NULL) {
        return 0;
    }
    n = strlen(w);
    if (t->n != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)t->p[i]) != tolower((unsigned char)w[i])) {
            return 0;
        }
    }
    return 1;
}

static void fail(struct P *p, const char *msg)
{
    if (p->err != NULL && p->errsz > 0) {
        snprintf(p->err, p->errsz, "line %d: %s", p->t.line, msg);
    }
}

static int adv(struct P *p)
{
    int rc = dinkc_lex_next(&p->lx, &p->t);

    if (rc == 1) {
        p->eof = 1;
        p->t.kind = DINKC_EOF;
        return 0;
    }
    if (rc < 0) {
        fail(p, "bad token");
        return -1;
    }
    return 0;
}

static int peek_kind(const struct P *p)
{
    return p->eof ? DINKC_EOF : (int)p->t.kind;
}

static int parse_expr(struct P *p);
static int parse_stmt(struct P *p);
static int parse_proc(struct P *p);

static int parse_primary(struct P *p)
{
    if (peek_kind(p) == DINKC_NUMBER || peek_kind(p) == DINKC_STRING ||
        peek_kind(p) == DINKC_VAR || peek_kind(p) == DINKC_OTHER) {
        return adv(p);
    }
    if (peek_kind(p) == DINKC_IDENT) {
        if (adv(p) != 0) {
            return -1;
        }
        if (peek_kind(p) == DINKC_LPAREN) {
            if (adv(p) != 0) {
                return -1;
            }
            if (peek_kind(p) != DINKC_RPAREN) {
                if (parse_expr(p) != 0) {
                    return -1;
                }
                while (peek_kind(p) == DINKC_COMMA) {
                    if (adv(p) != 0 || parse_expr(p) != 0) {
                        return -1;
                    }
                }
            }
            if (peek_kind(p) != DINKC_RPAREN) {
                fail(p, "expected )");
                return -1;
            }
            return adv(p);
        }
        return 0;
    }
    if (peek_kind(p) == DINKC_LPAREN) {
        if (adv(p) != 0 || parse_expr(p) != 0) {
            return -1;
        }
        if (peek_kind(p) != DINKC_RPAREN) {
            fail(p, "expected )");
            return -1;
        }
        return adv(p);
    }
    fail(p, "expected expression");
    return -1;
}

static int parse_unary(struct P *p)
{
    if (peek_kind(p) == DINKC_MINUS || peek_kind(p) == DINKC_PLUS) {
        if (adv(p) != 0) {
            return -1;
        }
        return parse_unary(p);
    }
    return parse_primary(p);
}

static int parse_mul(struct P *p)
{
    if (parse_unary(p) != 0) {
        return -1;
    }
    while (peek_kind(p) == DINKC_STAR || peek_kind(p) == DINKC_SLASH) {
        if (adv(p) != 0 || parse_unary(p) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_add(struct P *p)
{
    if (parse_mul(p) != 0) {
        return -1;
    }
    while (peek_kind(p) == DINKC_PLUS || peek_kind(p) == DINKC_MINUS) {
        if (adv(p) != 0 || parse_mul(p) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_cmp(struct P *p)
{
    if (parse_add(p) != 0) {
        return -1;
    }
    while (peek_kind(p) == DINKC_EQEQ || peek_kind(p) == DINKC_NE ||
           peek_kind(p) == DINKC_LT || peek_kind(p) == DINKC_GT ||
           peek_kind(p) == DINKC_LE || peek_kind(p) == DINKC_GE) {
        if (adv(p) != 0 || parse_add(p) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_and(struct P *p)
{
    if (parse_cmp(p) != 0) {
        return -1;
    }
    while (peek_kind(p) == DINKC_ANDAND) {
        if (adv(p) != 0 || parse_cmp(p) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_expr(struct P *p)
{
    if (parse_and(p) != 0) {
        return -1;
    }
    while (peek_kind(p) == DINKC_OROR) {
        if (adv(p) != 0 || parse_and(p) != 0) {
            return -1;
        }
    }
    return 0;
}

static int eat_semi(struct P *p)
{
    if (peek_kind(p) == DINKC_SEMI || peek_kind(p) == DINKC_COLON) {
        return adv(p);
    }
    return 0;
}

static int parse_block(struct P *p)
{
    if (peek_kind(p) != DINKC_LBRACE) {
        fail(p, "expected {");
        return -1;
    }
    if (adv(p) != 0) {
        return -1;
    }
    while (peek_kind(p) != DINKC_RBRACE && peek_kind(p) != DINKC_EOF) {
        if (tok_is(&p->t, "void")) {
            return 0;
        }
        if (parse_stmt(p) != 0) {
            return -1;
        }
    }
    if (peek_kind(p) == DINKC_RBRACE) {
        return adv(p);
    }
    return 0;
}

static int skip_until_ident(struct P *p, const char *name)
{
    int titles = 0;

    while (peek_kind(p) != DINKC_EOF) {
        if (tok_is(&p->t, "title_start")) {
            titles++;
        } else if (tok_is(&p->t, "title_end") && titles > 0) {
            titles--;
            if (strcmp(name, "title_end") == 0 && titles == 0) {
                return adv(p);
            }
        } else if (tok_is(&p->t, name) && titles == 0) {
            return adv(p);
        }
        if (adv(p) != 0) {
            return -1;
        }
    }
    fail(p, "unclosed choice/title");
    return -1;
}

static int parse_empty_call(struct P *p)
{
    if (peek_kind(p) != DINKC_LPAREN) {
        return 0;
    }
    if (adv(p) != 0) {
        return -1;
    }
    if (peek_kind(p) != DINKC_RPAREN) {
        fail(p, "expected )");
        return -1;
    }
    return adv(p);
}

static int parse_choice(struct P *p)
{
    if (adv(p) != 0 || parse_empty_call(p) != 0) {
        return -1;
    }
    (void)eat_semi(p);
    if (skip_until_ident(p, "choice_end") != 0) {
        return -1;
    }
    if (parse_empty_call(p) != 0) {
        return -1;
    }
    return eat_semi(p);
}

static int parse_title(struct P *p)
{
    if (adv(p) != 0 || parse_empty_call(p) != 0) {
        return -1;
    }
    (void)eat_semi(p);
    if (skip_until_ident(p, "title_end") != 0) {
        return -1;
    }
    if (parse_empty_call(p) != 0) {
        return -1;
    }
    return eat_semi(p);
}

static int parse_stmt(struct P *p)
{
    if (p->out != NULL) {
        p->out->nstmt++;
    }
    if (peek_kind(p) == DINKC_COMMA) {
        return adv(p);
    }
    if (peek_kind(p) == DINKC_SEMI || peek_kind(p) == DINKC_OTHER ||
        peek_kind(p) == DINKC_COLON) {
        return adv(p);
    }
    if (peek_kind(p) == DINKC_LPAREN) {
        if (adv(p) != 0) {
            return -1;
        }
        while (peek_kind(p) != DINKC_EOF && peek_kind(p) != DINKC_RPAREN) {
            if (adv(p) != 0) {
                return -1;
            }
        }
        if (peek_kind(p) == DINKC_RPAREN) {
            return adv(p);
        }
        return 0;
    }
    if (peek_kind(p) == DINKC_LBRACE) {
        return parse_block(p);
    }
    if (tok_is(&p->t, "if")) {
        if (adv(p) != 0) {
            return -1;
        }
        if (peek_kind(p) != DINKC_LPAREN) {
            fail(p, "if (");
            return -1;
        }
        if (adv(p) != 0 || parse_expr(p) != 0) {
            return -1;
        }
        if (peek_kind(p) != DINKC_RPAREN) {
            fail(p, "if )");
            return -1;
        }
        if (adv(p) != 0 || parse_stmt(p) != 0) {
            return -1;
        }
        if (tok_is(&p->t, "else")) {
            if (adv(p) != 0 || parse_stmt(p) != 0) {
                return -1;
            }
        }
        return 0;
    }
    if (tok_is(&p->t, "int")) {
        if (adv(p) != 0) {
            return -1;
        }
        if (peek_kind(p) != DINKC_VAR) {
            fail(p, "int &name");
            return -1;
        }
        if (adv(p) != 0) {
            return -1;
        }
        if (peek_kind(p) == DINKC_EQ) {
            if (adv(p) != 0 || parse_expr(p) != 0) {
                return -1;
            }
        }
        return eat_semi(p);
    }
    if (tok_is(&p->t, "goto")) {
        if (adv(p) != 0) {
            return -1;
        }
        if (peek_kind(p) != DINKC_IDENT) {
            fail(p, "goto name");
            return -1;
        }
        if (adv(p) != 0) {
            return -1;
        }
        return eat_semi(p);
    }
    if (tok_is(&p->t, "return")) {
        if (adv(p) != 0) {
            return -1;
        }
        return eat_semi(p);
    }
    if (tok_is(&p->t, "choice_start")) {
        return parse_choice(p);
    }
    if (tok_is(&p->t, "title_start")) {
        return parse_title(p);
    }
    if (peek_kind(p) == DINKC_VAR) {
        if (adv(p) != 0) {
            return -1;
        }
        if (peek_kind(p) == DINKC_PLUS || peek_kind(p) == DINKC_MINUS ||
            peek_kind(p) == DINKC_STAR || peek_kind(p) == DINKC_SLASH) {
            if (adv(p) != 0) {
                return -1;
            }
            if (peek_kind(p) == DINKC_EQ) {
                if (adv(p) != 0) {
                    return -1;
                }
            }
            if (parse_expr(p) != 0) {
                return -1;
            }
            return eat_semi(p);
        }
        if (peek_kind(p) == DINKC_EQ) {
            if (adv(p) != 0 || parse_expr(p) != 0) {
                return -1;
            }
        }
        return eat_semi(p);
    }
    if (peek_kind(p) == DINKC_IDENT) {
        /* label: */
        if (adv(p) != 0) {
            return -1;
        }
        if (peek_kind(p) == DINKC_COLON) {
            return adv(p);
        }
        if (peek_kind(p) == DINKC_LPAREN) {
            /* already ate ident; parse call as primary leftover */
            if (adv(p) != 0) {
                return -1;
            }
            if (peek_kind(p) != DINKC_RPAREN) {
                if (parse_expr(p) != 0 ||
                    (peek_kind(p) != DINKC_COMMA && peek_kind(p) != DINKC_RPAREN)) {
                    while (peek_kind(p) != DINKC_EOF &&
                           peek_kind(p) != DINKC_RPAREN &&
                           peek_kind(p) != DINKC_SEMI &&
                           peek_kind(p) != DINKC_RBRACE) {
                        if (adv(p) != 0) {
                            return -1;
                        }
                    }
                } else {
                    while (peek_kind(p) == DINKC_COMMA) {
                        if (adv(p) != 0 || parse_expr(p) != 0) {
                            while (peek_kind(p) != DINKC_EOF &&
                                   peek_kind(p) != DINKC_RPAREN &&
                                   peek_kind(p) != DINKC_COMMA) {
                                if (adv(p) != 0) {
                                    return -1;
                                }
                            }
                        }
                    }
                }
            }
            if (peek_kind(p) == DINKC_RPAREN) {
                if (adv(p) != 0) {
                    return -1;
                }
            }
            return eat_semi(p);
        }
        if (peek_kind(p) == DINKC_EQ) {
            if (adv(p) != 0 || parse_expr(p) != 0) {
                return -1;
            }
            return eat_semi(p);
        }
        /* Bare command: set_y 240 */
        while (peek_kind(p) == DINKC_NUMBER || peek_kind(p) == DINKC_STRING ||
               peek_kind(p) == DINKC_VAR || peek_kind(p) == DINKC_IDENT) {
            if (tok_is(&p->t, "if") || tok_is(&p->t, "else") ||
                tok_is(&p->t, "int") || tok_is(&p->t, "void") ||
                tok_is(&p->t, "goto") || tok_is(&p->t, "return") ||
                tok_is(&p->t, "choice_start") || tok_is(&p->t, "title_start") ||
                tok_is(&p->t, "choice_end") || tok_is(&p->t, "title_end")) {
                break;
            }
            if (adv(p) != 0) {
                return -1;
            }
        }
        return eat_semi(p);
    }
    fail(p, "bad statement");
    return -1;
}

static int parse_proc(struct P *p)
{
    struct DinkcTok name;

    if (!tok_is(&p->t, "void")) {
        fail(p, "expected void");
        return -1;
    }
    if (adv(p) != 0) {
        return -1;
    }
    if (peek_kind(p) != DINKC_IDENT) {
        fail(p, "proc name");
        return -1;
    }
    name = p->t;
    if (p->out != NULL && p->out->nproc < DINKC_MAX_PROC) {
        size_t n = name.n < 31 ? name.n : 31;

        memcpy(p->out->proc[p->out->nproc].name, name.p, n);
        p->out->proc[p->out->nproc].name[n] = '\0';
        p->out->proc[p->out->nproc].line = name.line;
        p->out->nproc++;
    }
    if (adv(p) != 0) {
        return -1;
    }
    if (peek_kind(p) != DINKC_LPAREN) {
        fail(p, "proc (");
        return -1;
    }
    if (adv(p) != 0) {
        return -1;
    }
    if (tok_is(&p->t, "void")) {
        if (adv(p) != 0) {
            return -1;
        }
    }
    if (peek_kind(p) != DINKC_RPAREN) {
        fail(p, "proc )");
        return -1;
    }
    if (adv(p) != 0) {
        return -1;
    }
    if (peek_kind(p) == DINKC_LBRACE) {
        return parse_block(p);
    }
    /* Stock often omits { after void name(). */
    while (peek_kind(p) != DINKC_EOF && !tok_is(&p->t, "void")) {
        if (peek_kind(p) == DINKC_RBRACE) {
            return adv(p);
        }
        if (parse_stmt(p) != 0) {
            return -1;
        }
    }
    return 0;
}

int dinkc_parse(const char *src, size_t n, struct DinkcProg *out, char *err,
                size_t errsz)
{
    struct P p;

    memset(&p, 0, sizeof(p));
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    p.out = out;
    p.err = err;
    p.errsz = errsz;
    if (err != NULL && errsz > 0) {
        err[0] = '\0';
    }
    dinkc_lex_init(&p.lx, src, n);
    if (adv(&p) != 0) {
        return -1;
    }
    while (peek_kind(&p) != DINKC_EOF) {
        if (peek_kind(&p) == DINKC_RBRACE || peek_kind(&p) == DINKC_OTHER) {
            if (adv(&p) != 0) {
                return -1;
            }
            continue;
        }
        if (tok_is(&p.t, "void")) {
            if (parse_proc(&p) != 0) {
                return -1;
            }
            continue;
        }
        if (parse_stmt(&p) != 0) {
            return -1;
        }
    }
    return 0;
}
