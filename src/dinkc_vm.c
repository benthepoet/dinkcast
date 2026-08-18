/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_vm.h"

#include "dinkc_lex.h"
#include "dinkc_var.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DINKC_MAX_TOK 4096

struct Fiber {
    int used;
    int state;
    int sprite;
    int wait_until;
    int depth;
    int ip;
    int ntok;
    int locals[DINKC_LOCALS];
    char name[32];
    char *src;
    size_t srclen;
    struct DinkcTok *tok;
};

static struct Fiber g_f[DINKC_MAX_LIVE + 1];
static int g_ops_ovf;
static int g_var_ready;

static int fiber_slot(const struct Fiber *f)
{
    return (int)(f - g_f);
}

static void copy_tok(const struct DinkcTok *t, char *buf, size_t sz)
{
    size_t n = t->n < sz - 1 ? t->n : sz - 1;

    memcpy(buf, t->p, n);
    buf[n] = '\0';
}

static int eval_expr(struct Fiber *f);
static void skip_balanced(struct Fiber *f, enum DinkcKind open,
                          enum DinkcKind close);

static int eval_prim(struct Fiber *f)
{
    char name[DINKC_VAR_NAME];
    int v;

    if (f->ip >= f->ntok) {
        return 0;
    }
    if (f->tok[f->ip].kind == DINKC_NUMBER) {
        copy_tok(&f->tok[f->ip], name, sizeof(name));
        f->ip++;
        return atoi(name);
    }
    if (f->tok[f->ip].kind == DINKC_VAR) {
        copy_tok(&f->tok[f->ip], name, sizeof(name));
        f->ip++;
        return dinkc_var_get(name, fiber_slot(f), f->sprite);
    }
    if (f->tok[f->ip].kind == DINKC_STRING) {
        f->ip++;
        return 0;
    }
    if (f->tok[f->ip].kind == DINKC_IDENT) {
        f->ip++;
        if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LPAREN) {
            skip_balanced(f, DINKC_LPAREN, DINKC_RPAREN);
        }
        return 0;
    }
    if (f->tok[f->ip].kind == DINKC_LPAREN) {
        f->ip++;
        v = eval_expr(f);
        if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_RPAREN) {
            f->ip++;
        }
        return v;
    }
    f->ip++;
    return 0;
}

static int eval_unary(struct Fiber *f)
{
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_MINUS) {
        f->ip++;
        return -eval_unary(f);
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_PLUS) {
        f->ip++;
        return eval_unary(f);
    }
    return eval_prim(f);
}

static int eval_mul(struct Fiber *f)
{
    int v = eval_unary(f);

    while (f->ip < f->ntok &&
           (f->tok[f->ip].kind == DINKC_STAR || f->tok[f->ip].kind == DINKC_SLASH)) {
        enum DinkcKind op = f->tok[f->ip].kind;
        int r;

        f->ip++;
        r = eval_unary(f);
        if (op == DINKC_STAR) {
            v *= r;
        } else if (r != 0) {
            v /= r;
        }
    }
    return v;
}

static int eval_add(struct Fiber *f)
{
    int v = eval_mul(f);

    while (f->ip < f->ntok &&
           (f->tok[f->ip].kind == DINKC_PLUS || f->tok[f->ip].kind == DINKC_MINUS)) {
        enum DinkcKind op = f->tok[f->ip].kind;
        int r;

        f->ip++;
        r = eval_mul(f);
        v = (op == DINKC_PLUS) ? v + r : v - r;
    }
    return v;
}

static int eval_cmp(struct Fiber *f)
{
    int v = eval_add(f);

    while (f->ip < f->ntok) {
        enum DinkcKind op = f->tok[f->ip].kind;
        int r;

        if (op != DINKC_EQEQ && op != DINKC_NE && op != DINKC_LT &&
            op != DINKC_GT && op != DINKC_LE && op != DINKC_GE) {
            break;
        }
        f->ip++;
        r = eval_add(f);
        if (op == DINKC_EQEQ) {
            v = v == r;
        } else if (op == DINKC_NE) {
            v = v != r;
        } else if (op == DINKC_LT) {
            v = v < r;
        } else if (op == DINKC_GT) {
            v = v > r;
        } else if (op == DINKC_LE) {
            v = v <= r;
        } else {
            v = v >= r;
        }
    }
    return v;
}

