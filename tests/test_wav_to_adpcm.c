/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int wav_convert_path(const char *in_path, const char *out_path);

static int wav_read_info(const char *path, uint16_t *fmt, uint16_t *ch, uint32_t *rate,
                         uint16_t *bits, uint32_t *dsz)
{
    FILE *fp = fopen(path, "rb");
    uint8_t hdr[44];

    if (fp == NULL || fread(hdr, 1, 44, fp) != 44) {
        if (fp != NULL) {
            fclose(fp);
        }
        return -1;
    }
    fclose(fp);
    *fmt = (uint16_t)(hdr[20] | (hdr[21] << 8));
    *ch = (uint16_t)(hdr[22] | (hdr[23] << 8));
    *rate = (uint32_t)hdr[24] | ((uint32_t)hdr[25] << 8) | ((uint32_t)hdr[26] << 16) |
            ((uint32_t)hdr[27] << 24);
    *bits = (uint16_t)(hdr[34] | (hdr[35] << 8));
    *dsz = (uint32_t)hdr[40] | ((uint32_t)hdr[41] << 8) | ((uint32_t)hdr[42] << 16) |
           ((uint32_t)hdr[43] << 24);
    return 0;
}

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

static void wr_u16(FILE *fp, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    fwrite(b, 1, 2, fp);
}

static void wr_u32(FILE *fp, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    fwrite(b, 1, 4, fp);
}

static void write_pcm16(const char *path, const int16_t *pcm, size_t ns, uint32_t rate)
{
    FILE *fp = fopen(path, "wb");
    uint32_t dsz = (uint32_t)(ns * 2);
    expect(fp != NULL, "open pcm");
    fwrite("RIFF", 1, 4, fp);
    wr_u32(fp, 36 + dsz);
    fwrite("WAVEfmt ", 1, 8, fp);
    wr_u32(fp, 16);
    wr_u16(fp, 1);
    wr_u16(fp, 1);
    wr_u32(fp, rate);
    wr_u32(fp, rate * 2);
    wr_u16(fp, 2);
    wr_u16(fp, 16);
    fwrite("data", 1, 4, fp);
    wr_u32(fp, dsz);
    fwrite(pcm, 2, ns, fp);
    fclose(fp);
}

int main(void)
{
    char in[] = "build/dinkcast-sfx-in.wav";
    char out[] = "build/dinkcast-sfx-out.wav";
    int16_t small[100];
    int16_t big[5000];
    uint16_t fmt, ch, bits;
    uint32_t rate, dsz;
    size_t i;

    mkdir("build", 0755);
    for (i = 0; i < 100; i++) {
        small[i] = (int16_t)(i * 200);
    }
    for (i = 0; i < 5000; i++) {
        big[i] = (int16_t)((i % 64) * 400 - 12800);
    }
    write_pcm16(in, small, 100, 8000);
    expect(wav_convert_path(in, out) == 0, "convert small");
    expect(wav_read_info(out, &fmt, &ch, &rate, &bits, &dsz) == 0, "info small");
    expect(fmt == 1 && bits == 16, "small stays PCM");
    expect(dsz == 200, "small data bytes");

    write_pcm16(in, big, 5000, 22050);
    expect(wav_convert_path(in, out) == 0, "convert big");
    expect(wav_read_info(out, &fmt, &ch, &rate, &bits, &dsz) == 0, "info big");
    expect(fmt == 0x14, "big is Yamaha ADPCM 0x14");
    expect(bits == 4, "4-bit");
    expect(ch == 1 && rate == 22050, "rate/ch");
    expect(dsz == 2500, "adpcm bytes = nsamp/2");

    unlink(in);
    unlink(out);
    printf("OK test_wav_to_adpcm\n");
    return 0;
}
