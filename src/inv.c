/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "inv.h"

#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "pad.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/pvr.h>
#endif

#define DINK_INV_ICON_N 24

static struct SeqInfo *g_seqs;
static struct SpriteFrame g_menu[6];
static struct {
    int seq;
    int frame;
    struct SpriteFrame fr;
} g_icon[DINK_INV_ICON_N];
static int g_show;
static int g_curitem;
static int g_type;
static int g_pic;
static int g_timer;

void inv_cell_xy(int magic, int idx0, int *x, int *y)
{
    if (x == NULL || y == NULL) {
        return;
    }
    if (idx0 < 0) {
        idx0 = 0;
    }
    if (magic) {
        *x = DINK_INV_MX + (idx0 % 2) * DINK_INV_DX;
        *y = DINK_INV_MY + (idx0 / 2) * DINK_INV_DY;
    } else {
        *x = DINK_INV_WX + (idx0 % 4) * DINK_INV_DX;
        *y = DINK_INV_WY + (idx0 / 4) * DINK_INV_DY;
    }
}

static int load_fr(struct SeqInfo *seqs, int seq, int frame,
                   struct SpriteFrame *dst)
{
    sprite_frame_free(dst);
    if (seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ ||
        seqs[seq].prefix[0] == '\0') {
        return -1;
    }
    if (sprite_load_seq_frame(&seqs[seq], seq, frame, dst) != 0) {
        printf("inv skip seq=%d fr=%d\n", seq, frame);
        return -1;
    }
    return 0;
}

static struct SpriteFrame *icon_slot(int seq, int frame, int create)
{
    int i, empty = -1;

    if (seq < 1 || frame < 1) {
        return NULL;
    }
    for (i = 0; i < DINK_INV_ICON_N; i++) {
        if (g_icon[i].seq == seq && g_icon[i].frame == frame) {
            return &g_icon[i].fr;
        }
        if (empty < 0 && g_icon[i].seq == 0) {
            empty = i;
        }
    }
    if (!create || empty < 0 || g_seqs == NULL) {
        return NULL;
    }
    if (load_fr(g_seqs, seq, frame, &g_icon[empty].fr) != 0) {
        return NULL;
    }
    g_icon[empty].seq = seq;
    g_icon[empty].frame = frame;
    return &g_icon[empty].fr;
}

static void sync_icons(void)
{
    int i, seq, fr;

    for (i = 0; i < DINK_INV_ITEMS; i++) {
        if (!dinkc_cmd_inv_active(0, i)) {
            continue;
        }
        seq = dinkc_cmd_inv_seq(0, i);
        fr = dinkc_cmd_inv_frame(0, i);
        (void)icon_slot(seq, fr, 1);
    }
    for (i = 0; i < DINK_INV_MAGIC; i++) {
        if (!dinkc_cmd_inv_active(1, i)) {
            continue;
        }
        seq = dinkc_cmd_inv_seq(1, i);
        fr = dinkc_cmd_inv_frame(1, i);
        (void)icon_slot(seq, fr, 1);
    }
}

static void clamp_cur(void)
{
    int cap = g_type == DINK_INV_MAGIC_KIND ? DINK_INV_MAGIC : DINK_INV_ITEMS;

    if (g_curitem < 0 || g_curitem >= cap) {
        g_curitem = 0;
    }
}

int inv_load(struct SeqInfo *seqs)
{
    int f, ok = 0;

    inv_free();
    g_seqs = seqs;
    g_pic = 2;
    g_timer = 0;
    if (seqs == NULL) {
        return -1;
    }
    for (f = 1; f <= 5; f++) {
        if (load_fr(seqs, DINK_INV_SEQ, f, &g_menu[f]) == 0) {
            ok++;
        }
    }
    sync_icons();
    printf("inv gfx menu=%d/5\n", ok);
    return ok >= 5 ? 0 : -1;
}

void inv_free(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        sprite_frame_free(&g_menu[i]);
    }
    for (i = 0; i < DINK_INV_ICON_N; i++) {
        sprite_frame_free(&g_icon[i].fr);
        g_icon[i].seq = 0;
        g_icon[i].frame = 0;
    }
    g_seqs = NULL;
    g_show = 0;
}

void inv_reset(void)
{
    g_show = 0;
    g_curitem = 0;
    g_type = DINK_INV_WEAPON;
    g_pic = 2;
    g_timer = 0;
}

