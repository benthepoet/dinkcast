/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_PARSE_H
#define DINKCAST_DINKC_PARSE_H

#include <stddef.h>

#define DINKC_MAX_PROC 64

struct DinkcProc {
    char name[32];
    int line;
};

struct DinkcProg {
    int nproc;
    int nstmt;
    struct DinkcProc proc[DINKC_MAX_PROC];
};

/* 0 ok. err is optional, filled on fail. Unknown commands are calls. */
int dinkc_parse(const char *src, size_t n, struct DinkcProg *out, char *err,
                size_t errsz);

#endif
