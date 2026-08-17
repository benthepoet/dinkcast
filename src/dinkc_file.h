/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_DINKC_FILE_H
#define DINKCAST_DINKC_FILE_H

#include <stddef.h>

#define DINKC_FILE_CAP 32768

/* story/NAME.c from map.dat script field (no ext, or strip one). */
int dinkc_story_rel(const char *name, char *rel, size_t relsz);
/* malloc *out, NUL-terminated, n = bytes. 0 ok. Logs miss/oversize. */
int dinkc_load(const char *name, char **out, size_t *n);
void dinkc_free(char *p);

#endif
