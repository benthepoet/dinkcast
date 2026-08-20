/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "saybox.h"

#include "dinkc_vm.h"
#include "font.h"

#include <stdio.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

static const struct MapScreen *g_scr;
static struct Player *g_pl;
static char g_text[200];
static int g_on, g_owner, g_x, g_y, g_color;

#ifdef _arch_dreamcast
static void *g_tex;
#endif

void saybox_bind(const struct MapScreen *scr, struct Player *pl)
{
    g_scr = scr;
    g_pl = pl;
}

static void owner_xy(int sprite, int *x, int *y)
{
    *x = 100;
    *y = 100;
    if (sprite == 0) {
        sprite = 1;
    }
    if (sprite == 1 && g_pl != NULL) {
        *x = g_pl->x;
        *y = g_pl->y;
        return;
    }
    if (g_scr != NULL && sprite >= 1 && sprite <= 99 &&
        g_scr->sprite[sprite].active) {
        *x = (int)g_scr->sprite[sprite].x;
        *y = (int)g_scr->sprite[sprite].y;
    }
}

/* text_brain: non-narrator text takes owner x/y every frame. */
static void saybox_place(void)
{
    int ox, oy, boxw;

    if (!g_on || g_owner == 1000) {
        return;
    }
    owner_xy(g_owner, &ox, &oy);
    g_x = ox - DINK_SAY_XOFF;
    g_y = oy - DINK_SAY_YOFF;
    boxw = DINK_SAY_BOX_W;
    if (g_x + boxw > DINK_SAY_PLAYX) {
        g_x -= (g_x + boxw - DINK_SAY_PLAYX);
    }
    if (g_x < 20) {
        g_x = 20;
    }
    if (g_y < 0) {
        g_y = 0;
    }
}

/* FreeDink text_draw: strip leading `X color pairs. */
static void strip_color(const char *in, char *out, size_t n, int *color)
{
    const char *p = in != NULL ? in : "";

    *color = 14;
    while (p[0] == '`' && p[1] != '\0') {
        char c = p[1];

        if (c == '#') {
            *color = 13;
        } else if (c == '4') {
            *color = 4;
        } else if (c == '$' || c == '%') {
            *color = (c == '$') ? 14 : 15;
        } else if (c >= '1' && c <= '9') {
            *color = c - '0';
        } else if (c == '0') {
            *color = 10;
        }
        p += 2;
    }
    strncpy(out, p, n - 1);
    out[n - 1] = '\0';
}

/* FreeDink process_text_for_wrapping at box width. */
static void wrap_line(char *s, int max_px)
{
    int i = 0, start = 0, last_sp = -1;
    int cell = font_advance('A');

    if (cell < 1) {
        cell = DINK_FONT_CELL;
    }
    while (s[i] != '\0') {
        if (s[i] == ' ') {
            last_sp = i;
        }
        if (s[i] == '\n') {
            start = i + 1;
            last_sp = -1;
            i++;
            continue;
        }
        if ((i - start + 1) * cell > max_px) {
            if (last_sp > start) {
                s[last_sp] = '\n';
                start = last_sp + 1;
                last_sp = -1;
            } else {
                start = i;
            }
        }
        i++;
    }
}

void saybox_set(const char *text, int sprite)
{
    int ox, oy, boxw;

    g_owner = sprite;
    if (g_owner == 0) {
        g_owner = 1;
    }
    strip_color(text, g_text, sizeof(g_text), &g_color);
    boxw = DINK_SAY_BOX_W;
    if (g_owner == 1000) {
        owner_xy(g_owner, &ox, &oy);
        boxw = DINK_SAY_PLAYX - 20;
        g_x = ox;
        g_y = oy;
    }
    wrap_line(g_text, boxw);
    g_on = 1;
    saybox_place();
}

void saybox_clear(void)
{
    g_on = 0;
    g_owner = 0;
    g_text[0] = '\0';
}

int saybox_active(void)
{
    return g_on;
}

const char *saybox_text(void)
{
    return g_text;
}

int saybox_x(void)
{
    saybox_place();
    return g_x;
}

int saybox_y(void)
{
    saybox_place();
    return g_y;
}

int saybox_color(void)
{
    return g_color;
}

