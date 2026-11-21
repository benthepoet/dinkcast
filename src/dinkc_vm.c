/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_vm.h"

#include "choice.h"
#include "dinkc_cmd.h"
#include "dinkc_lex.h"
#include "dinkc_var.h"
#include "fade.h"
#include "save.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DINKC_MAX_TOK 4096

struct Fiber {
    int used;
    int state;
    int sprite;
    int wait_until;
    int wait_child;
    int move_spr;
    int depth;
    int ip;
    int ntok;
    int arg[8];
    int keep;
    char name[32];
    /* Borrowed from the shared code entry (never written after setup). */
    const char *src;
    size_t srclen;
    const struct DinkcTok *tok;
    struct DinkcCode *code;
};

/* One copy of each distinct script's source and tokens, shared by every
 * instance (27 s4-duck sprites on map 536 hold one copy, not 27). Tokens
 * are sized to the script, not DINKC_MAX_TOK. */
struct DinkcCode {
    int refs;
    size_t n;
    uint32_t hash;
    char *src;
    struct DinkcTok *tok;
    int ntok;
};

/* FreeDink dinkc.cpp MAX_CALLBACKS. */
#define CB_N 100
static struct {
    int active;
    int owner;
    int minv;
    int maxv;
    int timer;
    char proc[32];
} g_cb[CB_N];

static struct Fiber g_f[DINKC_MAX_LIVE + 1];
static struct DinkcCode g_code[DINKC_MAX_LIVE + 1];
static struct DinkcTok g_tok_scratch[DINKC_MAX_TOK]; /* lex once, copy exact */
/* FreeDink keeps one choice menu (game state, not per script). */
static int g_ch_n, g_ch_cur, g_ch_ret[20];
/* Slot of the fiber the menu belongs to (the latest choice_start), 0 =
 * none. FreeDink: a later script's choice owns the menu; an earlier one
 * still waiting is left orphaned, never resumed with another's lines. */
static int g_ch_owner;
/* FreeDink rinfo->name: the file the next start came from (load_script). */
static char g_name_next[32];
static char g_ch_line[20][80];
static struct Fiber *g_running;
static int g_ops_ovf;
static int g_now_ms;
static int g_var_ready;
static char g_choice_title[3000];
static int g_choice_newy = DINK_CHOICE_NEWY_NONE;
static int g_choice_color;
static int g_sa_on;
static int g_sa_n;
static int g_sa_cur;
static char g_sa_line[20][80];
static int g_last_talk;

static int fiber_slot(const struct Fiber *f)
{
    return (int)(f - g_f);
}

static int start_fiber_args(const char *src, size_t n, int sprite,
                            const char *proc, int keep, const int *args,
                            int nargs);

