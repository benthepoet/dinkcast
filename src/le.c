/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "le.h"

#include <string.h>

int le_u16(const uint8_t *p, size_t n, size_t off, uint16_t *out)
{
    if (p == NULL || out == NULL || off + 2 > n) {
        return -1;
    }
    *out = (uint16_t)(p[off] | ((uint16_t)p[off + 1] << 8));
    return 0;
}

int le_u32(const uint8_t *p, size_t n, size_t off, uint32_t *out)
{
    if (p == NULL || out == NULL || off + 4 > n) {
        return -1;
    }
    *out = (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) |
           ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
    return 0;
}

int le_i32(const uint8_t *p, size_t n, size_t off, int32_t *out)
{
    uint32_t u;

    if (le_u32(p, n, off, &u) != 0) {
        return -1;
    }
    memcpy(out, &u, sizeof(u));
    return 0;
}

int fread_exact(FILE *fp, void *dst, size_t n)
{
    return (fp != NULL && dst != NULL && fread(dst, 1, n, fp) == n) ? 0 : -1;
}
