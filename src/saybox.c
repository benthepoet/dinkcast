/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "saybox.h"

#include "choice.h"
#include "dinkc_vm.h"
#include "fade.h"
#include "font.h"
#include "sprite.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

#define SAY_LINES 32

struct SayLine {
    int on;
    int owner;
    int xy;
    int x, y;
    int color;
    int fiber;  /* say_stop callback; 0 for plain say */
    int script; /* creating fiber, kept after the callback is cleared */
    int born;
    int kill_ttl, kill_start, kill_armed;
    char text[200];
};

static const struct MapScreen *g_scr;
static struct Player *g_pl;
static int (*g_live_xy)(int slot, int *x, int *y);
static int (*g_alive)(int slot);
static int g_born;
static struct SayLine g_line[SAY_LINES];
static int g_cur = -1;
static int g_exp[SAY_LINES];
static int g_nexp;

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

uint32_t saybox_num_argb(int exp, int bright)
{
    /* text_draw: color 0 is not a palette index; it becomes white.
     * brain_parm 5000 sets color 14. brightness < 256 forces 15. */
    if (bright < 256 || !exp) {
        return 0xFFFFFFFFu;
    }
    return saybox_argb(14);
}

uint32_t saybox_text_argb(int color, int bright)
{
    if (bright < 256) {
        return saybox_argb(15);
    }
    return saybox_argb(color);
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

void saybox_bind_alive(int (*fn)(int slot))
{
    g_alive = fn;
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

static void retarget(void);

static struct SayLine *focus(void)
{
    if (g_cur < 0 || g_cur >= SAY_LINES || !g_line[g_cur].on) {
        return NULL;
    }
    return &g_line[g_cur];
}

static int say_box_w(const struct SayLine *ln)
{
    if (ln->xy) {
        return DINK_SAY_PLAYX;
    }
    return ln->owner == 1000 ? DINK_SAY_PLAYX - 20 : DINK_SAY_BOX_W;
}

/* text_brain then text_draw rect: owner.x-75, clamp ≥1, then if right
 * edge > 620 shift left. Narrator and say_xy stay where they were born. */
/* FreeDink text_brain: a line whose owner sprite is no longer active is
 * removed before it is drawn (not moved to a default point). Narrator
 * (1000) and screen (0) lines have no owner sprite. */
static int line_owner_gone(const struct SayLine *ln)
{
    return ln->owner != 0 && ln->owner != 1000 && g_alive != NULL &&
           !g_alive(ln->owner);
}

static void place_line(struct SayLine *ln)
{
    int ox, oy, boxw;

    if (ln == NULL || !ln->on || ln->owner == 1000 || ln->xy) {
        return;
    }
    owner_xy(ln->owner, &ox, &oy);
    ln->x = ox - DINK_SAY_XOFF;
    ln->y = oy - DINK_SAY_YOFF;
    if (ln->x < 1) {
        ln->x = 1;
    }
    if (ln->y < 1) {
        ln->y = 1;
    }
    boxw = DINK_SAY_BOX_W;
    if (ln->x + boxw > DINK_SAY_PLAYX) {
        ln->x -= (ln->x + boxw - DINK_SAY_PLAYX);
    }
}

static void place_all(void)
{
    int i;

    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].on) {
            place_line(&g_line[i]);
        }
    }
}

static int line_owned(int sprite)
{
    int i;

    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].on && g_line[i].owner == sprite) {
            return 1;
        }
    }
    return 0;
}

/* kill_text_owned_by: remove the sprite, do not run its callback.
 * Owner 1000 is included (dc_say_stop on a narrator). */
void saybox_kill_owner(int sprite)
{
    int i;

    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].on && g_line[i].owner == sprite) {
            g_line[i].on = 0;
            if (g_cur == i) {
                g_cur = -1;
            }
        }
    }
    retarget();
}

static void retarget(void)
{
    int i;

    if (focus() != NULL) {
        return;
    }
    g_cur = -1;
    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].on) {
            g_cur = i;
        }
    }
}

static int alloc_line(void)
{
    int i, oldest = -1, age = 0;

    for (i = 0; i < SAY_LINES; i++) {
        if (!g_line[i].on) {
            return i;
        }
    }
    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].fiber != 0) {
            continue;
        }
        if (oldest < 0 || g_line[i].born < age) {
            oldest = i;
            age = g_line[i].born;
        }
    }
    if (oldest < 0) {
        printf("say full\n");
        return -1;
    }
    g_line[oldest].on = 0;
    return oldest;
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

/* Above FreeDink MAX_SPRITES_AT_ONCE (300) so sp_kill of a text id
 * does not land on a live sprite. */