/* draw_screen → kill_all must not fiber_kill the caller mid-dinkc_cmd. */
static int run_cmd(struct Fiber *f, const char *name, int *args, int nargs,
                   const char *str, const char *str2, int *yield, int *ret)
{
    struct Fiber *prev = g_running;
    int ok;

    g_running = f;
    dinkc_cmd_bind_fiber(fiber_slot(f), f->sprite);
    ok = dinkc_cmd(name, args, nargs, str, str2, yield, ret);
    dinkc_cmd_bind_arg0_current(0);
    g_running = prev;
    return ok;
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
static void parse_args(struct Fiber *f, int *args, int *nargs, char *str,
                       size_t strsz, char *str2, size_t str2sz);

static void choice_meta_clear(void)
{
    g_choice_title[0] = '\0';
    g_choice_newy = DINK_CHOICE_NEWY_NONE;
    g_choice_color = 0;
}

static void replace_str(char *s, const char *from, const char *to)
{
    size_t fl, tl;
    char *p;

    if (s == NULL || from == NULL || to == NULL || from[0] == '\0') {
        return;
    }
    fl = strlen(from);
    tl = strlen(to);
    p = s;
    while ((p = strstr(p, from)) != NULL) {
        memmove(p + tl, p + fl, strlen(p + fl) + 1);
        memcpy(p, to, tl);
        p += tl != 0 ? tl : 1;
    }
}

static void title_from_src(const char *beg, const char *end)
{
    char raw[3000];
    size_t n, o = 0;
    int i, ls;

    g_choice_title[0] = '\0';
    if (beg == NULL || end == NULL || end <= beg) {
        return;
    }
    n = (size_t)(end - beg);
    if (n >= sizeof(raw)) {
        n = sizeof(raw) - 1;
    }
    memcpy(raw, beg, n);
    raw[n] = '\0';
    i = 0;
    while (raw[i] != '\0' && o + 2 < sizeof(g_choice_title)) {
        ls = i;
        while (raw[i] != '\0' && raw[i] != '\n' && raw[i] != '\r') {
            i++;
        }
        while (ls < i && (raw[ls] == ' ' || raw[ls] == '\t')) {
            ls++;
        }
        while (o + (size_t)(i - ls) + 2 < sizeof(g_choice_title)) {
            int k;

            for (k = ls; k < i; k++) {
                g_choice_title[o++] = raw[k];
            }
            g_choice_title[o++] = '\n';
            break;
        }
        if (raw[i] == '\r') {
            i++;
        }
        if (raw[i] == '\n') {
            i++;
        }
    }
    g_choice_title[o] = '\0';
    replace_str(g_choice_title, "\n\n\n\n", "\n \n");
    replace_str(g_choice_title, "\n\n", "\n");
}

static int parse_choice_int(struct Fiber *f)
{
    char buf[32];

    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LPAREN) {
        int v;

        f->ip++;
        v = eval_expr(f);
        if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_RPAREN) {
            f->ip++;
        }
        return v;
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_NUMBER) {
        copy_tok(&f->tok[f->ip], buf, sizeof(buf));
        f->ip++;
        return atoi(buf);
    }
    return 0;
}

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
        char cname[40], sarg[192], sarg2[192];
        int args[8], nargs = 0, yld = 0, rv = 0;

        copy_tok(&f->tok[f->ip], cname, sizeof(cname));
        f->ip++;
        if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_LPAREN) {
            parse_args(f, args, &nargs, sarg, sizeof(sarg), sarg2,
                       sizeof(sarg2));
            if (run_cmd(f, cname, args, nargs, sarg, sarg2, &yld, &rv)) {
                dinkc_var_set("&return", rv, DINKC_GLOBAL_SCOPE, f->sprite);
                return rv;
            }
            printf("dinkc unimplemented %s\n", cname);
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

static int tok_is_var(const struct DinkcTok *t, const char *w)
{
    size_t i, n;
    const char *p;
    size_t pn;

    if (t->kind != DINKC_VAR || w == NULL) {
        return 0;
    }
    n = strlen(w);
    p = t->p;
    pn = t->n;
    if (pn > 0 && p[0] == '&') {
        p++;
        pn--;
    }
    if (pn != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)p[i]) != tolower((unsigned char)w[i])) {
            return 0;
        }
    }
    return 1;
}

static void parse_args(struct Fiber *f, int *args, int *nargs, char *str,
                       size_t strsz, char *str2, size_t str2sz)
{
    int nstr = 0;
    int arg0_current = 0;

    *nargs = 0;
    if (str != NULL && strsz > 0) {
        str[0] = '\0';
    }
    if (str2 != NULL && str2sz > 0) {
        str2[0] = '\0';
    }
    if (f->ip >= f->ntok || f->tok[f->ip].kind != DINKC_LPAREN) {
        return;
    }
    f->ip++;
    while (f->ip < f->ntok && f->tok[f->ip].kind != DINKC_RPAREN) {
        if (f->tok[f->ip].kind == DINKC_COMMA) {
            f->ip++;
            continue;
        }
        if (f->tok[f->ip].kind == DINKC_STRING) {
            char *dest = NULL;
            size_t destsz = 0;
            size_t n = f->tok[f->ip].n;
            const char *p = f->tok[f->ip].p;

            if (nstr == 0) {
                dest = str;
                destsz = strsz;
            } else if (nstr == 1) {
                dest = str2;
                destsz = str2sz;
            }
            nstr++;
            if (dest != NULL && destsz > 1) {
                if (n >= 1 && (p[0] == '"' || p[0] == '\'')) {
                    p++;
                    n--;
                }
                if (n >= 1 && (p[n - 1] == '"' || p[n - 1] == '\'')) {
                    n--;
                }
                if (n >= destsz) {
                    n = destsz - 1;
                }
                memcpy(dest, p, n);
                dest[n] = '\0';
            }
            if (*nargs < 8) {
                args[(*nargs)++] = 0;
            }
            f->ip++;
            continue;
        }
        if (*nargs == 0 && tok_is_var(&f->tok[f->ip], "current_sprite")) {
            arg0_current = 1;
        }
        if (*nargs < 8) {
            args[(*nargs)++] = eval_expr(f);
        } else {
            (void)eval_expr(f);
        }
    }
    if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_RPAREN) {
        f->ip++;
    }
    eat_semi(f);
    dinkc_cmd_bind_arg0_current(arg0_current);
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

static uint32_t code_hash(const char *src, size_t n)
{
    uint32_t h = 2166136261u; /* FNV-1a */
    size_t i;

    for (i = 0; i < n; i++) {
        h = (h ^ (uint8_t)src[i]) * 16777619u;
    }
    return h;
}

void dinkc_vm_name_next(const char *file)
{
    snprintf(g_name_next, sizeof(g_name_next), "%s", file != NULL ? file : "");
}