static int eval_and(struct Fiber *f)
{
    int v = eval_cmp(f);

    while (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_ANDAND) {
        f->ip++;
        v = eval_cmp(f) && v;
    }
    return v;
}

static int eval_expr(struct Fiber *f)
{
    int v = eval_and(f);

    while (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_OROR) {
        f->ip++;
        v = eval_and(f) || v;
    }
    return v;
}

static void skip_stmt_body(struct Fiber *f)
{
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LBRACE) {
        skip_balanced(f, DINKC_LBRACE, DINKC_RBRACE);
        return;
    }
    while (f->ip < f->ntok && f->tok[f->ip].kind != DINKC_SEMI &&
           f->tok[f->ip].kind != DINKC_RBRACE) {
        f->ip++;
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_SEMI) {
        f->ip++;
    }
}

static void eat_semi(struct Fiber *f)
{
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_SEMI) {
        f->ip++;
    }
}

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

static int fill_toks(struct Fiber *f)
{
    struct DinkcLex lx;
    struct DinkcTok t;
    int rc;

    f->ntok = 0;
    dinkc_lex_init(&lx, f->src, f->srclen);
    f->tok = (struct DinkcTok *)malloc(sizeof(struct DinkcTok) * DINKC_MAX_TOK);
    if (f->tok == NULL) {
        return -1;
    }
    for (;;) {
        rc = dinkc_lex_next(&lx, &t);
        if (rc == 1) {
            return 0;
        }
        if (rc < 0) {
            return -1;
        }
        if (f->ntok >= DINKC_MAX_TOK) {
            return -1;
        }
        f->tok[f->ntok++] = t;
    }
}

static int find_main(struct Fiber *f)
{
    int i;

    for (i = 0; i + 3 < f->ntok; i++) {
        if (tok_is(&f->tok[i], "void") && tok_is(&f->tok[i + 1], "main")) {
            while (i < f->ntok && f->tok[i].kind != DINKC_LBRACE) {
                i++;
            }
            if (i < f->ntok && f->tok[i].kind == DINKC_LBRACE) {
                return i + 1;
            }
            return i;
        }
    }
    return -1;
}

static void skip_balanced(struct Fiber *f, enum DinkcKind open, enum DinkcKind close)
{
    int d = 1;

    if (f->ip < f->ntok && f->tok[f->ip].kind == open) {
        f->ip++;
    }
    while (f->ip < f->ntok && d > 0) {
        if (f->tok[f->ip].kind == open) {
            d++;
        } else if (f->tok[f->ip].kind == close) {
            d--;
        }
        f->ip++;
    }
}

static void skip_call(struct Fiber *f)
{
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LPAREN) {
        skip_balanced(f, DINKC_LPAREN, DINKC_RPAREN);
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_SEMI) {
        f->ip++;
    }
}

static int num_arg(struct Fiber *f)
{
    int v = 0;
    char buf[16];
    size_t n;

    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LPAREN) {
        f->ip++;
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_NUMBER) {
        n = f->tok[f->ip].n < 15 ? f->tok[f->ip].n : 15;
        memcpy(buf, f->tok[f->ip].p, n);
        buf[n] = '\0';
        v = atoi(buf);
        f->ip++;
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_RPAREN) {
        f->ip++;
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_SEMI) {
        f->ip++;
    }
    return v;
}

static void fiber_kill(struct Fiber *f)
{
    free(f->src);
    free(f->tok);
    memset(f, 0, sizeof(*f));
}