#define SAY_ID_BASE 400

static int g_last_id;

static int id_of(int slot)
{
    return slot < 0 ? 0 : SAY_ID_BASE + slot;
}

static int add_line(const char *raw, int owner, int xy, int x, int y,
                    int fiber, int script)
{
    struct SayLine *ln;
    int slot, boxw, ox, oy;

    slot = alloc_line();
    if (slot < 0) {
        g_last_id = 0;
        return -1;
    }
    ln = &g_line[slot];
    memset(ln, 0, sizeof(*ln));
    ln->on = 1;
    ln->owner = owner;
    ln->xy = xy;
    ln->fiber = fiber;
    ln->script = script;
    ln->born = ++g_born;
    strip_color(raw, ln->text, sizeof(ln->text), &ln->color);
    if (xy) {
        ln->x = x;
        ln->y = y;
        boxw = DINK_SAY_PLAYX;
    } else if (owner == 1000) {
        owner_xy(owner, &ox, &oy);
        ln->x = ox;
        ln->y = oy;
        boxw = DINK_SAY_PLAYX - 20;
    } else {
        boxw = DINK_SAY_BOX_W;
    }
    wrap_line(ln->text, boxw);
    /* add_text_sprite: strlen of the DinkC string (color prefix counts). */
    ln->kill_ttl = (int)strlen(raw != NULL ? raw : "") * DINK_SAY_TEXT_TIMER;
    if (ln->kill_ttl < DINK_SAY_TEXT_MIN) {
        ln->kill_ttl = DINK_SAY_TEXT_MIN;
    }
    g_cur = slot;
    g_last_id = id_of(slot);
    place_line(ln);
    return slot;
}

int saybox_last_id(void)
{
    return g_last_id;
}

/* dc_sp_kill on the id say returned. ttl < 1 stays up (live_sprite
 * only expires kill_ttl > 0). 1 if id is a live text line. */
int saybox_set_ttl(int id, int ttl)
{
    int slot = id - SAY_ID_BASE;

    if (slot < 0 || slot >= SAY_LINES || !g_line[slot].on) {
        return 0;
    }
    g_line[slot].kill_ttl = ttl;
    return 1;
}

void saybox_clear_callback(int fiber)
{
    int i;

    if (fiber < 1) {
        return;
    }
    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].on && g_line[i].fiber == fiber) {
            g_line[i].fiber = 0;
        }
    }
}

/* Fiber is gone. Drop its callback and its script id so a reused slot
 * is not treated as the author of the leftover line. */
void saybox_forget_script(int script)
{
    int i;

    saybox_clear_callback(script);
    if (script < 1) {
        return;
    }
    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].on && g_line[i].script == script) {
            g_line[i].script = 0;
        }
    }
}

static int bad_sprite(int sprite)
{
    if (sprite != 0) {
        return 0;
    }
    printf("dinkc say: invalid sprite 0\n");
    return 1;
}

int saybox_set_from(const char *text, int sprite, int script)
{
    g_last_id = 0;
    if (bad_sprite(sprite)) {
        return 0;
    }
    /* dc_say: narrator does not kill an existing owner-1000 line. */
    if (sprite != 1000) {
        saybox_kill_owner(sprite);
    }
    (void)add_line(text, sprite, 0, 0, 0, 0, script);
    retarget();
    return g_last_id;
}

void saybox_set(const char *text, int sprite)
{
    saybox_set_from(text, sprite, 0);
}

int saybox_set_xy_from(const char *text, int x, int y, int script)
{
    g_last_id = 0;
    /* say_text_xy: add_text_sprite at (mx,my), text_owner 1000. */
    (void)add_line(text, 1000, 1, x, y, 0, script);
    return g_last_id;
}

void saybox_set_xy(const char *text, int x, int y)
{
    saybox_set_xy_from(text, x, y, 0);
}

int saybox_stop(const char *text, int sprite, int fiber, int npc)
{
    g_last_id = 0;
    if (bad_sprite(sprite)) {
        return 0;
    }
    /* dc_say_stop_npc: no-op if this sprite is already talking. */
    if (npc && line_owned(sprite)) {
        return 0;
    }
    if (!npc) {
        /* Includes owner 1000 (narrator / say_xy). */
        saybox_kill_owner(sprite);
        if (sprite != 1) {
            saybox_kill_owner(1);
        }
    }
    saybox_clear_callback(fiber);
    (void)add_line(text, sprite, 0, 0, 0, fiber, fiber);
    return 1;
}