#ifdef _arch_dreamcast
int saybox_upload(void)
{
    const uint16_t *px = font_atlas_argb1555();
    pvr_ptr_t tex;

    if (px == NULL) {
        return -1;
    }
    saybox_evict();
    tex = pvr_mem_malloc((size_t)DINK_FONT_ATLAS_W * DINK_FONT_ATLAS_H * 2u);
    if (tex == NULL) {
        return -1;
    }
    pvr_txr_load_ex((void *)px, tex, DINK_FONT_ATLAS_W, DINK_FONT_ATLAS_H,
                    PVR_TXRLOAD_16BPP);
    g_tex = tex;
    return 0;
}

void saybox_evict(void)
{
    if (g_tex != NULL) {
        pvr_mem_free((pvr_ptr_t)g_tex);
        g_tex = NULL;
    }
}

static uint32_t color_argb(int c)
{
    /* FreeDink font_colors (approx). */
    if (c == 13) {
        return 0xFFFF8484u;
    }
    if (c == 4) {
        return 0xFFFF9C4Au;
    }
    if (c == 15) {
        return 0xFFFFFFFFu;
    }
    return 0xFFFFFF02u; /* 14 yellow */
}

static void draw_ch(float x, float y, float z, int ch, uint32_t argb)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;
    int col, row;
    float u0, v0, u1, v1, cell;

    if (g_tex == NULL || font_glyph_cell(ch, &col, &row) == 0) {
        return;
    }
    cell = (float)DINK_FONT_CELL;
    u0 = (float)(col * DINK_FONT_CELL) / (float)DINK_FONT_ATLAS_W;
    v0 = (float)(row * DINK_FONT_CELL) / (float)DINK_FONT_ATLAS_H;
    u1 = u0 + cell / (float)DINK_FONT_ATLAS_W;
    v1 = v0 + cell / (float)DINK_FONT_ATLAS_H;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB1555,
                     DINK_FONT_ATLAS_W, DINK_FONT_ATLAS_H, (pvr_ptr_t)g_tex,
                     PVR_FILTER_NONE);
    cxt.gen.alpha = PVR_ALPHA_ENABLE;
    cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    vert.oargb = 0;
    vert.z = z;
    vert.argb = argb;
    vert.flags = PVR_CMD_VERTEX;
    vert.x = x;
    vert.y = y;
    vert.u = u0;
    vert.v = v0;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x + cell;
    vert.y = y;
    vert.u = u1;
    vert.v = v0;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x;
    vert.y = y + cell;
    vert.u = u0;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = x + cell;
    vert.y = y + cell;
    vert.u = u1;
    vert.v = v1;
    pvr_prim(&vert, sizeof(vert));
}

void saybox_draw_pvr(float z)
{
    const char *p;
    float x, y, line0;
    uint32_t fg, bg;

    if (!g_on || g_tex == NULL) {
        return;
    }
    saybox_place();
    p = g_text;
    x = (float)g_x;
    y = (float)g_y;
    line0 = x;
    fg = color_argb(g_color);
    bg = 0xFF080E15u;
    while (*p != '\0') {
        if (*p == '\n') {
            x = line0;
            y += (float)DINK_FONT_CELL;
            p++;
            continue;
        }
        draw_ch(x - 1.0f, y, z, (unsigned char)*p, bg);
        draw_ch(x + 1.0f, y, z, (unsigned char)*p, bg);
        draw_ch(x, y - 1.0f, z, (unsigned char)*p, bg);
        draw_ch(x, y + 1.0f, z, (unsigned char)*p, bg);
        draw_ch(x, y, z + 0.01f, (unsigned char)*p, fg);
        x += (float)font_advance((unsigned char)*p);
        p++;
    }
}

void saybox_draw_choices_pvr(float z)
{
    int n, i, cur;
    float y;

    n = dinkc_vm_choice_n();
    cur = dinkc_vm_choice_cur();
    if (n < 1) {
        return;
    }
    y = 140.0f;
    for (i = 1; i <= n; i++) {
        const char *s = dinkc_vm_choice_line(i);
        uint32_t fg = (i == cur) ? 0xFFFFFF02u : 0xFFFFFFFFu;
        float x = 80.0f;

        if (i == cur) {
            draw_ch(x - 12.0f, y, z, '>', fg);
        }
        {
            const char *p = s;

            while (*p != '\0') {
                draw_ch(x - 1.0f, y, z, (unsigned char)*p, 0xFF080E15u);
                draw_ch(x + 1.0f, y, z, (unsigned char)*p, 0xFF080E15u);
                draw_ch(x, y, z + 0.01f, (unsigned char)*p, fg);
                x += (float)font_advance((unsigned char)*p);
                p++;
            }
        }
        y += 12.0f;
    }
}
#endif