const char *dinkc_vm_name(int slot)
{
    if (slot < 1 || slot > DINKC_MAX_LIVE || !g_f[slot].used) {
        return NULL;
    }
    return g_f[slot].name;
}

int dinkc_vm_code_count(void)
{
    int i, n = 0;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        n += g_code[i].refs > 0;
    }
    return n;
}

size_t dinkc_vm_code_bytes(void)
{
    size_t b = 0;
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_code[i].refs > 0) {
            b += g_code[i].n + 1 +
                 sizeof(struct DinkcTok) * (size_t)g_code[i].ntok;
        }
    }
    return b;
}

static void code_release(struct DinkcCode *c)
{
    if (c == NULL || c->refs <= 0) {
        return;
    }
    if (--c->refs == 0) {
        free(c->src);
        free(c->tok);
        memset(c, 0, sizeof(*c));
    }
}

/* Share an existing copy of this script, or copy + lex it once. */
static int acquire_code(struct Fiber *f, const char *src, size_t n)
{
    struct DinkcLex lx;
    struct DinkcTok t;
    struct DinkcCode *c = NULL;
    uint32_t h = code_hash(src, n);
    int i, rc, nt = 0;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_code[i].refs > 0 && g_code[i].n == n && g_code[i].hash == h &&
            memcmp(g_code[i].src, src, n) == 0) {
            c = &g_code[i];
            break;
        }
    }
    if (c == NULL) {
        for (i = 1; i <= DINKC_MAX_LIVE && g_code[i].refs > 0; i++) {
        }
        if (i > DINKC_MAX_LIVE) {
            return -1;
        }
        c = &g_code[i];
        c->src = (char *)malloc(n + 1);
        if (c->src == NULL) {
            return -1;
        }
        memcpy(c->src, src, n);
        c->src[n] = '\0';
        dinkc_lex_init(&lx, c->src, n);
        for (;;) {
            rc = dinkc_lex_next(&lx, &t);
            if (rc == 1) {
                break;
            }
            if (rc < 0 || nt >= DINKC_MAX_TOK) {
                free(c->src);
                c->src = NULL;
                return -1;
            }
            g_tok_scratch[nt++] = t;
        }
        c->tok = (struct DinkcTok *)malloc(sizeof(struct DinkcTok) *
                                           (size_t)(nt > 0 ? nt : 1));
        if (c->tok == NULL) {
            free(c->src);
            c->src = NULL;
            return -1;
        }
        memcpy(c->tok, g_tok_scratch, sizeof(struct DinkcTok) * (size_t)nt);
        c->ntok = nt;
        c->n = n;
        c->hash = h;
    }
    c->refs++;
    f->code = c;
    f->src = c->src;
    f->srclen = c->n;
    f->tok = c->tok;
    f->ntok = c->ntok;
    return 0;
}