static void run_fiber(struct Fiber *f, int now_ms)
{
    int ops = 0;

    (void)now_ms;
    f->state = DINKC_RUN;
    while (f->used && f->state == DINKC_RUN && f->ip < f->ntok) {
        const struct DinkcTok *t = &f->tok[f->ip];

        ops++;
        if (ops > DINKC_OPS_CAP) {
            g_ops_ovf++;
            printf("dinkc vm ops cap slot overflow\n");
            return;
        }
        if (t->kind == DINKC_LBRACE) {
            f->depth++;
            f->ip++;
            continue;
        }
        if (t->kind == DINKC_RBRACE) {
            f->depth--;
            f->ip++;
            if (f->depth <= 0) {
                fiber_kill(f);
                return;
            }
            continue;
        }
        if (tok_is(t, "void") || tok_is(t, "return") ||
            tok_is(t, "kill_this_task")) {
            fiber_kill(f);
            return;
        }
        if (tok_is(t, "wait")) {
            int ms;

            f->ip++;
            ms = num_arg(f);
            if (ms < 1) {
                ms = 1;
            }
            f->state = DINKC_WAIT_MS;
            f->wait_until = now_ms + ms;
            return;
        }
        if (tok_is(t, "say_stop") || tok_is(t, "say_stop_npc")) {
            f->ip++;
            skip_call(f);
            f->state = DINKC_WAIT_SAY;
            printf("dinkc yield say\n");
            return;
        }
        if (tok_is(t, "move_stop")) {
            f->ip++;
            skip_call(f);
            f->state = DINKC_WAIT_MOVE;
            return;
        }
        if (tok_is(t, "choice_start")) {
            f->ip++;
            skip_call(f);
            while (f->ip < f->ntok && !tok_is(&f->tok[f->ip], "choice_end")) {
                f->ip++;
            }
            if (f->ip < f->ntok) {
                f->ip++;
                skip_call(f);
            }
            f->state = DINKC_WAIT_CHOICE;
            printf("dinkc yield choice\n");
            return;
        }
        if (tok_is(t, "else")) {
            f->ip++;
            skip_stmt_body(f);
            continue;
        }
        if (tok_is(t, "if")) {
            int cond;

            f->ip++;
            if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LPAREN) {
                f->ip++;
                cond = eval_expr(f);
                if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_RPAREN) {
                    f->ip++;
                }
            } else {
                cond = 0;
            }
            if (cond) {
                continue;
            }
            skip_stmt_body(f);
            if (f->ip < f->ntok && tok_is(&f->tok[f->ip], "else")) {
                f->ip++;
            }
            continue;
        }
        if (tok_is(t, "int")) {
            char name[DINKC_VAR_NAME];

            f->ip++;
            if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_VAR) {
                copy_tok(&f->tok[f->ip], name, sizeof(name));
                f->ip++;
                dinkc_var_make(name, 0, fiber_slot(f));
                if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_EQ) {
                    f->ip++;
                    dinkc_var_set(name, eval_expr(f), fiber_slot(f), f->sprite);
                }
            }
            eat_semi(f);
            continue;
        }
        if (tok_is(t, "make_global_int")) {
            char name[DINKC_VAR_NAME];
            int val = 0;

            f->ip++;
            if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LPAREN) {
                f->ip++;
            }
            if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_STRING) {
                copy_tok(&f->tok[f->ip], name, sizeof(name));
                /* strip quotes */
                if (name[0] == '"' && name[strlen(name) - 1] == '"') {
                    name[strlen(name) - 1] = '\0';
                    memmove(name, name + 1, strlen(name));
                }
                f->ip++;
            } else {
                name[0] = '\0';
            }
            if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_COMMA) {
                f->ip++;
                val = eval_expr(f);
            }
            if (name[0] == '&') {
                dinkc_var_make_global(name, val);
            }
            skip_call(f);
            continue;
        }
        if (t->kind == DINKC_VAR) {
            char name[DINKC_VAR_NAME];
            int v;

            copy_tok(t, name, sizeof(name));
            f->ip++;
            if (f->ip < f->ntok &&
                (f->tok[f->ip].kind == DINKC_PLUS ||
                 f->tok[f->ip].kind == DINKC_MINUS ||
                 f->tok[f->ip].kind == DINKC_STAR ||
                 f->tok[f->ip].kind == DINKC_SLASH)) {
                enum DinkcKind op = f->tok[f->ip].kind;

                f->ip++;
                if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_EQ) {
                    f->ip++;
                }
                v = dinkc_var_get(name, fiber_slot(f), f->sprite);
                if (op == DINKC_PLUS) {
                    v += eval_expr(f);
                } else if (op == DINKC_MINUS) {
                    v -= eval_expr(f);
                } else if (op == DINKC_STAR) {
                    v *= eval_expr(f);
                } else {
                    int r = eval_expr(f);

                    if (r != 0) {
                        v /= r;
                    }
                }
                dinkc_var_set(name, v, fiber_slot(f), f->sprite);
            } else if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_EQ) {
                f->ip++;
                dinkc_var_set(name, eval_expr(f), fiber_slot(f), f->sprite);
            }
            eat_semi(f);
            continue;
        }
        f->ip++;
    }
    if (f->ip >= f->ntok) {
        fiber_kill(f);
    }
}