int saybox_stop_xy(const char *text, int x, int y, int fiber)
{
    g_last_id = 0;
    saybox_clear_callback(fiber);
    (void)add_line(text, 1000, 1, x, y, fiber, fiber);
    return 1;
}

void saybox_expire_script(int script, int now_ms)
{
    int i;

    if (script < 1) {
        return;
    }
    for (i = 0; i < SAY_LINES; i++) {
        if (!g_line[i].on || g_line[i].script != script) {
            continue;
        }
        g_line[i].kill_armed = 1;
        g_line[i].kill_ttl = 1;
        g_line[i].kill_start = now_ms - 2;
    }
}

int saybox_tick(int now_ms)
{
    int i, any = 0;

    g_nexp = 0;
    for (i = 0; i < SAY_LINES; i++) {
        struct SayLine *ln = &g_line[i];

        if (!ln->on) {
            continue;
        }
        /* text_brain: owner gone → drop the line, do not run the
         * callback. The script stays in WAIT_SAY (FreeDink hangs too).
         * kill_ttl < 1 would be a permanent sign; we still drop it. */
        if (line_owner_gone(ln)) {
            ln->on = 0;
            ln->fiber = 0;
            if (g_cur == i) {
                g_cur = -1;
            }
            any = 1;
            continue;
        }
        if (ln->kill_ttl < 1) {
            continue;
        }
        /* FreeDink kill_start==0; our now_ms can be 0 so keep a flag. */
        if (!ln->kill_armed) {
            ln->kill_start = now_ms;
            ln->kill_armed = 1;
            continue;
        }
        if (ln->kill_start + ln->kill_ttl < now_ms) {
            if (ln->fiber > 0 && g_nexp < SAY_LINES) {
                g_exp[g_nexp++] = ln->fiber;
            }
            ln->on = 0;
            if (g_cur == i) {
                g_cur = -1;
            }
            any = 1;
        }
    }
    retarget();
    return any;
}

int saybox_expired_n(void)
{
    return g_nexp;
}

int saybox_expired_fiber(int i)
{
    if (i < 0 || i >= g_nexp) {
        return 0;
    }
    return g_exp[i];
}

void saybox_clear(void)
{
    memset(g_line, 0, sizeof(g_line));
    g_cur = -1;
    g_nexp = 0;
}

int saybox_active(void)
{
    int i;

    for (i = 0; i < SAY_LINES; i++) {
        if (g_line[i].on) {
            return 1;
        }
    }
    return 0;
}

const char *saybox_text(void)
{
    struct SayLine *ln = focus();

    return ln != NULL ? ln->text : "";
}

int saybox_x(void)
{
    struct SayLine *ln;

    place_all();
    ln = focus();
    return ln != NULL ? ln->x : 0;
}

int saybox_y(void)
{
    struct SayLine *ln;

    place_all();
    ln = focus();
    return ln != NULL ? ln->y : 0;
}

int saybox_line_x(const char *line)
{
    struct SayLine *ln;
    int cell, w, n = 0, boxw;
    const char *p = line != NULL ? line : "";

    place_all();
    ln = focus();
    cell = font_advance('A');
    if (cell < 1) {
        cell = DINK_FONT_CELL;
    }
    while (p[n] != '\0' && p[n] != '\n') {
        n++;
    }
    w = n * cell;
    boxw = ln != NULL ? say_box_w(ln) : DINK_SAY_BOX_W;
    return (ln != NULL ? ln->x : 0) + boxw / 2 - w / 2;
}

int saybox_color(void)
{
    struct SayLine *ln = focus();

    return ln != NULL ? ln->color : 14;
}

