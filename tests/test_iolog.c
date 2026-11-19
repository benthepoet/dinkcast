/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Play-path I/O log: enter-path ops stay silent, play-path ops count,
 * music refills count separately, summary resets. */
#include "fs.h"

#include <stdio.h>
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
    CHECK(st.music_max == 300);

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

    /* Summary resets every counter. */
    dink_io_summary(408);
    dink_io_stats(&st);
    CHECK(st.ops == 0 && st.frames == 0 && st.music_ops == 0 &&
          st.gpu_waits == 0);

    if (fails != 0) {
        return 1;
    }
    printf("OK test_iolog\n");
    return 0;
}
