/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_FS_H
#define DINKCAST_FS_H

#include <stdio.h>

#define DINK_FS_PATH_MAX 512

/* Bite 1.1. Probe /pc/dink, /cd/dink, then compile-time or env DINK_DATA. */
int dink_fs_init(void);
const char *dink_fs_root(void);

/* Open rel under the resolved root. Tries exact, case-fold, then 8.3. */
FILE *dink_fopen(const char *rel, const char *mode);

/* Host-testable pieces (also used on DC). */
int dink_fs_join(char *dst, size_t dstsz, const char *root, const char *rel);
void dink_fs_set_probe_roots(const char *pc, const char *cd, const char *fallback);
int dink_fs_exists_dir(const char *path);

#endif