/* FreeDink text_draw: 50 px box, hcenter, then the glyph 1 px left of the rect. */
int saybox_num_x(int x, int num)
{
    char buf[16];
    const char *p;
    int w = 0;

    snprintf(buf, sizeof(buf), "%d", num);
    for (p = buf; *p != '\0'; p++) {
        w += font_advance((unsigned char)*p);
    }
    return x + 25 - w / 2 - 1;
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

/* One header per draw. Cap quads so 32 full lines cannot blow the
 * 512 KiB TA buffer. 400 glyphs × 5 (outline + fill) stays under it.
 * A busy bar is about 6 × 40. */
#define SAY_GLYPH_CAP 2000 /* 400 characters × outline + fill */

static int g_glyphs;
static int g_glyph_logged;
static int g_hdr_ok;
static pvr_poly_hdr_t g_hdr;

static void say_draw_begin(void)
{
    pvr_poly_cxt_t cxt;

    g_glyphs = 0;
    g_glyph_logged = 0;
    g_hdr_ok = 0;
    if (g_tex == NULL) {
        return;
    }
    pvr_poly_cxt_txr(&cxt, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB1555,
                     DINK_FONT_ATLAS_W, DINK_FONT_ATLAS_H, (pvr_ptr_t)g_tex,
                     PVR_FILTER_NONE);
    cxt.gen.alpha = PVR_ALPHA_ENABLE;
    cxt.txr.alpha = PVR_TXRALPHA_ENABLE;
    pvr_poly_compile(&g_hdr, &cxt);
    pvr_prim(&g_hdr, sizeof(g_hdr));
    g_hdr_ok = 1;
}

static void draw_ch(float x, float y, float z, int ch, uint32_t argb)
{
    pvr_vertex_t vert;
    int col, row;
    float u0, v0, u1, v1, cell;

    if (!g_hdr_ok || font_glyph_cell(ch, &col, &row) == 0) {
        return;
    }
    if (g_glyphs >= SAY_GLYPH_CAP) {
        if (!g_glyph_logged) {
            printf("say glyphs capped\n");
            g_glyph_logged = 1;
        }
        return;
    }
    g_glyphs++;
    cell = (float)DINK_FONT_CELL;
    u0 = (float)(col * DINK_FONT_CELL) / (float)DINK_FONT_ATLAS_W;
    v0 = (float)(row * DINK_FONT_CELL) / (float)DINK_FONT_ATLAS_H;
    u1 = u0 + cell / (float)DINK_FONT_ATLAS_W;
    v1 = v0 + cell / (float)DINK_FONT_ATLAS_H;
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

static void draw_line_text(const struct SayLine *ln, float z)
{
    const char *p;
    float x, y;
    uint32_t fg, bg;
    int saved = g_cur;

    if (ln == NULL || !ln->on) {
        return;
    }
    g_cur = (int)(ln - g_line);
    p = ln->text;
    y = (float)ln->y;
    fg = saybox_text_argb(ln->color, fade_brightness());
    bg = 0xFF080E15u;
    while (*p != '\0') {
        x = (float)saybox_line_x(p);
        while (*p != '\0' && *p != '\n') {
            draw_ch(x - 1.0f, y, z, (unsigned char)*p, bg);
            draw_ch(x + 1.0f, y, z, (unsigned char)*p, bg);
            draw_ch(x, y - 1.0f, z, (unsigned char)*p, bg);
            draw_ch(x, y + 1.0f, z, (unsigned char)*p, bg);
            /* Smaller than the 0.002 birth step so a later line's
             * outline is not covered by an earlier line's fill. */
            draw_ch(x, y, z + 0.001f, (unsigned char)*p, fg);
            x += (float)font_advance((unsigned char)*p);
            p++;
        }
        if (*p == '\n') {
            p++;
        }
        y += (float)DINK_FONT_CELL;
    }
    g_cur = saved;
}

/* Rank by birth so a later line draws above an earlier one. The step
 * stays inside the 0.1 gap under the choice menu (3.8) and inventory
 * (4.0): 32 * 0.002 + the 0.01 fill lift is under 0.1. */
static float line_z(float z, int i)
{
    int j, rank = 0;

    for (j = 0; j < SAY_LINES; j++) {
        if (!g_line[j].on || j == i) {
            continue;
        }
        if (g_line[j].born < g_line[i].born ||
            (g_line[j].born == g_line[i].born && j < i)) {
            rank++;
        }
    }
    return z + (float)rank * 0.002f;
}

void saybox_draw_pvr(float z)
{
    int i;

    if (g_tex == NULL) {
        return;
    }
    place_all();
    say_draw_begin();
    for (i = 0; i < SAY_LINES; i++) {
        /* Said and orphaned in the same tick (S1-FMONS die(): say on
         * &current_sprite, then kill_sprite): the next tick drops it; do
         * not draw it at owner_xy's (100, 100) fallback meanwhile. */
        if (g_line[i].on && !line_owner_gone(&g_line[i])) {
            draw_line_text(&g_line[i], line_z(z, i));
        }
    }
}

void saybox_draw_num_pvr(int x, int y, int num, float z, int exp)
{
    char buf[16];
    const char *p;
    float cx;
    uint32_t fg, bg;

    if (g_tex == NULL) {
        return;
    }
    say_draw_begin();
    snprintf(buf, sizeof(buf), "%d", num);
    if (y < 0) {
        y = 0;
    }
    cx = (float)saybox_num_x(x, num);
    fg = saybox_num_argb(exp, fade_brightness());
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
    /* Box blits each send their own header. Re-emit the font header
     * after them or the title and lines sample the choice texture. */
    say_draw_begin();
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
