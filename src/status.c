/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "status.h"

#include "bmp.h"
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "fs.h"
#include "inv.h"
#include "script.h"
#include "sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

#define HUD_GLYPH_N 96
#define HUD_CHROME_ROWS 3

struct HudGlyph {
    int seq;
    int frame;
    int sl;
    int st;
    int sr;
    int sb;
    int adv;
};

static struct SeqInfo *g_seqs;
static uint16_t *g_digit;
static uint16_t *g_chrome;
static uint16_t *g_side;
static struct SpriteFrame g_digit_fr;
static struct SpriteFrame g_chrome_fr;
static struct SpriteFrame g_side_fr;
static struct HudGlyph g_glyph[HUD_GLYPH_N];
static int g_nglyph;
static int g_pack_x;
static int g_pack_y;
static int g_pack_h;
static int g_flife;
static int g_flifemax;
static int g_fexp;
static int g_fraise;
static int g_fstrength;
static int g_fdefense;
static int g_fmagic;
static int g_fgold;
static int g_status_ms;
static int g_inited;
static int g_map_on;
static int g_map_dot;
static int g_map_fiber;
static int g_map_frame;
static int g_map_timer;
static struct SpriteFrame g_map;
static struct SpriteFrame g_mark[8];

int status_next_raise(int level)
{
    int n;

    if (level < 1) {
        level = 1;
    }
    n = (100 * level) * level;
    if (n > 99999) {
        n = 99999;
    }
    return n;
}

int status_atlas_bytes(void)
{
    return DINK_HUD_ATLAS_BYTES;
}

static int glyph_find(int seq, int frame)
{
    int i;

    for (i = 0; i < g_nglyph; i++) {
        if (g_glyph[i].seq == seq && g_glyph[i].frame == frame) {
            return i;
        }
    }
    return -1;
}

int status_glyph(int seq, int frame, int *sl, int *st, int *sr, int *sb,
                 int *adv)
{
    int i = glyph_find(seq, frame);

    if (i < 0) {
        return -1;
    }
    if (sl != NULL) {
        *sl = g_glyph[i].sl;
    }
    if (st != NULL) {
        *st = g_glyph[i].st;
    }
    if (sr != NULL) {
        *sr = g_glyph[i].sr;
    }
    if (sb != NULL) {
        *sb = g_glyph[i].sb;
    }
    if (adv != NULL) {
        *adv = g_glyph[i].adv;
    }
    return 0;
}

static int pack_copy(uint16_t *atlas, int aw, int ah, const struct SpriteFrame *fr,
                     int *ox, int *oy)
{
    int x, y, w, h;

    if (fr == NULL || fr->argb1555 == NULL || atlas == NULL) {
        return -1;
    }
    w = fr->w;
    h = fr->h;
    if (g_pack_x + w > aw) {
        g_pack_x = 0;
        g_pack_y += g_pack_h;
        g_pack_h = 0;
    }
    if (g_pack_y + h > ah) {
        return -1;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            atlas[(g_pack_y + y) * aw + (g_pack_x + x)] =
                fr->argb1555[y * fr->tw + x];
        }
    }
    *ox = g_pack_x;
    *oy = g_pack_y;
    g_pack_x += w;
    if (h > g_pack_h) {
        g_pack_h = h;
    }
    return 0;
}

static int add_glyph(int seq, int frame)
{
    struct SpriteFrame fr;
    int ox = 0, oy = 0;

    memset(&fr, 0, sizeof(fr));
    if (g_seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ) {
        return -1;
    }
    if (g_nglyph >= HUD_GLYPH_N) {
        return -1;
    }
    if (sprite_load_seq_frame(&g_seqs[seq], seq, frame, &fr) != 0) {
        return -1;
    }
    if (pack_copy(g_digit, DINK_HUD_ATLAS, DINK_HUD_ATLAS, &fr, &ox, &oy) != 0) {
        sprite_frame_free(&fr);
        return -1;
    }
    g_glyph[g_nglyph].seq = seq;
    g_glyph[g_nglyph].frame = frame;
    g_glyph[g_nglyph].sl = ox;
    g_glyph[g_nglyph].st = oy;
    g_glyph[g_nglyph].sr = ox + fr.w;
    g_glyph[g_nglyph].sb = oy + fr.h;
    g_glyph[g_nglyph].adv = fr.w;
    g_nglyph++;
    sprite_frame_free(&fr);
    return 0;
}

