/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Play-path I/O log: enter-path ops stay silent, play-path ops count,
 * music refills count separately, summary resets. */
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;

#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);           \
            fails++;                                                     \
        }                                                                \
    } while (0)

int main(void)
{
    struct DinkIoStats st;
    uint8_t buf[64];
    FILE *fp;

    dink_io_summary(0); /* start clean */

    /* Enter path: not counted. */
    dink_io_set_play(0);
    dink_io_note("pread", NULL, 100, 50);
    dink_io_note_frame("edraw", 20, 2, 5000, 1);
    dink_io_stats(&st);
    CHECK(st.ops == 0 && st.frames == 0);

    /* Play path: counted. */
    dink_io_set_play(1);
    CHECK(dink_io_play() == 1);
    dink_io_ctx("edraw seq=20 fr=2");
    dink_io_note("pread", NULL, 100, 50);
    dink_io_note("pread", NULL, 28, 700);
    dink_io_note_frame("edraw", 20, 2, 900, 2);
    dink_io_note_frame("dink", 72, 3, 10, 0); /* quiet, still counted */
    dink_io_note_gpu_wait(4000);
    dink_io_ctx(NULL);
    dink_io_stats(&st);
    CHECK(st.ops == 2);
    CHECK(st.bytes == 128);
    CHECK(st.us == 750);
    CHECK(st.max_us == 700);
    CHECK(st.frames == 2);
    CHECK(st.frame_max == 900);
    CHECK(st.gpu_waits == 1 && st.gpu_us == 4000);

    /* Music refill: own counters in either phase, never a disc op. */
    dink_io_note("music", NULL, 8192, 300);
    dink_io_set_play(0);
    dink_io_note("music", NULL, 8192, 100);
    dink_io_stats(&st);
    CHECK(st.ops == 2);
    CHECK(st.music_ops == 2 && st.music_bytes == 16384);
    CHECK(st.music_us == 400);

    /* Real primitive: dink_pread counts in play phase only. */
    fp = tmpfile();
    CHECK(fp != NULL);
    if (fp != NULL) {
        memset(buf, 7, sizeof(buf));
        CHECK(fwrite(buf, 1, sizeof(buf), fp) == sizeof(buf));
        fflush(fp);
        CHECK(dink_pread(fp, 0, buf, 16) == 0);
        dink_io_stats(&st);
        CHECK(st.ops == 2);
        dink_io_set_play(1);
        CHECK(dink_pread(fp, 8, buf, 16) == 0);
        dink_io_stats(&st);
        CHECK(st.ops == 3 && st.bytes == 144);
        fclose(fp);
    }

    /* fread_n / fread_all count too; a failed read is still a disc op. */
    fp = tmpfile();
    CHECK(fp != NULL);
    if (fp != NULL) {
        uint8_t *all = NULL;
        size_t an = 0;

        memset(buf, 3, sizeof(buf));
        CHECK(fwrite(buf, 1, sizeof(buf), fp) == sizeof(buf));
        fflush(fp);
        rewind(fp);
        dink_io_stats(&st);
        {
            unsigned ops0 = st.ops;

            CHECK(dink_fread_n(fp, buf, 8) == 0);
            rewind(fp);
            CHECK(dink_fread_all(fp, &all, &an) == 0 && an == sizeof(buf));
            free(all);
            CHECK(dink_fread_n(fp, buf, 8) != 0); /* at EOF: read-fail */
            dink_io_stats(&st);
            CHECK(st.ops == ops0 + 3);
        }
        fclose(fp);
    }

    /* Queue: lines wait for dink_io_flush (SCIF outside timed windows). */
    dink_io_flush();
    CHECK(dink_io_pending() == 0);
    dink_io_note_frame("dink", 72, 4, 10, 0);      /* quiet: no line */
    CHECK(dink_io_pending() == 0);
    dink_io_note_frame("edraw", 20, 5, 2500, 0);   /* >= 2 ms: line */
    CHECK(dink_io_pending() == 1);
    dink_io_note_frame("edraw", -20, 6, 10, 0);    /* failed: line */
    CHECK(dink_io_pending() == 2);
    dink_io_flush();
    CHECK(dink_io_pending() == 0);

    /* Context save/restore keeps the outer label. */
    {
        char outer[64], got[64];

        dink_io_ctx("preload_seq 20");
        dink_io_ctx_save(outer, sizeof(outer));
        dink_io_ctx("edraw seq=20 fr=1");
        dink_io_ctx(outer);
        dink_io_ctx_save(got, sizeof(got));
        CHECK(strcmp(got, "preload_seq 20") == 0);
        dink_io_ctx(NULL);
    }

    /* music-poll keeps the worst poll, not a chunk sum. */
    dink_io_note("music-poll", NULL, 0, 9000);
    dink_io_note("music-poll", NULL, 0, 1000);
    dink_io_stats(&st);
    CHECK(st.music_max == 9000);

    /* VRAM low-water mark: enter = first sample, min keeps the split
     * held at the low point, peaks are independent. Enter path ignored. */
    dink_io_note_vram(4000000u, 100000u, 200000u);
    dink_io_note_vram(3000000u, 900000u, 400000u);
    dink_io_note_vram(3500000u, 950000u, 300000u);
    dink_io_set_play(0);
    dink_io_note_vram(1000u, 0u, 0u);
    dink_io_set_play(1);
    dink_io_stats(&st);
    CHECK(st.vram_samples == 3 && st.vram_enter == 4000000u &&
          st.vram_min == 3000000u);
    CHECK(st.min_screen_tex == 900000u && st.min_dink_tex == 400000u);
    CHECK(st.peak_screen_tex == 950000u && st.peak_dink_tex == 400000u);

    /* Tick pacing: first tick is the baseline; >20 ms and >34 ms count.
     * In main.c the baseline reset on a swap comes from dink_io_summary
     * (play_load_screen); the enter-path branch below is a guard. */
    dink_io_tick(1000000u);
    dink_io_tick(1016667u);           /* 16.7 ms */
    dink_io_tick(1041667u);           /* 25 ms: over20 */
    dink_io_tick(1081667u);           /* 40 ms: over20 + over34 */
    dink_io_set_play(0);
    dink_io_tick(1500000u);
    dink_io_set_play(1);
    dink_io_tick(1900000u);           /* new baseline, not a 400 ms tick */
    dink_io_tick(1916667u);
    dink_io_stats(&st);
    CHECK(st.ticks == 4 && st.ticks_over20 == 2 && st.ticks_over34 == 1);
    CHECK(st.tick_max_us == 40000u);
    {
        unsigned wrap = 0xFFFFF000u;   /* dink_io_now_us wraps */

        dink_io_summary(34);
        dink_io_tick(wrap);
        dink_io_tick(wrap + 16000u);
        dink_io_stats(&st);
        CHECK(st.ticks == 1 && st.tick_max_us == 16000u);
    }

    /* Upload-time low-water: a dip below every tick sample wins min. */
    dink_io_summary(34);
    dink_io_note_vram(4000000u, 0u, 0u);
    dink_io_note_vram_upload(2500000u);
    dink_io_note_vram_upload(2600000u);
    dink_io_note_vram(3900000u, 0u, 0u);
    dink_io_stats(&st);
    CHECK(st.vram_min == 3900000u && st.upload_samples == 2 &&
          st.min_upload == 2500000u);
    dink_io_set_play(0);
    dink_io_note_vram_upload(1u); /* enter path: ignored */
    dink_io_set_play(1);
    dink_io_stats(&st);
    CHECK(st.min_upload == 2500000u && st.upload_samples == 2);

    /* Our own log flush is counted apart and taken out of the tick.
     * Leaving the play phase drains it (timed) like the top of a tick. */
    dink_io_summary(34);
    dink_io_tick(3000000u);
    dink_io_note("pread", NULL, 10, 5);
    dink_io_note("pread", NULL, 10, 5);
    dink_io_flush();
    dink_io_stats(&st);
    CHECK(st.logged == 2);
    dink_io_tick(3016000u + st.log_us);
    dink_io_stats(&st);
    CHECK(st.ticks == 1 && st.tick_max_us <= 16000u + 1u);
    {
        unsigned lus = st.log_us;

        /* Empty ring: no ioplay lines; the timed stdout drain (batched
         * play printfs on DC) still runs and is counted, ~0 on host. */
        dink_io_flush();
        dink_io_stats(&st);
        CHECK(st.logged == 2 && st.log_us >= lus && st.log_us - lus < 5000u);
    }

    /* Summary resets every counter. */
    dink_io_summary(408);
    dink_io_stats(&st);
    CHECK(st.ops == 0 && st.frames == 0 && st.music_ops == 0 &&
          st.gpu_waits == 0 && st.vram_samples == 0 && st.ticks == 0);
    dink_io_tick(2000000u);
    dink_io_tick(2016000u);
    dink_io_stats(&st);
    CHECK(st.ticks == 1); /* baseline restarts after a summary */

    if (fails != 0) {
        return 1;
    }
    printf("OK test_iolog\n");
    return 0;
}