static int find_proc(struct Fiber *f, const char *proc)
{
    int i;

    if (proc == NULL) {
        proc = "main";
    }
    for (i = 0; i + 1 < f->ntok; i++) {
        if (tok_is(&f->tok[i], "void") && tok_is(&f->tok[i + 1], proc)) {
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

/* FreeDink locate_goto: from the start of the script, first word of a
 * line matching "name:". skipnext/onlevel/level reset; we reset depth. */
static int locate_goto(struct Fiber *f, const char *name)
{
    int i;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (i = 0; i + 1 < f->ntok; i++) {
        if (f->tok[i + 1].kind == DINKC_COLON && tok_is(&f->tok[i], name)) {
            f->ip = i + 2;
            f->depth = 0;
            return 1;
        }
    }
    return 0;
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

static void fiber_kill(struct Fiber *f)
{
    int i, slot;

    slot = (int)(f - g_f);
    if (g_last_talk == slot) {
        g_last_talk = 0;
    }
    if (g_ch_owner == slot) {
        g_ch_owner = 0;
    }
    if (f->used) {
        dinkc_var_kill_scope(slot);
        for (i = 1; i < CB_N; i++) {
            if (g_cb[i].active && g_cb[i].owner == slot) {
                g_cb[i].active = 0;
            }
        }
    }
    code_release(f->code);
    memset(f, 0, sizeof(*f));
}

static void run_fiber(struct Fiber *f, int now_ms)
{
    int ops = 0;

    if (now_ms == 0) {
        now_ms = g_now_ms;
    }
    f->state = DINKC_RUN;
    while (f->used && f->state == DINKC_RUN && f->ip < f->ntok) {
        const struct DinkcTok *t = &f->tok[f->ip];

        ops++;
        if (ops > DINKC_OPS_CAP) {
            g_ops_ovf++;
            printf("dinkc vm ops cap slot overflow\n");
            return;
        }
        /* FreeDink process_line: level never stays < 1. Extra '}' does
         * not end the proc (s1-h2-o talk has one before unfreeze). */
        if (f->depth < 1) {
            f->depth = 1;
        }
        if (t->kind == DINKC_LBRACE) {
            f->depth++;
            f->ip++;
            continue;
        }
        if (t->kind == DINKC_RBRACE) {
            f->depth--;
            f->ip++;
            if (f->depth < 0) {
                f->depth = 0;
            }
            continue;
        }
        if (tok_is(t, "kill_this_task")) {
            fiber_kill(f);
            return;
        }
        if (tok_is(t, "void") || tok_is(t, "return")) {
            /* FreeDink return ends the proc; keep fibers stay for locate(USE). */
            if (f->keep) {
                f->state = DINKC_IDLE;
                return;
            }
            fiber_kill(f);
            return;
        }
        if (tok_is(t, "goto")) {
            char name[40];

            f->ip++;
            name[0] = '\0';
            if (f->ip < f->ntok && f->tok[f->ip].kind == DINKC_IDENT) {
                copy_tok(&f->tok[f->ip], name, sizeof(name));
                f->ip++;
            }
            eat_semi(f);
            if (!locate_goto(f, name)) {
                printf("dinkc cannot goto %s:\n", name);
                /* FreeDink scan left current at EOF; do not fall through. */
                f->ip = f->ntok;
            }
            continue;
        }
        /* label: FreeDink process_line DCPS_GOTO_NEXTLINE */
        if (t->kind == DINKC_IDENT && f->ip + 1 < f->ntok &&
            f->tok[f->ip + 1].kind == DINKC_COLON) {
            f->ip += 2;
            continue;
        }
        if (t->kind == DINKC_IDENT && f->ip + 1 < f->ntok &&
            f->tok[f->ip + 1].kind == DINKC_LPAREN && !tok_is(t, "if") &&
            !tok_is(t, "while") && !tok_is(t, "int")) {
            char cname[40], sarg[192], sarg2[192];
            int args[8], nargs = 0, yld = 0, rv = 0;

            copy_tok(t, cname, sizeof(cname));
            f->ip++;
            if (tok_is(t, "wait")) {
                int ms;

                parse_args(f, args, &nargs, sarg, sizeof(sarg), sarg2,
                           sizeof(sarg2));
                ms = nargs > 0 ? args[0] : 1;
                if (ms < 1) {
                    ms = 1;
                }
                f->state = DINKC_WAIT_MS;
                f->wait_until = g_now_ms + ms;
                return;
            }
            if (tok_is(t, "move_stop")) {
                parse_args(f, args, &nargs, sarg, sizeof(sarg), sarg2,
                           sizeof(sarg2));
                (void)run_cmd(f, "move", args, nargs, sarg, sarg2, &yld, &rv);
                f->move_spr = nargs > 0 ? args[0] : 0;
                if (!dinkc_cmd_move_busy(f->move_spr)) {
                    continue;
                }
                f->state = DINKC_WAIT_MOVE;
                return;
            }
            if (tok_is(t, "choice_start")) {
                /* dinkc_get_choices: retnum even if condition false. */
                int retnum = 0;

                parse_args(f, args, &nargs, sarg, sizeof(sarg), sarg2,
                           sizeof(sarg2));
                /* FreeDink dinkc_get_choices: game_choice_clear() first, so
                 * an earlier menu is dropped even if this one ends empty. */
                g_ch_n = 0;
                g_ch_owner = 0;
                choice_meta_clear();
                while (f->ip < f->ntok &&
                       !tok_is(&f->tok[f->ip], "choice_end")) {
                    int ok = 1;

                    if (tok_is(&f->tok[f->ip], "set_y")) {
                        f->ip++;
                        g_choice_newy = parse_choice_int(f);
                        eat_semi(f);
                        continue;
                    }
                    if (tok_is(&f->tok[f->ip], "set_title_color")) {
                        f->ip++;
                        g_choice_color = parse_choice_int(f);
                        eat_semi(f);
                        continue;
                    }
                    if (tok_is(&f->tok[f->ip], "title_start")) {
                        const char *beg = NULL;

                        f->ip++;
                        skip_call(f);
                        if (f->ip < f->ntok) {
                            beg = f->tok[f->ip].p;
                        }
                        while (f->ip < f->ntok &&
                               !tok_is(&f->tok[f->ip], "title_end")) {
                            f->ip++;
                        }
                        if (beg != NULL && f->ip < f->ntok) {
                            title_from_src(beg, f->tok[f->ip].p);
                        }
                        if (f->ip < f->ntok) {
                            f->ip++;
                            skip_call(f);
                        }
                        continue;
                    }
                    while (f->ip < f->ntok &&
                           f->tok[f->ip].kind == DINKC_LPAREN) {
                        int cond;

                        f->ip++;
                        cond = eval_expr(f);
                        if (f->ip < f->ntok &&
                            f->tok[f->ip].kind == DINKC_RPAREN) {
                            f->ip++;
                        }
                        if (!cond) {
                            ok = 0;
                        }
                    }
                    if (f->ip < f->ntok &&
                        f->tok[f->ip].kind == DINKC_STRING) {
                        char line[80];

                        retnum++;
                        copy_tok(&f->tok[f->ip], line, sizeof(line));
                        if (ok && g_ch_n < 20) {
                            const char *p = line;
                            size_t ln = strlen(line);

                            if (ln >= 1 && (p[0] == '"' || p[0] == '\'')) {
                                p++;
                                ln--;
                            }
                            if (ln >= 1 &&
                                (p[ln - 1] == '"' || p[ln - 1] == '\'')) {
                                ln--;
                            }
                            if (ln > 79) {
                                ln = 79;
                            }
                            memcpy(g_ch_line[g_ch_n], p, ln);
                            g_ch_line[g_ch_n][ln] = '\0';
                            if (strcmp(g_ch_line[g_ch_n],
                                       "&savegameinfo") == 0) {
                                save_info_line(retnum, g_ch_line[g_ch_n],
                                               sizeof(g_ch_line[0]));
                            }
                            g_ch_ret[g_ch_n] = retnum;
                            g_ch_n++;
                            printf("choice %d %s\n", retnum, g_ch_line[g_ch_n - 1]);
                        }
                        f->ip++;
                        eat_semi(f);
                        continue;
                    }
                    f->ip++;
                }
                if (f->ip < f->ntok) {
                    f->ip++;
                    skip_call(f);
                }
                if (g_ch_n == 0) {
                    printf("dinkc choice empty\n");
                    continue;
                }
                g_ch_cur = 1;
                f->state = DINKC_WAIT_CHOICE;
                g_ch_owner = fiber_slot(f);
                printf("dinkc yield choice n=%d\n", g_ch_n);
                return;
            }
            parse_args(f, args, &nargs, sarg, sizeof(sarg), sarg2,
                       sizeof(sarg2));
            if (run_cmd(f, cname, args, nargs, sarg, sarg2, &yld, &rv)) {
                dinkc_var_set("&return", rv, DINKC_GLOBAL_SCOPE, f->sprite);
                if (cname[0] == 's' && strstr(cname, "script_attach") != NULL) {
                    f->sprite = nargs > 0 ? args[0] : f->sprite;
                }
                if (cname[0] == 'f' && strstr(cname, "force_vision") != NULL) {
                    f->sprite = 1000;
                }
                if (yld == 3) {
                    fiber_kill(f);
                    return;
                }
                if (yld == 1) {
                    f->state = DINKC_WAIT_SAY;
                    printf("dinkc yield say\n");
                    return;
                }
                if (yld == 6) {
                    f->state = DINKC_WAIT_BMP;
                    printf("dinkc yield bmp\n");
                    return;
                }
                if (yld == 7) {
                    f->state = DINKC_WAIT_FADE;
                    printf("dinkc yield fade\n");
                    return;
                }
                if (yld == 5) {
                    /* FreeDink run_script(child) then proc_return: sheart()
                     * is already done, so HIT must reach sp_hard this tick. */
                    if (!dinkc_vm_used(rv)) {
                        continue;
                    }
                    f->wait_child = rv;
                    f->state = DINKC_WAIT_EXT;
                    return;
                }
                continue;
            }
            /* FreeDink: ident( is a same-file proc (S2-OUT buybomb). */
            if (find_proc(f, cname) >= 0) {
                int ch;

                /* Same rinfo in FreeDink: the child keeps the file name. */
                snprintf(g_name_next, sizeof(g_name_next), "%s", f->name);
                ch = start_fiber_args(f->src, f->srclen, f->sprite, cname,
                                          0, args, nargs);

                if (ch > 0) {
                    if (dinkc_vm_used(ch)) {
                        f->wait_child = ch;
                        f->state = DINKC_WAIT_EXT;
                        return;
                    }
                    continue;
                }
            }
            printf("dinkc unimplemented %s\n", cname);
            continue;
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
        if (f->keep) {
            f->state = DINKC_IDLE;
            return;
        }
        fiber_kill(f);
    }
}

void dinkc_vm_set_args(int slot, const int *args, int n)
{
    int i;

    if (slot < 1 || slot > DINKC_MAX_LIVE || !g_f[slot].used) {
        return;
    }
    for (i = 0; i < 8; i++) {
        g_f[slot].arg[i] = (args != NULL && i < n) ? args[i] : 0;
    }
}

int dinkc_vm_arg(int slot, int n1)
{
    if (slot < 1 || slot > DINKC_MAX_LIVE || n1 < 1 || n1 > 8) {
        return 0;
    }
    return g_f[slot].arg[n1 - 1];
}

int dinkc_vm_used(int slot)
{
    if (slot < 1 || slot > DINKC_MAX_LIVE) {
        return 0;
    }
    return g_f[slot].used;
}

int dinkc_vm_sprite_fiber(int sprite)
{
    int i, found = 0;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (!g_f[i].used || g_f[i].sprite != sprite) {
            continue;
        }
        if (g_f[i].keep) {
            return i;
        }
        if (found == 0) {
            found = i;
        }
    }
    return found;
}

void dinkc_vm_kill_returning(int slot)
{
    int i;

    if (slot < 1 || slot > DINKC_MAX_LIVE || !g_f[slot].used) {
        return;
    }
    for (i = 1; i < CB_N; i++) {
        if (g_cb[i].active && g_cb[i].owner == slot) {
            g_cb[i].active = 0;
        }
    }
}

static int vm_add_cb(const char *proc, int base, int range, int fiber,
                     int sprite)
{
    int k;

    (void)sprite;
    if (proc == NULL || proc[0] == '\0') {
        return 0;
    }
    for (k = 1; k < CB_N; k++) {
        if (!g_cb[k].active) {
            memset(&g_cb[k], 0, sizeof(g_cb[k]));
            g_cb[k].active = 1;
            g_cb[k].owner = fiber;
            g_cb[k].minv = base;
            g_cb[k].maxv = range;
            strncpy(g_cb[k].proc, proc, sizeof(g_cb[k].proc) - 1);
            return k;
        }
    }
    return 0;
}

void dinkc_vm_resume_move(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        struct Fiber *f = &g_f[i];

        if (!f->used) {
            continue;
        }
        if (f->state == DINKC_WAIT_MOVE && !dinkc_cmd_move_busy(f->move_spr)) {
            run_fiber(f, g_now_ms);
        } else if (f->state == DINKC_WAIT_EXT && !dinkc_vm_used(f->wait_child)) {
            run_fiber(f, g_now_ms);
        }
    }
}

void dinkc_vm_tick_callbacks(int now_ms)
{
    int k;

    g_now_ms = now_ms;
    for (k = 1; k < CB_N; k++) {
        if (!g_cb[k].active) {
            continue;
        }
        if (g_cb[k].owner < 1 || !g_f[g_cb[k].owner].used) {
            g_cb[k].active = 0;
            continue;
        }
        if (g_cb[k].timer == 0) {
            if (g_cb[k].maxv > 0) {
                g_cb[k].timer = now_ms + (rand() % g_cb[k].maxv) + g_cb[k].minv;
            } else {
                g_cb[k].timer = now_ms + g_cb[k].minv;
            }
        } else if (g_cb[k].timer < now_ms) {
            struct Fiber *own = &g_f[g_cb[k].owner];

            g_cb[k].timer = 0;
            snprintf(g_name_next, sizeof(g_name_next), "%s", own->name);
            (void)dinkc_vm_start_proc(own->src, own->srclen, own->sprite,
                                      g_cb[k].proc);
        }
    }
}

static int start_fiber_args(const char *src, size_t n, int sprite,
                            const char *proc, int keep, const int *args,
                            int nargs)
{
    char fname[32];

    /* Take the pending name now so a failed start cannot leak it. */
    snprintf(fname, sizeof(fname), "%s", g_name_next);
    g_name_next[0] = '\0';
    int s, ip;
    struct Fiber *f;

    if (src == NULL || n == 0) {
        return -1;
    }
    if (!g_var_ready) {
        dinkc_var_init();
        g_var_ready = 1;
        dinkc_cmd_bind_callback(vm_add_cb);
    }
    for (s = 1; s <= DINKC_MAX_LIVE; s++) {
        if (!g_f[s].used) {
            break;
        }
    }
    if (s > DINKC_MAX_LIVE) {
        printf("dinkc vm full (%d scripts, %d distinct, %u B)\n",
               DINKC_MAX_LIVE, dinkc_vm_code_count(),
               (unsigned)dinkc_vm_code_bytes());
        return -1;
    }
    f = &g_f[s];
    memset(f, 0, sizeof(*f));
    if (acquire_code(f, src, n) != 0) {
        memset(f, 0, sizeof(*f));
        return -1;
    }
    f->sprite = sprite;
    f->used = 1;
    f->keep = keep ? 1 : 0;
    f->depth = 1;
    ip = find_proc(f, proc);
    if (ip < 0) {
        printf("dinkc vm no %s\n", proc != NULL ? proc : "main");
        fiber_kill(f);
        return -1;
    }
    f->ip = ip;
    if (fname[0] != '\0') {
        snprintf(f->name, sizeof(f->name), "%s", fname);
    } else {
        snprintf(f->name, sizeof(f->name), "slot%d", s);
    }
    if (args != NULL && nargs > 0) {
        int i, ncp = nargs < 8 ? nargs : 8;

        for (i = 0; i < ncp; i++) {
            f->arg[i] = args[i];
        }
    }
    f->state = DINKC_RUN;
    run_fiber(f, 0);
    return s;
}

static int start_fiber(const char *src, size_t n, int sprite, const char *proc,
                       int keep)
{
    return start_fiber_args(src, n, sprite, proc, keep, NULL, 0);
}

int dinkc_vm_start_proc(const char *src, size_t n, int sprite, const char *proc)
{
    return start_fiber(src, n, sprite, proc, 0);
}

int dinkc_vm_start_keep(const char *src, size_t n, int sprite, const char *proc)
{
    return start_fiber(src, n, sprite, proc, 1);
}

int dinkc_vm_locate(int slot, const char *proc)
{
    struct Fiber *f;
    int ip;

    if (slot < 1 || slot > DINKC_MAX_LIVE || !g_f[slot].used) {
        return -1;
    }
    f = &g_f[slot];
    ip = find_proc(f, proc);
    if (ip < 0) {
        printf("dinkc locate no %s\n", proc != NULL ? proc : "");
        return -1;
    }
    f->ip = ip;
    f->depth = 1;
    f->state = DINKC_RUN;
    run_fiber(f, 0);
    return slot;
}

int dinkc_vm_start(const char *src, size_t n, int sprite)
{
    return dinkc_vm_start_proc(src, n, sprite, "main");
}

void dinkc_vm_kill(int slot)
{
    if (slot < 1 || slot > DINKC_MAX_LIVE) {
        return;
    }
    fiber_kill(&g_f[slot]);
}

void dinkc_vm_kill_sprite(int sprite)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_f[i].used && g_f[i].sprite == sprite) {
            fiber_kill(&g_f[i]);
        }
    }
}