static int add_level_digit(int frame)
{
    char rel[80];
    const uint8_t *p = NULL;
    size_t n = 0;
    struct Bitmap bm;
    struct SpriteFrame fr;
    int x, y, ox = 0, oy = 0, tw, th;
    uint16_t *pad;

    snprintf(rel, sizeof(rel), "graphics/inter/level#/ln-%02d.bmp", frame);
    if (dink_blob_get(rel, &p, &n) != 0 || p == NULL) {
        return -1;
    }
    memset(&bm, 0, sizeof(bm));
    memset(&fr, 0, sizeof(fr));
    if (bitmap_load_mem(p, n, &bm) != 0) {
        return -1;
    }
    tw = 8;
    th = 8;
    while (tw < bm.w) {
        tw <<= 1;
    }
    while (th < bm.h) {
        th <<= 1;
    }
    pad = (uint16_t *)calloc((size_t)tw * (size_t)th, 2u);
    if (pad == NULL) {
        bitmap_free(&bm);
        return -1;
    }
    for (y = 0; y < bm.h; y++) {
        for (x = 0; x < bm.w; x++) {
            uint8_t r = 0, g = 0, b = 0, i;

            if (bm.bpp == 8) {
                i = bm.pixels[y * bm.stride + x];
                r = bm.pal[i * 3];
                g = bm.pal[i * 3 + 1];
                b = bm.pal[i * 3 + 2];
            }
            if (r > 240 && g > 240 && b > 240) {
                pad[y * tw + x] = 0;
            } else {
                pad[y * tw + x] = (uint16_t)(0x8000u | ((r >> 3) << 10) |
                                             ((g >> 3) << 5) | (b >> 3));
            }
        }
    }
    fr.w = bm.w;
    fr.h = bm.h;
    fr.tw = tw;
    fr.th = th;
    fr.argb1555 = pad;
    bitmap_free(&bm);
    if (pack_copy(g_digit, DINK_HUD_ATLAS, DINK_HUD_ATLAS, &fr, &ox, &oy) != 0) {
        sprite_frame_free(&fr);
        return -1;
    }
    g_glyph[g_nglyph].seq = DINK_STATUS_LEVEL_SEQ;
    g_glyph[g_nglyph].frame = frame;
    g_glyph[g_nglyph].sl = ox;
    g_glyph[g_nglyph].st = oy;
    g_glyph[g_nglyph].sr = ox + fr.w;
    g_glyph[g_nglyph].sb = oy + fr.h;
    g_glyph[g_nglyph].adv = fr.w;
    g_nglyph++;
    sprite_frame_free(&fr);
    return 0;
}

#ifdef _arch_dreamcast
static int rnum_of(char c)
{
    if (c == '0') {
        return 10;
    }
    if (c >= '1' && c <= '9') {
        return c - '0';
    }
    if (c == '/') {
        return 11;
    }
    return 0;
}

static int draw_num(int seq, const char *nums, int mx, int my, float z)
{
    int i, length = 0, rnum, sl, st, sr, sb, adv;

    if (nums == NULL) {
        return 0;
    }
    for (i = 0; nums[i] != '\0'; i++) {
        rnum = rnum_of(nums[i]);
        if (rnum < 1 || status_glyph(seq, rnum, &sl, &st, &sr, &sb, &adv) != 0) {
            continue;
        }
#ifdef _arch_dreamcast
        if (g_digit_fr.tex != NULL) {
            sprite_blit_pvr_src(&g_digit_fr, (float)(mx + length), (float)my, z,
                                sl, st, sr, sb);
        }
#else
        (void)z;
        (void)my;
#endif
        length += adv;
    }
    return length;
}

