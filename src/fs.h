/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_FS_H
#define DINKCAST_FS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DINK_FS_PATH_MAX 512

/* Bite 1.1. Probe /pc/dink, /cd/dink, then compile-time or env DINK_DATA. */
int dink_fs_init(void);
const char *dink_fs_root(void);

/* Open rel under the resolved root. Tries exact, case-fold, then 8.3. */
FILE *dink_fopen(const char *rel, const char *mode);
/* ISO9660/Flycast: no SEEK_END; 8 KiB fread, no yield mid-file. */
int dink_fread_all(FILE *fp, uint8_t **out, size_t *n);
int dink_pread(FILE *fp, long off, uint8_t *dst, size_t n);
/* Copy-out slurp. Repeat path does not fopen. Caller frees *out. */
int dink_slurp_rel(const char *rel, uint8_t **out, size_t *n);
/* Session cache. Pointer valid until dink_blob_clear or dink_blob_try_drop
 * of that rel. Do not free the pointer. Never evicts a live slot to insert
 * another (ff/hard borrow it) — grow the table instead. */
int dink_blob_get(const char *rel, const uint8_t **ptr, size_t *n);
/* Free one blob after the borrower (ff slot) is gone. 0 if dropped or absent. */
int dink_blob_try_drop(const char *rel);
size_t dink_blob_bytes(void);
int dink_disc_opens(void);
void dink_disc_note_open(void);
void dink_blob_clear(void);
int dink_fread_n(FILE *fp, uint8_t *dst, size_t n);
void dink_cd_yield(void);
void dink_cd_settle(void);

/* Host-testable pieces (also used on DC). */
int dink_fs_join(char *dst, size_t dstsz, const char *root, const char *rel);
void dink_fs_set_probe_roots(const char *pc, const char *cd, const char *fallback);
int dink_fs_exists_dir(const char *path);

#endif