int dinkc_vm_start(const char *src, size_t n, int sprite)
{
    int s, ip;
    struct Fiber *f;

    if (src == NULL || n == 0) {
        return -1;
    }
    if (!g_var_ready) {
        dinkc_var_init();
        g_var_ready = 1;
    }
    for (s = 1; s <= DINKC_MAX_LIVE; s++) {
        if (!g_f[s].used) {
            break;
        }
    }
    if (s > DINKC_MAX_LIVE) {
        printf("dinkc vm full\n");
        return -1;
    }
    f = &g_f[s];
    memset(f, 0, sizeof(*f));
    f->src = (char *)malloc(n + 1);
    if (f->src == NULL) {
        return -1;
    }
    memcpy(f->src, src, n);
    f->src[n] = '\0';
    f->srclen = n;
    f->sprite = sprite;
    f->used = 1;
    f->depth = 1;
    if (fill_toks(f) != 0) {
        fiber_kill(f);
        return -1;
    }
    ip = find_main(f);
    if (ip < 0) {
        printf("dinkc vm no main\n");
        fiber_kill(f);
        return -1;
    }
    f->ip = ip;
    snprintf(f->name, sizeof(f->name), "slot%d", s);
    f->state = DINKC_RUN;
    run_fiber(f, 0);
    return s;
}

void dinkc_vm_kill(int slot)
{
    if (slot < 1 || slot > DINKC_MAX_LIVE) {
        return;
    }
    fiber_kill(&g_f[slot]);
}

void dinkc_vm_reset(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        fiber_kill(&g_f[i]);
    }
    g_ops_ovf = 0;
    dinkc_var_init();
    g_var_ready = 1;
}

void dinkc_vm_tick(int now_ms)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        struct Fiber *f = &g_f[i];

        if (!f->used) {
            continue;
        }
        if (f->state == DINKC_WAIT_MS && now_ms >= f->wait_until) {
            run_fiber(f, now_ms);
        } else if (f->state == DINKC_WAIT_MOVE) {
            run_fiber(f, now_ms);
        }
    }
}

void dinkc_vm_advance_say(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_f[i].used && g_f[i].state == DINKC_WAIT_SAY) {
            run_fiber(&g_f[i], g_f[i].wait_until);
        }
    }
}

int dinkc_vm_waiting_say(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_f[i].used && g_f[i].state == DINKC_WAIT_SAY) {
            return 1;
        }
    }
    return 0;
}

void dinkc_vm_choice_done(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_f[i].used && g_f[i].state == DINKC_WAIT_CHOICE) {
            run_fiber(&g_f[i], g_f[i].wait_until);
        }
    }
}

int dinkc_vm_live(void)
{
    int i, n = 0;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_f[i].used) {
            n++;
        }
    }
    return n;
}

int dinkc_vm_state(int slot)
{
    if (slot < 1 || slot > DINKC_MAX_LIVE || !g_f[slot].used) {
        return DINKC_DEAD;
    }
    return g_f[slot].state;
}