static void draw_bar(int life, int seqman, float z)
{
    int cur = 0, curx = 284, cury = 412, rnum = 3;
    int sl, st, sr, sb, adv, w;

    while (1) {
        cur++;
        if (cur > life) {
            cur--;
            if (cur - (cur / 10) * 10 != 0 &&
                status_glyph(seqman, rnum, &sl, &st, &sr, &sb, &adv) == 0) {
                w = (sr - sl) * ((cur - (cur / 10) * 10) * 10) / 100;
#ifdef _arch_dreamcast
                if (w > 0 && g_digit_fr.tex != NULL) {
                    sprite_blit_pvr_src(&g_digit_fr, (float)curx, (float)cury, z,
                                        sl, st, sl + w, sb);
                }
#else
                (void)z;
                (void)w;
#endif
            }
            return;
        }
        rnum = 2;
        if (cur < 11) {
            rnum = 1;
        }
        if (cur == dinkc_var_get("&lifemax", DINKC_GLOBAL_SCOPE, 1)) {
            rnum = 3;
        }
        if ((cur / 10) * 10 == cur) {
            if (status_glyph(seqman, rnum, &sl, &st, &sr, &sb, &adv) == 0) {
#ifdef _arch_dreamcast
                if (g_digit_fr.tex != NULL) {
                    sprite_blit_pvr_src(&g_digit_fr, (float)curx, (float)cury, z,
                                        sl, st, sr, sb);
                }
#endif
                curx += adv;
            }
            if (cur == 110) {
                cury += (sb - st) + 5;
                curx = 284;
            }
            if (cur == 220) {
                return;
            }
        }
    }
}

#ifdef _arch_dreamcast
static void blit_glyph_src(int sl, int st, int sr, int sb, int dx, int dy,
                           float z)
{
    if (g_digit_fr.tex != NULL && sr > sl && sb > st) {
        sprite_blit_pvr_src(&g_digit_fr, (float)dx, (float)dy, z, sl, st, sr,
                            sb);
    }
}
#endif

static void draw_mgauge(int percent, float z)
{
    int sl, st, sr, sb, adv, p, cut, full;

#ifdef _arch_dreamcast
    if (percent > 0 &&
        status_glyph(DINK_STATUS_SEQ, 6, &sl, &st, &sr, &sb, &adv) == 0) {
        p = percent > 25 ? 25 : percent;
        p *= 4;
        full = sb - st;
        cut = (full * p) / 100;
        blit_glyph_src(sl, sb - cut, sr, sb, 149, 411 + (full - cut), z);
    }
    percent -= 25;
    if (percent > 0 &&
        status_glyph(DINK_STATUS_SEQ, 7, &sl, &st, &sr, &sb, &adv) == 0) {
        p = percent > 25 ? 25 : percent;
        p *= 4;
        full = sr - sl;
        cut = (full * p) / 100;
        blit_glyph_src(sl, st, sl + cut, sb, 149, 409, z);
    }
    percent -= 25;
    if (percent > 0 &&
        status_glyph(DINK_STATUS_SEQ, 6, &sl, &st, &sr, &sb, &adv) == 0) {
        p = percent > 25 ? 25 : percent;
        p *= 4;
        full = sb - st;
        cut = (full * p) / 100;
        blit_glyph_src(sl, st, sr, st + cut, 215, 411, z);
    }
    percent -= 25;
    if (percent > 0 &&
        status_glyph(DINK_STATUS_SEQ, 7, &sl, &st, &sr, &sb, &adv) == 0) {
        p = percent > 25 ? 25 : percent;
        p *= 4;
        full = sr - sl;
        cut = (full * p) / 100;
        blit_glyph_src(sr - cut, st, sr, sb, 149 + (full - cut), 466, z);
    }
#else
    (void)percent;
    (void)z;
    (void)sl;
    (void)st;
    (void)sr;
    (void)sb;
    (void)adv;
    (void)p;
    (void)cut;
    (void)full;
#endif
}

