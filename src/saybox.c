/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "saybox.h"

#include "choice.h"
#include "dinkc_vm.h"
#include "font.h"
#include "sprite.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

static const struct MapScreen *g_scr;
static struct Player *g_pl;
static int (*g_live_xy)(int slot, int *x, int *y);
static char g_text[200];
static int g_on, g_owner, g_x, g_y, g_color;
static int g_kill_ttl, g_kill_start, g_kill_armed;
static int g_xy;

/* FreeDink gfx_fonts.cpp gfx_fonts_init_colors. Index 0 unused. */
static const uint32_t g_font_argb[16] = {
    0xFFFFFF02u, /* 0 unused, same yellow as 14 */
    0xFFFFC6FFu, /* 1 Light Magenta 255,198,255 */
    0xFF83B54Au, /* 2 Dark Green 131,181,74 */
    0xFF63F2F7u, /* 3 Bold Cyan 99,242,247 */
    0xFFFF9C4Au, /* 4 Orange 255,156,74 */
    0xFFDEADFFu, /* 5 Magenta 222,173,255 */
    0xFFF4BC49u, /* 6 Brown Orange 244,188,73 */
    0xFFADADADu, /* 7 Light Gray 173,173,173 */
    0xFF555555u, /* 8 Dark Gray 85,85,85 */
    0xFF94C6FFu, /* 9 Sky Blue 148,198,255 */
    0xFF00FF00u, /* 10 Bright Green 0,255,0 */
    0xFFFFFF02u, /* 11 Yellow 255,255,2 */
    0xFFFFFF02u, /* 12 Yellow 255,255,2 */
    0xFFFF8484u, /* 13 Hot Pink 255,132,132 */
    0xFFFFFF02u, /* 14 Yellow 255,255,2 */
    0xFFFFFFFFu, /* 15 White 255,255,255 */
};

uint32_t saybox_argb(int color)
{
    if (color < 0 || color > 15) {
        color = 14;
    }
    return g_font_argb[color];
}

#ifdef _arch_dreamcast
static void *g_tex;
#endif

void saybox_bind(const struct MapScreen *scr, struct Player *pl)
{
    g_scr = scr;
    g_pl = pl;
}

void saybox_bind_live_xy(int (*fn)(int slot, int *x, int *y))
{
    g_live_xy = fn;
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
    if (g_live_xy != NULL && g_live_xy(sprite, x, y)) {
        return;
    }
    if (g_scr != NULL && sprite >= 1 && sprite <= 99 &&
        g_scr->sprite[sprite].active) {
        *x = (int)g_scr->sprite[sprite].x;
        *y = (int)g_scr->sprite[sprite].y;
    }
}

static int say_box_w(void)
{
    if (g_xy) {
        return DINK_SAY_PLAYX;
    }
    return g_owner == 1000 ? DINK_SAY_PLAYX - 20 : DINK_SAY_BOX_W;
}

/* text_brain then text_draw rect: owner.x-75, clamp ≥1, then if right
 * edge > 620 shift left. */
static void saybox_place(void)
{
    int ox, oy, boxw;

    if (!g_on || g_owner == 1000) {
        return;
    }
    owner_xy(g_owner, &ox, &oy);
    g_x = ox - DINK_SAY_XOFF;
    g_y = oy - DINK_SAY_YOFF;
    if (g_x < 1) {
        g_x = 1;
    }
    if (g_y < 1) {
        g_y = 1;
    }
    boxw = DINK_SAY_BOX_W;
    if (g_x + boxw > DINK_SAY_PLAYX) {
        g_x -= (g_x + boxw - DINK_SAY_PLAYX);
    }
}

