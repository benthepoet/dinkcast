/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fade.h"

#ifdef _arch_dreamcast
#include <dc/pvr.h>
#endif

static int g_bright = FADE_FULL;
static int g_down;
static int g_up;
static int g_last;
static int g_have_last;
static int g_cycle_clock;

void fade_reset(void)
{
    g_bright = FADE_FULL;
    g_down = 0;
    g_up = 0;
    g_last = 0;
    g_have_last = 0;
    g_cycle_clock = 0;
}

static int fade_step(int delta)
{
    if (delta < 1) {
        return 0;
    }
    if (delta > FADE_MS) {
        delta = FADE_MS;
    }
    return delta * FADE_FULL / FADE_MS;
}

static void fade_apply_down(int now_ms)
{
    int step;

    if (!g_have_last) {
        g_last = now_ms;
        g_have_last = 1;
        return;
    }
    step = fade_step(now_ms - g_last);
    g_last = now_ms;
    if (g_bright > step) {
        g_bright -= step;
    } else {
        g_bright = 0;
    }
}

static int fade_apply_up(int now_ms)
{
    int step;

    if (!g_have_last) {
        g_last = now_ms;
        g_have_last = 1;
        return 0;
    }
    step = fade_step(now_ms - g_last);
    g_last = now_ms;
    g_bright += step;
    if (g_bright >= FADE_FULL) {
        g_bright = FADE_FULL;
        return 1;
    }
    return 0;
}

void fade_tick(int now_ms)
{
    /* update_frame: up_cycle then CyclePalette. */
    if (g_up) {
        if (fade_apply_up(now_ms)) {
            g_up = 0;
            g_have_last = 0;
        }
    }
    if (g_down) {
        fade_apply_down(now_ms);
        if (now_ms > g_cycle_clock) {
            g_down = 0;
            g_have_last = 0;
        }
    }
}

void fade_down_start(int now_ms)
{
    if (!g_up) {
        g_down = 1;
    }
    g_cycle_clock = now_ms + FADE_DOWN_YIELD_MS;
    g_have_last = 0;
}

void fade_up_start(int now_ms)
{
    (void)now_ms;
    if (g_down) {
        g_down = 0;
    }
    g_up = 1;
    g_have_last = 0;
}

int fade_busy(void)
{
    return g_up || g_down;
}

int fade_brightness(void)
{
    return g_bright;
}

void fade_draw_pvr(void)
{
#ifdef _arch_dreamcast
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    int a;

    if (g_bright >= FADE_FULL) {
        return;
    }
    a = (FADE_FULL - g_bright) * 255 / FADE_FULL;
    if (a < 1) {
        return;
    }
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    vert.flags = PVR_CMD_VERTEX;
    vert.argb = ((unsigned)a << 24);
    vert.oargb = 0;
    vert.z = 10.0f;
    vert.u = vert.v = 0.0f;
    vert.x = 0.0f;
    vert.y = 0.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = 640.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = 0.0f;
    vert.y = 480.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = 640.0f;
    pvr_prim(&vert, sizeof(vert));
#else
    (void)g_bright;
#endif
}