static void draw_icons(float z)
{
#ifdef _arch_dreamcast
    int seq, fr;
    struct SpriteFrame *ic;

    if (dinkc_cmd_inv_active(0, dinkc_var_get("&cur_weapon", DINKC_GLOBAL_SCOPE,
                                              1) -
                                    1)) {
        seq = dinkc_cmd_inv_seq(
            0, dinkc_var_get("&cur_weapon", DINKC_GLOBAL_SCOPE, 1) - 1);
        fr = dinkc_cmd_inv_frame(
            0, dinkc_var_get("&cur_weapon", DINKC_GLOBAL_SCOPE, 1) - 1);
        ic = inv_icon_frame(seq, fr);
        if (ic != NULL && ic->tex != NULL) {
            sprite_blit_pvr(ic, 557.0f, 413.0f, z);
        }
    }
    if (dinkc_cmd_inv_active(1, dinkc_var_get("&cur_magic", DINKC_GLOBAL_SCOPE,
                                              1) -
                                    1)) {
        seq = dinkc_cmd_inv_seq(
            1, dinkc_var_get("&cur_magic", DINKC_GLOBAL_SCOPE, 1) - 1);
        fr = dinkc_cmd_inv_frame(
            1, dinkc_var_get("&cur_magic", DINKC_GLOBAL_SCOPE, 1) - 1);
        ic = inv_icon_frame(seq, fr);
        if (ic != NULL && ic->tex != NULL) {
            sprite_blit_pvr(ic, 153.0f, 413.0f, z);
        }
    }
#else
    (void)z;
#endif
}

#endif /* _arch_dreamcast */

static void sync_from_vars(void)
{
    int lv = dinkc_var_get("&level", DINKC_GLOBAL_SCOPE, 1);

    g_fraise = status_next_raise(lv < 1 ? 1 : lv);
    g_fexp = dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1);
    if (g_fexp >= g_fraise) {
        g_fexp = g_fraise - 1;
    }
    g_fstrength = dinkc_var_get("&strength", DINKC_GLOBAL_SCOPE, 1);
    g_fmagic = dinkc_var_get("&magic", DINKC_GLOBAL_SCOPE, 1);
    g_fgold = dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1);
    g_fdefense = dinkc_var_get("&defense", DINKC_GLOBAL_SCOPE, 1);
    g_flifemax = dinkc_var_get("&lifemax", DINKC_GLOBAL_SCOPE, 1);
    g_flife = dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1);
}

void status_draw_all(void)
{
    sync_from_vars();
    g_inited = 1;
}

void status_update(int now_ms)
{
    int next, drawexp = 0, cost, lv, mg;
    int life, lifemax;

    if (now_ms - g_status_ms < 100 && g_inited) {
        return;
    }
    g_status_ms = now_ms;
    if (!g_inited) {
        status_draw_all();
    }
    next = status_next_raise(dinkc_var_get("&level", DINKC_GLOBAL_SCOPE, 1));
    if (next != g_fraise) {
        g_fraise += next / 40;
        if (g_fraise > next) {
            g_fraise = next;
        }
        drawexp = 1;
    }
    if (dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1) != g_fexp) {
        g_fexp += 10;
        if (g_fexp > dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1)) {
            g_fexp = dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1);
        }
        drawexp = 1;
        if (g_fexp >= g_fraise) {
            int exp = dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1) - next;

            dinkc_var_set("&exp", exp < 0 ? 0 : exp, DINKC_GLOBAL_SCOPE, 1);
            g_fexp = 0;
            (void)script_on_raise();
        }
    }
    (void)drawexp;
    lifemax = dinkc_var_get("&lifemax", DINKC_GLOBAL_SCOPE, 1);
    life = dinkc_var_get("&life", DINKC_GLOBAL_SCOPE, 1);
    if (g_flifemax != lifemax || g_flife != life) {
        if (g_flifemax < lifemax) {
            g_flifemax++;
        }
        if (g_flifemax > lifemax) {
            g_flifemax--;
        }
        if (g_flife > life) {
            g_flife--;
        }
        if (g_flife < life) {
            g_flife++;
        }
        if (g_flife > life) {
            g_flife--;
        }
        if (g_flife < life) {
            g_flife++;
        }
    }
    if (g_fstrength != dinkc_var_get("&strength", DINKC_GLOBAL_SCOPE, 1)) {
        if (g_fstrength < dinkc_var_get("&strength", DINKC_GLOBAL_SCOPE, 1)) {
            g_fstrength++;
        } else {
            g_fstrength--;
        }
    }
    if (g_fdefense != dinkc_var_get("&defense", DINKC_GLOBAL_SCOPE, 1)) {
        if (g_fdefense < dinkc_var_get("&defense", DINKC_GLOBAL_SCOPE, 1)) {
            g_fdefense++;
        } else {
            g_fdefense--;
        }
    }
    if (g_fmagic != dinkc_var_get("&magic", DINKC_GLOBAL_SCOPE, 1)) {
        if (g_fmagic < dinkc_var_get("&magic", DINKC_GLOBAL_SCOPE, 1)) {
            g_fmagic++;
        } else {
            g_fmagic--;
        }
    }
    if (g_fgold != dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1)) {
        if (g_fgold < dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1)) {
            g_fgold += 20;
            if (g_fgold > dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1)) {
                g_fgold = dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1);
            }
        } else {
            g_fgold -= 20;
            if (g_fgold < dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1)) {
                g_fgold = dinkc_var_get("&gold", DINKC_GLOBAL_SCOPE, 1);
            }
        }
    }
    cost = dinkc_var_get("&magic_cost", DINKC_GLOBAL_SCOPE, 1);
    lv = dinkc_var_get("&magic_level", DINKC_GLOBAL_SCOPE, 1);
    mg = dinkc_var_get("&magic", DINKC_GLOBAL_SCOPE, 1);
    if (lv < cost && !inv_showing()) {
        lv += mg;
        if (lv > cost) {
            lv = cost;
        }
        dinkc_var_set("&magic_level", lv, DINKC_GLOBAL_SCOPE, 1);
    }
    if (g_flife < 1) {
        (void)script_on_dink_die();
    }
}

