/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Overlay VRAM (choice box, inventory menu) only while shown: open loads and
 * uploads making room (Dink shortfall, then screen one at a time), refusal
 * frees everything and latches until close, close frees, art another
 * screen loaded is freed on close. Fake VRAM. */
#include "overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c)                                                               \
    do {                                                                       \
        if (!(c)) {                                                            \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);       \
            g_fail = 1;                                                        \
        }                                                                      \
    } while (0)

#define NF 3
static struct SpriteFrame g_fr[NF];
static int g_loads, g_load_fail;

/* VRAM: free bytes, largest block (fragmentation), screen and Dink pools. */
static size_t g_free, g_block, g_screen, g_screen_unit, g_dink;
static int g_uploads, g_screen_frees, g_dink_calls;

static int fake_load(void)
{
    int i;

    g_loads++;
    for (i = 0; i < NF; i++) {
        if (g_fr[i].tex == NULL && g_fr[i].argb1555 == NULL) {
            g_fr[i].tw = 256;
            g_fr[i].th = 256; /* 128 KB each */
            g_fr[i].argb1555 = (uint16_t *)malloc(4);
        }
    }
    return g_load_fail ? -1 : 0;
}

static void fake_release(void)
{
    int i;

    for (i = 0; i < NF; i++) {
        if (g_fr[i].tex != NULL) {
            g_free += 131072u;
            g_block += 131072u;
        }
        free(g_fr[i].argb1555);
        memset(&g_fr[i], 0, sizeof(g_fr[i]));
    }
}

static int fake_n(void)
{
    return NF;
}

static struct SpriteFrame *fake_frame(int i)
{
    return i >= 0 && i < NF ? &g_fr[i] : NULL;
}

static int fake_upload(struct SpriteFrame *f)
{
    size_t need = (size_t)f->tw * (size_t)f->th * 2u;

    g_uploads++;
    if (g_block < need) {
        return -1;
    }
    g_free -= need;
    g_block -= need;
    f->tex = (void *)0x1;
    return 0;
}

static size_t fake_avail(void)
{
    return g_free;
}

static size_t fake_screen_ev(void)
{
    return g_screen;
}

static size_t fake_screen_one(void)
{
    if (g_screen == 0) {
        return 0;
    }
    g_screen -= g_screen_unit;
    g_free += g_screen_unit;
    g_block += g_screen_unit;
    g_screen_frees++;
    return g_screen_unit;
}

static size_t fake_dink(size_t extra)
{
    g_dink_calls++;
    if (g_dink < extra) {
        return 0;
    }
    g_free += g_dink;
    g_block += g_dink;
    extra = g_dink;
    g_dink = 0;
    return extra;
}

static const struct OverlayOps ops = {fake_load, fake_release, fake_n,
                                      fake_frame};
static const struct OverlayVram vram = {fake_upload, fake_avail,
                                        fake_screen_ev, fake_screen_one,
                                        fake_dink};

static void setv(size_t free_b, size_t block, size_t screen, size_t unit,
                 size_t dink)
{
    g_free = free_b;
    g_block = block;
    g_screen = screen;
    g_screen_unit = unit;
    g_dink = dink;
    g_uploads = g_screen_frees = g_dink_calls = 0;
}

int main(void)
{
    struct Overlay o = {"t", &ops, OVERLAY_OFF};
    int i;

    overlay_set_vram(&vram);

    /* Plenty of VRAM: open uploads all, drops CPU; close frees. */
    setv(1u << 20, 1u << 20, 0, 0, 0);
    overlay_sync_one(&o, 1);
    CHECK(o.state == OVERLAY_ON);
    for (i = 0; i < NF; i++) {
        CHECK(g_fr[i].tex != NULL && g_fr[i].argb1555 == NULL);
    }
    overlay_sync_one(&o, 1); /* stays: no reload */
    CHECK(g_loads == 1);
    overlay_sync_one(&o, 0);
    CHECK(o.state == OVERLAY_OFF && g_fr[0].tex == NULL);

    /* Neither pool alone: Dink gives the shortfall, then the screen frees
     * one texture at a time until each frame fits. Need 384 KB; free 0;
     * screen 256 KB in 64 KB units; Dink 128 KB. */
    setv(0, 0, 262144u, 65536u, 131072u);
    overlay_sync_one(&o, 1);
    CHECK(o.state == OVERLAY_ON);
    CHECK(g_dink_calls >= 1);
    CHECK(g_screen_frees == 4);
    overlay_sync_one(&o, 0);

    /* Fragmented: total free covers it but no block does; the screen
     * frees one at a time (the fake makes blocks contiguous as it frees). */
    setv(393216u, 0, 393216u, 131072u, 0);
    overlay_sync_one(&o, 1);
    CHECK(o.state == OVERLAY_ON);
    CHECK(g_screen_frees == 3);
    overlay_sync_one(&o, 0);

    /* Not enough anywhere: nothing on screen is freed for nothing, every
     * frame (CPU and texture) is released, state latches FAILED: no reload
     * while still wanted, retry after close. */
    setv(131072u, 131072u, 65536u, 65536u, 0);
    overlay_sync_one(&o, 1);
    CHECK(o.state == OVERLAY_FAILED);
    CHECK(g_screen_frees == 0);
    for (i = 0; i < NF; i++) {
        CHECK(g_fr[i].tex == NULL && g_fr[i].argb1555 == NULL);
    }
    i = g_loads;
    overlay_sync_one(&o, 1);
    overlay_sync_one(&o, 1);
    CHECK(g_loads == i);
    overlay_sync_one(&o, 0);
    CHECK(o.state == OVERLAY_OFF);
    setv(1u << 20, 1u << 20, 0, 0, 0);
    overlay_sync_one(&o, 1);
    CHECK(o.state == OVERLAY_ON && g_loads == i + 1);
    overlay_sync_one(&o, 0);

    /* Partial load (a frame missing): release everything, FAILED. */
    g_load_fail = 1;
    overlay_sync_one(&o, 1);
    CHECK(o.state == OVERLAY_FAILED);
    for (i = 0; i < NF; i++) {
        CHECK(g_fr[i].argb1555 == NULL && g_fr[i].tex == NULL);
    }
    g_load_fail = 0;
    overlay_sync_one(&o, 0);

    /* Art loaded elsewhere (start menu Continue) while OFF: freed on the
     * first tick that does not want it. */
    (void)fake_load();
    overlay_sync_one(&o, 0);
    CHECK(g_fr[0].argb1555 == NULL);

    if (g_fail) {
        return 1;
    }
    printf("OK test_overlay\n");
    return 0;
}