void dinkc_vm_set_now(int now_ms)
{
    if (now_ms > 0) {
        g_now_ms = now_ms;
    }
    dinkc_cmd_set_now(g_now_ms);
}

void dinkc_vm_kill_all(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        /* FreeDink kill_all_scripts: skip sprite 1000. Engine 1001+
         * is the same class (GOTCHAS). Skip the running fiber so
         * draw_screen can kill_all then yield=3 after return. */
        if (g_running == &g_f[i]) {
            continue;
        }
        if (g_f[i].used && g_f[i].sprite >= 1000) {
            continue;
        }
        fiber_kill(&g_f[i]);
    }
}

void dinkc_vm_reset(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        fiber_kill(&g_f[i]);
    }
    memset(g_cb, 0, sizeof(g_cb));
    g_ch_owner = 0;
    g_ops_ovf = 0;
    dinkc_var_init();
    g_var_ready = 1;
    dinkc_cmd_bind_callback(vm_add_cb);
    choice_meta_clear();
    dinkc_vm_choice_close_saves();
    fade_reset();
}

void dinkc_vm_tick(int now_ms)
{
    int i;

    g_now_ms = now_ms;
    dinkc_cmd_set_now(now_ms);
    dinkc_vm_tick_callbacks(now_ms);
    fade_tick(now_ms);
    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        struct Fiber *f = &g_f[i];

        if (!f->used) {
            continue;
        }
        if (f->state == DINKC_RUN) {
            run_fiber(f, now_ms);
        } else if (f->state == DINKC_WAIT_MS && now_ms >= f->wait_until) {
            run_fiber(f, now_ms);
        } else if (f->state == DINKC_WAIT_EXT &&
                   !dinkc_vm_used(f->wait_child)) {
            run_fiber(f, now_ms);
        } else if (f->state == DINKC_WAIT_FADE && !fade_busy()) {
            run_fiber(f, now_ms);
        }
    }
}