int status_flife(void)
{
    return g_flife;
}

int status_fgold(void)
{
    return g_fgold;
}

int status_fexp(void)
{
    return g_fexp;
}

static int pack_chrome(void)
{
    struct SpriteFrame bar, s1, s2;
    int x, y, col, row;

    memset(&bar, 0, sizeof(bar));
    memset(&s1, 0, sizeof(s1));
    memset(&s2, 0, sizeof(s2));
    memset(g_chrome, 0, DINK_HUD_ATLAS_BYTES);
    memset(g_side, 0, 64u * 512u * 2u);
    if (sprite_load_seq_frame(&g_seqs[DINK_STATUS_SEQ], DINK_STATUS_SEQ, 3,
                              &bar) != 0) {
        return -1;
    }
    row = 0;
    for (col = 0; col < bar.w && row < HUD_CHROME_ROWS; col += DINK_HUD_ATLAS) {
        int w = bar.w - col;

        if (w > DINK_HUD_ATLAS) {
            w = DINK_HUD_ATLAS;
        }
        for (y = 0; y < bar.h && y < 80; y++) {
            for (x = 0; x < w; x++) {
                g_chrome[(row * 80 + y) * DINK_HUD_ATLAS + x] =
                    bar.argb1555[y * bar.tw + (col + x)];
            }
        }
        row++;
    }
    sprite_frame_free(&bar);
    if (sprite_load_seq_frame(&g_seqs[DINK_STATUS_SEQ], DINK_STATUS_SEQ, 1,
                              &s1) == 0) {
        for (y = 0; y < s1.h && y < 400; y++) {
            for (x = 0; x < s1.w && x < 20; x++) {
                g_side[y * 64 + x] = s1.argb1555[y * s1.tw + x];
            }
        }
        sprite_frame_free(&s1);
    }
    if (sprite_load_seq_frame(&g_seqs[DINK_STATUS_SEQ], DINK_STATUS_SEQ, 2,
                              &s2) == 0) {
        for (y = 0; y < s2.h && y < 400; y++) {
            for (x = 0; x < s2.w && x < 20; x++) {
                g_side[y * 64 + 32 + x] = s2.argb1555[y * s2.tw + x];
            }
        }
        sprite_frame_free(&s2);
    }
    g_chrome_fr.w = DINK_HUD_ATLAS;
    g_chrome_fr.h = DINK_HUD_ATLAS;
    g_chrome_fr.tw = DINK_HUD_ATLAS;
    g_chrome_fr.th = DINK_HUD_ATLAS;
    g_chrome_fr.argb1555 = g_chrome;
    g_side_fr.w = 64;
    g_side_fr.h = 512;
    g_side_fr.tw = 64;
    g_side_fr.th = 512;
    g_side_fr.argb1555 = g_side;
    return 0;
}