/* FreeDink text_draw: strip leading `X color pairs. */
static void strip_color(const char *in, char *out, size_t n, int *color)
{
    const char *p = in != NULL ? in : "";

    *color = 14;
    while (p[0] == '`' && p[1] != '\0') {
        char c = p[1];

        /* FreeDink text_draw (dversion >= 108). */
        if (c == '#') {
            *color = 13;
        } else if (c == '4') {
            *color = 4;
        } else if (c == '$') {
            *color = 14;
        } else if (c == '%') {
            *color = 15;
        } else if (c == '@') {
            *color = 12;
        } else if (c == '!') {
            *color = 11;
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

static void saybox_arm(const char *text, int boxw)
{
    wrap_line(g_text, boxw);
    g_on = 1;
    /* add_text_sprite: strlen of the DinkC string (color prefix counts). */
    g_kill_ttl = (int)strlen(text != NULL ? text : "") * DINK_SAY_TEXT_TIMER;
    if (g_kill_ttl < DINK_SAY_TEXT_MIN) {
        g_kill_ttl = DINK_SAY_TEXT_MIN;
    }
    g_kill_start = 0;
    g_kill_armed = 0;
}

void saybox_set(const char *text, int sprite)
{
    int ox, oy, boxw;

    g_xy = 0;
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
    saybox_arm(text, boxw);
    saybox_place();
}

void saybox_set_xy(const char *text, int x, int y)
{
    /* say_text_xy: add_text_sprite at (mx,my), text_owner 1000. */
    g_xy = 1;
    g_owner = 1000;
    strip_color(text, g_text, sizeof(g_text), &g_color);
    g_x = x;
    g_y = y;
    saybox_arm(text, DINK_SAY_PLAYX);
}

int saybox_tick(int now_ms)
{
    if (!g_on || g_kill_ttl < 1) {
        return 0;
    }
    /* FreeDink kill_start==0; our now_ms can be 0 so keep a flag. */
    if (!g_kill_armed) {
        g_kill_start = now_ms;
        g_kill_armed = 1;
        return 0;
    }
    if (g_kill_start + g_kill_ttl < now_ms) {
        saybox_clear();
        return 1;
    }
    return 0;
}

void saybox_clear(void)
{
    g_on = 0;
    g_owner = 0;
    g_xy = 0;
    g_text[0] = '\0';
    g_kill_ttl = 0;
    g_kill_start = 0;
    g_kill_armed = 0;
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

int saybox_line_x(const char *line)
{
    int cell, w, n = 0, boxw;
    const char *p = line != NULL ? line : "";

    saybox_place();
    cell = font_advance('A');
    if (cell < 1) {
        cell = DINK_FONT_CELL;
    }
    while (p[n] != '\0' && p[n] != '\n') {
        n++;
    }
    w = n * cell;
    boxw = say_box_w();
    return g_x + boxw / 2 - w / 2;
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
    float x, y;
    uint32_t fg, bg;

    if (!g_on || g_tex == NULL) {
        return;
    }
    saybox_place();
    p = g_text;
    y = (float)g_y;
    fg = saybox_argb(g_color);
    bg = 0xFF080E15u;
    while (*p != '\0') {
        x = (float)saybox_line_x(p);
        while (*p != '\0' && *p != '\n') {
            draw_ch(x - 1.0f, y, z, (unsigned char)*p, bg);
            draw_ch(x + 1.0f, y, z, (unsigned char)*p, bg);
            draw_ch(x, y - 1.0f, z, (unsigned char)*p, bg);
            draw_ch(x, y + 1.0f, z, (unsigned char)*p, bg);
            draw_ch(x, y, z + 0.01f, (unsigned char)*p, fg);
            x += (float)font_advance((unsigned char)*p);
            p++;
        }
        if (*p == '\n') {
            p++;
        }
        y += (float)DINK_FONT_CELL;
    }
}

void saybox_draw_num_pvr(int x, int y, int num, float z)
{
    char buf[16];
    const char *p;
    float cx;
    uint32_t fg, bg;

    if (g_tex == NULL) {
        return;
    }
    snprintf(buf, sizeof(buf), "%d", num);
    cx = (float)x;
    fg = saybox_argb(14);
    bg = 0xFF080E15u;
    for (p = buf; *p != '\0'; p++) {
        draw_ch(cx - 1.0f, (float)y, z, (unsigned char)*p, bg);
        draw_ch(cx + 1.0f, (float)y, z, (unsigned char)*p, bg);
        draw_ch(cx, (float)y - 1.0f, z, (unsigned char)*p, bg);
        draw_ch(cx, (float)y + 1.0f, z, (unsigned char)*p, bg);
        draw_ch(cx, (float)y, z + 0.01f, (unsigned char)*p, fg);
        cx += (float)font_advance((unsigned char)*p);
    }
}

static void draw_str_shadow(float x, float y, float z, const char *s,
                            uint32_t fg, int title)
{
    const char *p = s;
    uint32_t bg = 0xFF080E15u;

    while (p != NULL && *p != '\0' && *p != '\n') {
        unsigned char ch = (unsigned char)*p;

        if (title) {
            draw_ch(x, y, z, ch, bg);
            draw_ch(x + 1.0f, y + 1.0f, z + 0.01f, ch, fg);
        } else {
            draw_ch(x, y, z, ch, bg);
            draw_ch(x - 2.0f, y - 2.0f, z, ch, bg);
            draw_ch(x - 1.0f, y - 1.0f, z + 0.01f, ch, fg);
        }
        x += (float)font_advance(ch);
        p++;
    }
}

static void draw_wrapped_center(float y, float z, const char *s, uint32_t fg,
                                int title)
{
    char buf[256];
    const char *p;

    if (s == NULL) {
        s = "";
    }
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    choice_wrap(buf, DINK_CHOICE_BOX_R - DINK_CHOICE_BOX_L);
    p = buf;
    while (*p != '\0') {
        float x = (float)choice_center_x(p);

        draw_str_shadow(x, y, z, p, fg, title);
        while (*p != '\0' && *p != '\n') {
            p++;
        }
        if (*p == '\n') {
            p++;
        }
        y += (float)DINK_FONT_CELL;
    }
}

void saybox_draw_choices_pvr(float z)
{
    int n, i, cur, h[21];
    struct ChoiceLayout lay;
    struct SpriteFrame *fr;
    int cf, curyl, curyr, y;

    n = dinkc_vm_choice_n();
    cur = dinkc_vm_choice_cur();
    if (n < 1) {
        return;
    }
    fr = choice_frame(DINK_CHOICE_SEQ, 2);
    if (fr != NULL) {
        sprite_blit_pvr(fr, (float)DINK_CHOICE_PX, (float)DINK_CHOICE_PY, z);
    }
    fr = choice_frame(DINK_CHOICE_SEQ, 3);
    if (fr != NULL) {
        sprite_blit_pvr(fr, (float)(DINK_CHOICE_PX + 169),
                        (float)(DINK_CHOICE_PY + 42), z);
    }
    fr = choice_frame(DINK_CHOICE_SEQ, 4);
    if (fr != NULL) {
        sprite_blit_pvr(fr, (float)(DINK_CHOICE_PX + 169 + 180),
                        (float)(DINK_CHOICE_PY + 1), z);
    }
    if (dinkc_vm_choice_title()[0] != '\0') {
        uint32_t tfg;
        int col = dinkc_vm_choice_color();

        if (col >= 1 && col <= 15) {
            tfg = saybox_argb(col);
        } else {
            tfg = 0xFFFFFFFFu;
        }
        draw_wrapped_center(94.0f, z + 0.05f, dinkc_vm_choice_title(), tfg, 1);
    }
    memset(h, 0, sizeof(h));
    for (i = 1; i <= n; i++) {
        h[i] = choice_wrap_height(dinkc_vm_choice_line(i));
    }
    choice_layout(n, cur, h, dinkc_vm_choice_newy(), &lay);
    y = lay.choices_y;
    curyl = 200;
    curyr = 200;
    for (i = lay.view_lo; i <= lay.view_hi && i <= n; i++) {
        uint32_t fg = (i == cur) ? 0xFFFFFFFFu : 0xFFFFFF02u;

        if (i == cur) {
            curyl = y - 4;
            curyr = y - 4;
        }
        draw_wrapped_center((float)y, z + 0.05f, dinkc_vm_choice_line(i), fg,
                            0);
        y += h[i];
    }
    cf = choice_curf();
    fr = choice_frame(DINK_CHOICE_AROWL, cf);
    if (fr != NULL) {
        sprite_blit_pvr(fr, (float)DINK_CHOICE_CURXL, (float)curyl, z + 0.08f);
    }
    fr = choice_frame(DINK_CHOICE_AROWR, cf);
    if (fr != NULL) {
        sprite_blit_pvr(fr, (float)DINK_CHOICE_CURXR, (float)curyr, z + 0.08f);
    }
}
#endif
