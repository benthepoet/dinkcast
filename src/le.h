/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_LE_H
#define DINKCAST_LE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int le_u16(const uint8_t *p, size_t n, size_t off, uint16_t *out);
int le_i32(const uint8_t *p, size_t n, size_t off, int32_t *out);
int le_u32(const uint8_t *p, size_t n, size_t off, uint32_t *out);
int fread_exact(FILE *fp, void *dst, size_t n);

#endif