int status_load(struct SeqInfo *seqs)
{
    int seq, fr, n = 0;

    status_free();
    g_seqs = seqs;
    g_pack_x = g_pack_y = g_pack_h = 0;
    g_digit = (uint16_t *)calloc(DINK_HUD_ATLAS * DINK_HUD_ATLAS, 2u);
    g_chrome = (uint16_t *)calloc(DINK_HUD_ATLAS * DINK_HUD_ATLAS, 2u);
    g_side = (uint16_t *)calloc(64u * 512u, 2u);
    if (g_digit == NULL || g_chrome == NULL || g_side == NULL) {
        status_free();
        return -1;
    }
    if (seqs == NULL) {
        return -1;
    }
    for (seq = 181; seq <= 185; seq++) {
        int last = seq == 181 ? 11 : 10;

        for (fr = 1; fr <= last; fr++) {
            if (add_glyph(seq, fr) == 0) {
                n++;
            }
        }
    }
    for (fr = 1; fr <= 3; fr++) {
        if (add_glyph(190, fr) == 0) {
            n++;
        }
        if (add_glyph(451, fr) == 0) {
            n++;
        }
    }
    if (add_glyph(DINK_STATUS_SEQ, 6) == 0) {
        n++;
    }
    if (add_glyph(DINK_STATUS_SEQ, 7) == 0) {
        n++;
    }
    for (fr = 1; fr <= 10; fr++) {
        if (add_level_digit(fr) == 0) {
            n++;
        }
    }
    g_digit_fr.w = DINK_HUD_ATLAS;
    g_digit_fr.h = DINK_HUD_ATLAS;
    g_digit_fr.tw = DINK_HUD_ATLAS;
    g_digit_fr.th = DINK_HUD_ATLAS;
    g_digit_fr.argb1555 = g_digit;
    if (pack_chrome() != 0) {
        printf("status chrome pack fail\n");
    }
    printf("status glyphs=%d atlas=%d\n", n, DINK_HUD_ATLAS_BYTES);
    fflush(stdout);
    return n >= 40 ? 0 : -1;
}

void status_free(void)
{
    int i;

    sprite_evict_pvr(&g_digit_fr);
    sprite_evict_pvr(&g_chrome_fr);
    sprite_evict_pvr(&g_side_fr);
    free(g_digit);
    free(g_chrome);
    free(g_side);
    g_digit = NULL;
    g_chrome = NULL;
    g_side = NULL;
    g_digit_fr.argb1555 = NULL;
    g_chrome_fr.argb1555 = NULL;
    g_side_fr.argb1555 = NULL;
    memset(&g_digit_fr, 0, sizeof(g_digit_fr));
    memset(&g_chrome_fr, 0, sizeof(g_chrome_fr));
    memset(&g_side_fr, 0, sizeof(g_side_fr));
    sprite_frame_free(&g_map);
    for (i = 0; i < 8; i++) {
        sprite_frame_free(&g_mark[i]);
    }
    g_seqs = NULL;
    g_nglyph = 0;
    g_inited = 0;
    g_map_on = 0;
}

size_t status_cpu_bytes(void)
{
    size_t t = 0;

    if (g_digit_fr.argb1555 != NULL) {
        t += DINK_HUD_ATLAS_BYTES;
    }
    if (g_chrome_fr.argb1555 != NULL) {
        t += DINK_HUD_ATLAS_BYTES;
    }
    if (g_side_fr.argb1555 != NULL) {
        t += 64u * 512u * 2u;
    }
    if (g_map.argb1555 != NULL && g_map.tw > 0 && g_map.th > 0) {
        t += (size_t)g_map.tw * (size_t)g_map.th * 2u;
    }
    return t;
}