void inv_sync_icons(void)
{
    sync_icons();
}

void inv_open(int now_ms)
{
    g_show = 1;
    clamp_cur();
    g_pic = 2;
    g_timer = now_ms + 400;
    sync_icons();
}

void inv_close(void)
{
    g_show = 0;
}

int inv_showing(void)
{
    return g_show;
}

int inv_curitem(void)
{
    return g_curitem;
}

int inv_item_type(void)
{
    return g_type;
}

struct SpriteFrame *inv_menu_frame(int frame)
{
    if (frame < 1 || frame > 5) {
        return NULL;
    }
    return g_menu[frame].w > 0 ? &g_menu[frame] : NULL;
}

struct SpriteFrame *inv_icon_frame(int seq, int frame)
{
    return icon_slot(seq, frame, 0);
}

static void arm_cur(void)
{
    int yld = 0, rv = 0;

    if (g_type == DINK_INV_MAGIC_KIND) {
        if (!dinkc_cmd_inv_active(1, g_curitem)) {
            return;
        }
        dinkc_var_set("&cur_magic", g_curitem + 1, DINKC_GLOBAL_SCOPE, 1);
        (void)dinkc_cmd("arm_magic", NULL, 0, NULL, NULL, &yld, &rv);
        return;
    }
    if (!dinkc_cmd_inv_active(0, g_curitem)) {
        return;
    }
    dinkc_var_set("&cur_weapon", g_curitem + 1, DINKC_GLOBAL_SCOPE, 1);
    (void)dinkc_cmd("arm_weapon", NULL, 0, NULL, NULL, &yld, &rv);
}

void inv_tick(uint32_t prev, uint32_t buttons, int now_ms)
{
    int hor, vert;

    if (!g_show) {
        return;
    }
    clamp_cur();
    if (now_ms >= g_timer) {
        g_pic = (g_pic == 2) ? 3 : 2;
        g_timer = now_ms + 400;
    }
    if (pad_just_pressed(prev, buttons, DINK_PAD_Y)) {
        inv_close();
        return;
    }
    if (pad_just_pressed(prev, buttons, DINK_PAD_A)) {
        arm_cur();
        return;
    }
    if (g_type == DINK_INV_WEAPON) {
        hor = g_curitem % 4;
        vert = g_curitem / 4;
        if (pad_just_pressed(prev, buttons, DINK_PAD_RIGHT)) {
            if (hor < 3) {
                g_curitem++;
            }
        } else if (pad_just_pressed(prev, buttons, DINK_PAD_LEFT)) {
            if (hor > 0) {
                g_curitem--;
            } else {
                g_type = DINK_INV_MAGIC_KIND;
                g_curitem = vert * 2 + 1;
            }
        } else if (pad_just_pressed(prev, buttons, DINK_PAD_DOWN)) {
            if (vert < 3) {
                g_curitem += 4;
            }
        } else if (pad_just_pressed(prev, buttons, DINK_PAD_UP)) {
            if (vert > 0) {
                g_curitem -= 4;
            }
        }
        return;
    }
    hor = g_curitem % 2;
    vert = g_curitem / 2;
    if (pad_just_pressed(prev, buttons, DINK_PAD_RIGHT)) {
        if (hor < 1) {
            g_curitem++;
        } else {
            g_type = DINK_INV_WEAPON;
            g_curitem = vert * 4;
        }
    } else if (pad_just_pressed(prev, buttons, DINK_PAD_LEFT)) {
        if (hor > 0) {
            g_curitem--;
        }
    } else if (pad_just_pressed(prev, buttons, DINK_PAD_DOWN)) {
        if (vert < 3) {
            g_curitem += 2;
        }
    } else if (pad_just_pressed(prev, buttons, DINK_PAD_UP)) {
        if (vert > 0) {
            g_curitem -= 2;
        }
    }
}

#ifdef _arch_dreamcast
void inv_evict_pvr(void)
{
    int i;

    for (i = 1; i <= 5; i++) {
        sprite_evict_pvr(&g_menu[i]);
    }
    for (i = 0; i < DINK_INV_ICON_N; i++) {
        sprite_evict_pvr(&g_icon[i].fr);
    }
}

