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

/* Open rel under the resolved root. Tries exact, case-fold, then 8.3.
 * Host: DINK_DISTILL overlay is tried first (14.5 subset dir.ff). */
FILE *dink_fopen(const char *rel, const char *mode);
/* ISO9660/Flycast: fstat size (no SEEK_END); 8 KiB fread, no yield mid-file. */
int dink_fread_all(FILE *fp, uint8_t **out, size_t *n);
int dink_pread(FILE *fp, long off, uint8_t *dst, size_t n);
/* Copy-out slurp. Repeat path does not fopen. Caller frees *out. */
int dink_slurp_rel(const char *rel, uint8_t **out, size_t *n);
/* Session cache. Pointer valid until dink_blob_clear or dink_blob_try_drop
 * of that rel. Do not free the pointer. Never evicts a live slot to insert
 * another (ff/hard borrow it) — grow the table instead. */
int dink_blob_get(const char *rel, const uint8_t **ptr, size_t *n);
/* Take ownership of data; charges file_blob. 0 on ok. */
int dink_blob_put(const char *rel, uint8_t *data, size_t n);
int dink_fp_size(FILE *fp, size_t *n);
/* Free one blob after the borrower (ff slot) is gone. 0 if dropped or absent. */
int dink_blob_try_drop(const char *rel);
size_t dink_blob_bytes(void);
int dink_blob_slot(int i, const char **rel, size_t *n);
void dink_blob_set_cls(const char *rel, int cls, int age_out);
int dink_blob_get_cls(const char *rel, int *cls, int *age_out);
int dink_disc_opens(void);
void dink_disc_note_open(void);
void dink_blob_clear(void);
int dink_fread_n(FILE *fp, uint8_t *dst, size_t n);
void dink_cd_yield(void);
void dink_cd_settle(void);
/* RAM-only tick (music stream poll). Must not fopen. */
void dink_cd_set_pump(void (*fn)(void));

/* Play-path I/O log (diagnostic only; no policy). Phase "play" is gameplay
 * after a screen finished entering. Every disc op or frame load in that
 * phase queues one `ioplay` line; dink_io_flush prints the queue once per
 * tick so SCIF time stays outside the timed windows. dink_io_summary
 * flushes, prints one line per screen and resets. Music stream refills are
 * tallied, not printed (music_max = worst single poll). */
struct DinkIoStats {
    unsigned ops, bytes, us, max_us;       /* play-path disc ops */
    unsigned frames, frame_us, frame_max;  /* play-path frame loads */
    unsigned gpu_waits, gpu_us;            /* play-path pvr_wait_ready */
    unsigned music_ops, music_bytes, music_us, music_max;
    /* Per-tick VRAM samples on the play path (dink_io_note_vram). */
    unsigned vram_samples, vram_enter, vram_min;
    unsigned min_screen_tex, min_dink_tex; /* held at the low point */
    unsigned peak_screen_tex, peak_dink_tex;
    /* Free VRAM right after each play-path upload: catches a dip that is
     * undone before the next tick sample. */
    unsigned upload_samples, min_upload;
    /* Play-path tick pacing (dink_io_tick): 60 Hz is 16.7 ms. ioplay's own
     * ring flush (SCIF, ~87 us/byte) is subtracted and reported apart. */
    unsigned ticks, ticks_over20, ticks_over34, tick_max_us;
    unsigned log_us, logged;
};
unsigned dink_io_now_us(void);
void dink_io_set_play(int on);
int dink_io_play(void);
/* Label for the next disc ops (copied). NULL or "" clears. Save the
 * outer label first and restore it, so nested loads keep attribution. */
void dink_io_ctx(const char *ctx);
void dink_io_ctx_save(char *dst, size_t n);
/* op "music" counts the stream refill only. what overrides ctx if set. */
void dink_io_note(const char *op, const char *what, size_t bytes, unsigned us);
/* seq < 0 marks a failed load of -seq (always printed). */
void dink_io_note_frame(const char *who, int seq, int frame, unsigned us,
                        unsigned disc_ops);
void dink_io_note_gpu_wait(unsigned us);
void dink_io_stats(struct DinkIoStats *out);
int dink_io_pending(void);
void dink_io_flush(void);
void dink_io_summary(int map);
/* Once per play tick: free VRAM and what screen sprites / Dink hold. The
 * summary reports the screen's low-water mark and the split at it. */
void dink_io_note_vram(unsigned free_b, unsigned screen_tex, unsigned dink_tex);
/* Once per play tick at the same point: tick-to-tick time. The first tick
 * after a summary (screen change) only sets the baseline. */
void dink_io_tick(unsigned now_us);
/* After a successful play-path texture upload (DC sprite_upload_pvr). */
void dink_io_note_vram_upload(unsigned free_b);

/* Host-testable pieces (also used on DC). */
int dink_fs_join(char *dst, size_t dstsz, const char *root, const char *rel);
void dink_fs_set_probe_roots(const char *pc, const char *cd, const char *fallback);
int dink_fs_exists_dir(const char *path);

#endif