void status_drop_cpu(void)
{
    if (g_digit_fr.argb1555 == g_digit) {
        g_digit_fr.argb1555 = NULL;
        free(g_digit);
        g_digit = NULL;
    } else {
        sprite_drop_cpu(&g_digit_fr);
    }
    if (g_chrome_fr.argb1555 == g_chrome) {
        g_chrome_fr.argb1555 = NULL;
        free(g_chrome);
        g_chrome = NULL;
    } else {
        sprite_drop_cpu(&g_chrome_fr);
    }
    if (g_side_fr.argb1555 == g_side) {
        g_side_fr.argb1555 = NULL;
        free(g_side);
        g_side = NULL;
    } else {
        sprite_drop_cpu(&g_side_fr);
    }
    sprite_drop_cpu(&g_map);
}

int status_show_bmp(const char *rel, int showdot, int fiber)
{
    const uint8_t *p = NULL;
    size_t n = 0;
    struct Bitmap bm;
    char path[96];
    int x, y, tw, th;
    uint16_t *pad;

    if (rel == NULL || rel[0] == '\0') {
        return -1;
    }
    strncpy(path, rel, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    for (x = 0; path[x] != '\0'; x++) {
        if (path[x] == '\\') {
            path[x] = '/';
        }
    }
    sprite_frame_free(&g_map);
    if (dink_blob_get(path, &p, &n) != 0 || p == NULL) {
        printf("show_bmp miss %s\n", path);
        return -1;
    }
    memset(&bm, 0, sizeof(bm));
    if (bitmap_load_mem(p, n, &bm) != 0) {
        return -1;
    }
    tw = 8;
    th = 8;
    while (tw < bm.w) {
        tw <<= 1;
    }
    while (th < bm.h) {
        th <<= 1;
    }
    pad = (uint16_t *)calloc((size_t)tw * (size_t)th, 2u);
    if (pad == NULL) {
        bitmap_free(&bm);
        return -1;
    }
    for (y = 0; y < bm.h; y++) {
        for (x = 0; x < bm.w; x++) {
            uint8_t r = 0, g = 0, b = 0, i;

            if (bm.bpp == 8) {
                i = bm.pixels[y * bm.stride + x];
                r = bm.pal[i * 3];
                g = bm.pal[i * 3 + 1];
                b = bm.pal[i * 3 + 2];
            } else if (bm.bpp == 24) {
                b = bm.pixels[y * bm.stride + x * 3];
                g = bm.pixels[y * bm.stride + x * 3 + 1];
                r = bm.pixels[y * bm.stride + x * 3 + 2];
            }
            pad[y * tw + x] = (uint16_t)(0x8000u | ((r >> 3) << 10) |
                                         ((g >> 3) << 5) | (b >> 3));
        }
    }
    g_map.w = bm.w;
    g_map.h = bm.h;
    g_map.tw = tw;
    g_map.th = th;
    g_map.argb1555 = pad;
    bitmap_free(&bm);
    if (showdot && g_seqs != NULL) {
        int f;

        for (f = 1; f <= 7; f++) {
            sprite_frame_free(&g_mark[f]);
            (void)sprite_load_seq_frame(&g_seqs[DINK_MAP_MARK_SEQ],
                                        DINK_MAP_MARK_SEQ, f, &g_mark[f]);
        }
    }
    g_map_on = 1;
    g_map_dot = showdot;
    g_map_fiber = fiber;
    g_map_frame = 1;
    g_map_timer = 0;
    printf("show_bmp %s dot=%d\n", path, showdot);
    return 0;
}

int status_map_active(void)
{
    return g_map_on;
}

void status_map_tick(int now_ms)
{
    if (!g_map_on) {
        return;
    }
    if (now_ms >= g_map_timer) {
        g_map_frame++;
        if (g_map_frame < 1 || g_map_frame > 7) {
            g_map_frame = 1;
        }
        g_map_timer = now_ms + 50;
    }
}

void status_map_dismiss(void)
{
    if (!g_map_on) {
        return;
    }
    g_map_on = 0;
    sprite_drop_cpu(&g_map);
}

#ifdef _arch_dreamcast
static int upload_fr(struct SpriteFrame *f)
{
    if (f->tex != NULL) {
        return 0;
    }
    if (f->argb1555 == NULL) {
        return -1;
    }
    if (sprite_upload_pvr(f) != 0) {
        return -1;
    }
    if (f->argb1555 == g_digit || f->argb1555 == g_chrome ||
        f->argb1555 == g_side) {
        f->argb1555 = NULL;
    } else {
        sprite_drop_cpu(f);
    }
    return 0;
}

int status_upload_pvr(void)
{
    int f, n = 0;

    if (upload_fr(&g_digit_fr) == 0) {
        n++;
    }
    if (upload_fr(&g_chrome_fr) == 0) {
        n++;
    }
    (void)upload_fr(&g_side_fr);
    if (g_map.argb1555 != NULL || g_map.tex != NULL) {
        (void)upload_fr(&g_map);
    }
    for (f = 1; f <= 7; f++) {
        if (g_mark[f].argb1555 != NULL) {
            (void)upload_fr(&g_mark[f]);
        }
    }
    return n >= 2 ? 0 : -1;
}

void status_draw_pvr(float z)
{
    char buf[16];
    int cost, lv, p, row;
    int dx[] = {0, 256, 512};

    if (g_chrome_fr.tex != NULL) {
        for (row = 0; row < HUD_CHROME_ROWS; row++) {
            int w = row < 2 ? 256 : 128;

            sprite_blit_pvr_src(&g_chrome_fr, (float)dx[row], 400.0f, z, 0,
                                row * 80, w, row * 80 + 80);
        }
    }
    if (g_side_fr.tex != NULL) {
        sprite_blit_pvr_src(&g_side_fr, 0.0f, 0.0f, z, 0, 0, 20, 400);
        sprite_blit_pvr_src(&g_side_fr, 620.0f, 0.0f, z, 32, 0, 52, 400);
    }
    snprintf(buf, sizeof(buf), "%05d/%05d", g_fexp, g_fraise);
    draw_num(181, buf, 404, 459, z + 0.02f);
    snprintf(buf, sizeof(buf), "%03d", g_fstrength);
    draw_num(182, buf, 81, 415, z + 0.02f);
    snprintf(buf, sizeof(buf), "%03d", g_fdefense);
    draw_num(183, buf, 81, 437, z + 0.02f);
    snprintf(buf, sizeof(buf), "%03d", g_fmagic);
    draw_num(184, buf, 81, 459, z + 0.02f);
    snprintf(buf, sizeof(buf), g_fgold < 100000 ? "%05d" : "%06d", g_fgold);
    draw_num(185, buf, 298, 457, z + 0.02f);
    {
        int level = dinkc_var_get("&level", DINKC_GLOBAL_SCOPE, 1);

        snprintf(buf, sizeof(buf), "%d", level < 1 ? 1 : level);
        draw_num(DINK_STATUS_LEVEL_SEQ, buf, strlen(buf) == 1 ? 528 : 523, 456,
                 z + 0.02f);
    }
    draw_bar(g_flifemax, 190, z + 0.01f);
    draw_bar(g_flife, 451, z + 0.015f);
    cost = dinkc_var_get("&magic_cost", DINKC_GLOBAL_SCOPE, 1);
    lv = dinkc_var_get("&magic_level", DINKC_GLOBAL_SCOPE, 1);
    if (cost > 0 && lv > 0) {
        p = lv * 100 / cost;
        draw_mgauge(p, z + 0.02f);
    }
    draw_icons(z + 0.03f);
}

void status_draw_map_pvr(float z)
{
    int map, x, y;

    if (!g_map_on || g_map.tex == NULL) {
        return;
    }
    sprite_blit_pvr(&g_map, 0.0f, 0.0f, z);
    if (!g_map_dot) {
        return;
    }
    map = dinkc_var_get("&player_map", DINKC_GLOBAL_SCOPE, 1) - 1;
    if (map < 0) {
        map = 0;
    }
    x = (map % 32) * 20;
    y = (map / 32) * 20;
    if (g_map_frame >= 1 && g_map_frame <= 7 && g_mark[g_map_frame].tex != NULL) {
        sprite_blit_pvr(&g_mark[g_map_frame], (float)x, (float)y, z + 0.05f);
    }
}
#endif