int inv_upload_pvr(void)
{
    int i, n = 0;

    for (i = 1; i <= 5; i++) {
        if (g_menu[i].tex != NULL) {
            n++;
            continue;
        }
        if (g_menu[i].argb1555 == NULL && g_seqs != NULL) {
            (void)load_fr(g_seqs, DINK_INV_SEQ, i, &g_menu[i]);
        }
        if (g_menu[i].argb1555 != NULL && sprite_upload_pvr(&g_menu[i]) == 0) {
            sprite_drop_cpu(&g_menu[i]);
            n++;
        }
    }
    for (i = 0; i < DINK_INV_ICON_N; i++) {
        if (g_icon[i].fr.tex != NULL) {
            continue;
        }
        if (g_icon[i].fr.argb1555 != NULL &&
            sprite_upload_pvr(&g_icon[i].fr) == 0) {
            sprite_drop_cpu(&g_icon[i].fr);
        }
    }
    return n >= 5 ? 0 : -1;
}

static void blit_item(int magic, int idx0, int seq, int frame, float z)
{
    struct SpriteFrame *fr;
    int x, y;

    if (seq == DINK_INV_SEQ) {
        fr = inv_menu_frame(frame);
    } else {
        fr = icon_slot(seq, frame, 0);
    }
    if (fr == NULL) {
        return;
    }
    inv_cell_xy(magic, idx0, &x, &y);
    sprite_blit_pvr(fr, (float)x, (float)y, z);
}

void inv_draw_pvr(float z)
{
    int i, curw, curm;

    if (!g_show) {
        return;
    }
    if (g_menu[1].tex != NULL) {
        sprite_blit_pvr(&g_menu[1], (float)DINK_INV_BG_X, (float)DINK_INV_BG_Y,
                        z);
    }
    for (i = 0; i < DINK_INV_MAGIC; i++) {
        if (dinkc_cmd_inv_active(1, i)) {
            blit_item(1, i, dinkc_cmd_inv_seq(1, i), dinkc_cmd_inv_frame(1, i),
                      z + 0.02f);
        }
    }
    /* FreeDink draws the armed-magic box with play.item[*pcur_magic-1];
     * that is the weapon row. Use mitem (the magic grid). */
    curm = dinkc_var_get("&cur_magic", DINKC_GLOBAL_SCOPE, 1);
    if (curm >= 1 && curm <= DINK_INV_MAGIC &&
        dinkc_cmd_inv_active(1, curm - 1)) {
        blit_item(1, curm - 1, DINK_INV_SEQ, 5, z + 0.04f);
    }
    for (i = 0; i < DINK_INV_ITEMS; i++) {
        if (dinkc_cmd_inv_active(0, i)) {
            blit_item(0, i, dinkc_cmd_inv_seq(0, i), dinkc_cmd_inv_frame(0, i),
                      z + 0.02f);
        }
    }
    curw = dinkc_var_get("&cur_weapon", DINKC_GLOBAL_SCOPE, 1);
    if (curw >= 1 && curw <= DINK_INV_ITEMS &&
        dinkc_cmd_inv_active(0, curw - 1)) {
        blit_item(0, curw - 1, DINK_INV_SEQ, 4, z + 0.04f);
    }
    clamp_cur();
    blit_item(g_type == DINK_INV_MAGIC_KIND, g_curitem, DINK_INV_SEQ, g_pic,
              z + 0.06f);
}
#endif

size_t inv_cpu_bytes(void)
{
    size_t t = 0;
    int i;

    for (i = 1; i <= 5; i++) {
        if (g_menu[i].argb1555 != NULL && g_menu[i].tw > 0 && g_menu[i].th > 0) {
            t += (size_t)g_menu[i].tw * (size_t)g_menu[i].th * 2u;
        }
    }
    for (i = 0; i < DINK_INV_ICON_N; i++) {
        if (g_icon[i].fr.argb1555 != NULL && g_icon[i].fr.tw > 0 &&
            g_icon[i].fr.th > 0) {
            t += (size_t)g_icon[i].fr.tw * (size_t)g_icon[i].fr.th * 2u;
        }
    }
    return t;
}

void inv_drop_cpu(void)
{
    int i;

    for (i = 1; i <= 5; i++) {
        sprite_drop_cpu(&g_menu[i]);
    }
    for (i = 0; i < DINK_INV_ICON_N; i++) {
        sprite_drop_cpu(&g_icon[i].fr);
    }
}