void dinkc_vm_note_last_talk(int fiber)
{
    g_last_talk = (fiber >= 1 && fiber <= DINKC_MAX_LIVE) ? fiber : 0;
}

int dinkc_vm_last_talk(void)
{
    int i = g_last_talk;

    if (i < 1 || i > DINKC_MAX_LIVE || !g_f[i].used ||
        g_f[i].state != DINKC_WAIT_SAY) {
        return 0;
    }
    return i;
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

void dinkc_vm_advance_last_talk(void)
{
    int i = dinkc_vm_last_talk();

    if (i < 1) {
        return;
    }
    run_fiber(&g_f[i], g_f[i].wait_until);
    if (!g_f[i].used || g_f[i].state != DINKC_WAIT_SAY) {
        g_last_talk = 0;
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

void dinkc_vm_advance_bmp(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_f[i].used && g_f[i].state == DINKC_WAIT_BMP) {
            run_fiber(&g_f[i], g_f[i].wait_until);
        }
    }
}

int dinkc_vm_waiting_bmp(void)
{
    int i;

    for (i = 1; i <= DINKC_MAX_LIVE; i++) {
        if (g_f[i].used && g_f[i].state == DINKC_WAIT_BMP) {
            return 1;
        }
    }
    return 0;
}

static struct Fiber *choice_fiber(void)
{
    if (g_ch_owner >= 1 && g_ch_owner <= DINKC_MAX_LIVE &&
        g_f[g_ch_owner].used && g_f[g_ch_owner].state == DINKC_WAIT_CHOICE) {
        return &g_f[g_ch_owner];
    }
    return NULL;
}

void dinkc_vm_choice_open_saves(void)
{
    int i;

    choice_meta_clear();
    g_sa_n = 0;
    for (i = 1; i <= DINK_SAVE_SLOTS && g_sa_n < 20; i++) {
        save_info_line(i, g_sa_line[g_sa_n], sizeof(g_sa_line[0]));
        g_sa_n++;
    }
    if (g_sa_n < 20) {
        snprintf(g_sa_line[g_sa_n], sizeof(g_sa_line[0]), "Nevermind");
        g_sa_n++;
    }
    g_sa_cur = 1;
    g_sa_on = 1;
}

void dinkc_vm_choice_close_saves(void)
{
    g_sa_on = 0;
    g_sa_n = 0;
    g_sa_cur = 0;
}

void dinkc_vm_choice_open_pause(int cur1)
{
    /* ESCAPE.c is a choice overlay. DC pause is Continue / Title only. */
    choice_meta_clear();
    snprintf(g_sa_line[0], sizeof(g_sa_line[0]), "Continue");
    snprintf(g_sa_line[1], sizeof(g_sa_line[1]), "Title");
    g_sa_n = 2;
    g_sa_cur = (cur1 == 2) ? 2 : 1;
    g_sa_on = 1;
}

int dinkc_vm_choice_n(void)
{
    struct Fiber *f = choice_fiber();

    if (f != NULL) {
        return g_ch_n;
    }
    return g_sa_on ? g_sa_n : 0;
}

int dinkc_vm_choice_cur(void)
{
    struct Fiber *f = choice_fiber();

    if (f != NULL) {
        return g_ch_cur;
    }
    return g_sa_on ? g_sa_cur : 0;
}

const char *dinkc_vm_choice_line(int vis1)
{
    struct Fiber *f = choice_fiber();
    int i = vis1 - 1;

    if (f != NULL) {
        if (i < 0 || i >= g_ch_n) {
            return "";
        }
        return g_ch_line[i];
    }
    if (!g_sa_on || i < 0 || i >= g_sa_n) {
        return "";
    }
    return g_sa_line[i];
}

const char *dinkc_vm_choice_title(void)
{
    return g_choice_title;
}

int dinkc_vm_choice_newy(void)
{
    return g_choice_newy;
}

int dinkc_vm_choice_color(void)
{
    return g_choice_color;
}

void dinkc_vm_choice_move(int delta)
{
    struct Fiber *f = choice_fiber();
    int n, c, *cur;

    if (f != NULL && g_ch_n > 0) {
        n = g_ch_n;
        cur = &g_ch_cur;
    } else if (g_sa_on && g_sa_n > 0) {
        n = g_sa_n;
        cur = &g_sa_cur;
    } else {
        return;
    }
    c = *cur + delta;
    while (c < 1) {
        c += n;
    }
    while (c > n) {
        c -= n;
    }
    *cur = c;
}

void dinkc_vm_choice_pick(int result)
{
    struct Fiber *f = choice_fiber();
    int vis = result, off;

    /* result is 1-based visible index; &result is official line number. */
    if (f == NULL) {
        return;
    }
    if (vis < 1) {
        vis = 1;
    }
    if (vis > g_ch_n) {
        vis = g_ch_n;
    }
    off = vis - 1;
    if (off >= 0 && off < g_ch_n) {
        dinkc_var_set("&result", g_ch_ret[off], DINKC_GLOBAL_SCOPE, 1);
    } else {
        dinkc_var_set("&result", 0, DINKC_GLOBAL_SCOPE, 1);
    }
    g_ch_owner = 0;
    run_fiber(f, f->wait_until);
}

int dinkc_vm_waiting_choice(void)
{
    return choice_fiber() != NULL;
}

void dinkc_vm_choice_done(void)
{
    dinkc_vm_choice_pick(dinkc_vm_choice_cur());
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
