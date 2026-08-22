/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startmenu.h"

#include "pad.h"

#ifdef _arch_dreamcast
#include "ini.h"
#include "save.h"
#include "saybox.h"
#include "sprite.h"
#include "tiles.h"

#include <kos.h>
#include <stdio.h>
#include <string.h>
#endif

static int g_focus;
static int g_pause_focus;
static int g_pause_open;
static int g_slot;

void startmenu_reset(void)
{
    g_focus = STARTMENU_NEW;
}

int startmenu_focus(void)
{
    return g_focus;
}

int startmenu_tick(uint32_t prev, uint32_t now)
{
    if (pad_just_pressed(prev, now, DINK_PAD_DOWN) ||
        pad_just_pressed(prev, now, DINK_PAD_RIGHT)) {
        g_focus = (g_focus + 1) % STARTMENU_N;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_UP) ||
        pad_just_pressed(prev, now, DINK_PAD_LEFT)) {
        g_focus = (g_focus + STARTMENU_N - 1) % STARTMENU_N;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_A)) {
        return g_focus;
    }
    return -1;
}

const char *startmenu_script(int focus)
{
    if (focus == STARTMENU_NEW) {
        return "start-1";
    }
    if (focus == STARTMENU_LOAD) {
        return "start-2";
    }
    if (focus == STARTMENU_QUIT) {
        return "start-4";
    }
    return "";
}

void startmenu_slot_reset(void)
{
    g_slot = 1;
}

int startmenu_slot_focus(void)
{
    return g_slot;
}

int startmenu_slot_tick(uint32_t prev, uint32_t now)
{
    if (pad_just_pressed(prev, now, DINK_PAD_DOWN) ||
        pad_just_pressed(prev, now, DINK_PAD_RIGHT)) {
        g_slot = (g_slot >= 10) ? 0 : g_slot + 1;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_UP) ||
        pad_just_pressed(prev, now, DINK_PAD_LEFT)) {
        g_slot = (g_slot <= 0) ? 10 : g_slot - 1;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_B)) {
        return 0;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_A)) {
        return g_slot;
    }
    return -1;
}

void startpause_reset(void)
{
    g_pause_focus = STARTPAUSE_CONTINUE;
    g_pause_open = 0;
}

int startpause_focus(void)
{
    return g_pause_focus;
}

int startpause_open(void)
{
    return g_pause_open;
}

int startpause_eats_pad(uint32_t prev, uint32_t now)
{
    return g_pause_open || pad_just_pressed(prev, now, DINK_PAD_START);
}

int startpause_tick(uint32_t prev, uint32_t now)
{
    if (!g_pause_open) {
        if (pad_just_pressed(prev, now, DINK_PAD_START)) {
            g_pause_open = 1;
            g_pause_focus = STARTPAUSE_CONTINUE;
        }
        return -1;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_B) ||
        pad_just_pressed(prev, now, DINK_PAD_START)) {
        g_pause_open = 0;
        return -2;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_DOWN) ||
        pad_just_pressed(prev, now, DINK_PAD_UP)) {
        g_pause_focus = 1 - g_pause_focus;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_A)) {
        g_pause_open = 0;
        return g_pause_focus;
    }
    return -1;
}

#ifdef _arch_dreamcast
static void startmenu_fill_black(void)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    vert.flags = PVR_CMD_VERTEX;
    vert.argb = 0xff000000;
    vert.oargb = 0;
    vert.z = 1.0f;
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
    vert.y = 480.0f;
    pvr_prim(&vert, sizeof(vert));
}

static int load_seq_fr(struct SeqInfo *seqs, int seq, int frame,
                       struct SpriteFrame *out)
{
    if (seqs == NULL || seq < 1 || seq >= DINK_MAX_SEQ || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (sprite_load_seq_frame(&seqs[seq], seq, frame, out) != 0) {
        return -1;
    }
    return sprite_upload_pvr(out);
}

int startmenu_present_pvr(struct SeqInfo *seqs)
{
    struct SpriteFrame logo, btn[3][2], hover[3];
    static const int bseq[3] = {STARTMENU_SEQ_NEW, STARTMENU_SEQ_LOAD,
                                STARTMENU_SEQ_QUIT};
    static const int bx[3] = {76, 524, 560};
    static const int by[3] = {40, 40, 440};
    static const int hseq[3] = {199, 200, 198};
    static const int hx[3] = {204, 358, 446};
    static const int hy[3] = {86, 93, 417};
    int i, fr, pick = -1;
    uint32_t prev = 0;

    memset(&logo, 0, sizeof(logo));
    memset(btn, 0, sizeof(btn));
    memset(hover, 0, sizeof(hover));
    (void)tiles_pvr_ensure();
    (void)load_seq_fr(seqs, STARTMENU_SEQ_LOGO, 1, &logo);
    for (i = 0; i < 3; i++) {
        (void)load_seq_fr(seqs, bseq[i], 1, &btn[i][0]);
        if (load_seq_fr(seqs, bseq[i], 2, &btn[i][1]) != 0) {
            btn[i][1] = btn[i][0];
            btn[i][1].tex = btn[i][0].tex;
            btn[i][1].argb1555 = NULL;
        }
        (void)load_seq_fr(seqs, hseq[i], 1, &hover[i]);
    }
    startmenu_reset();
    printf("startmenu seq=%d buttons=%d,%d,%d\n", STARTMENU_SEQ_LOGO,
           STARTMENU_SEQ_NEW, STARTMENU_SEQ_LOAD, STARTMENU_SEQ_QUIT);
    while (pick < 0) {
        uint32_t buttons = 0;
        int have = (pad_poll_port0(&buttons) == 0);
        int foc;

        if (have) {
            pick = startmenu_tick(prev, buttons);
        }
        prev = have ? buttons : 0;
        foc = startmenu_focus();
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        startmenu_fill_black();
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_PT_POLY);
        sprite_draw_pvr(&logo, 320.0f, 240.0f, 2.0f);
        for (i = 0; i < 3; i++) {
            fr = (foc == i) ? 1 : 0;
            sprite_draw_pvr(&btn[i][fr], (float)bx[i], (float)by[i], 3.0f);
            if (foc == i) {
                sprite_draw_pvr(&hover[i], (float)hx[i], (float)hy[i], 3.2f);
            }
        }
        pvr_list_finish();
        pvr_scene_finish();
    }
    sprite_frame_free(&logo);
    for (i = 0; i < 3; i++) {
        sprite_frame_free(&btn[i][0]);
        if (btn[i][1].argb1555 != NULL ||
            (btn[i][1].tex != NULL && btn[i][1].tex != btn[i][0].tex)) {
            sprite_frame_free(&btn[i][1]);
        }
        sprite_frame_free(&hover[i]);
    }
    printf("startmenu click %s\n", startmenu_script(pick));
    return pick;
}

int startmenu_present_slots_pvr(void)
{
    int pick = -1;
    uint32_t prev = 0;
    char line[80];

    startmenu_slot_reset();
    while (pick < 0) {
        uint32_t buttons = 0;
        int have = (pad_poll_port0(&buttons) == 0);
        int foc;

        if (have) {
            pick = startmenu_slot_tick(prev, buttons);
        }
        prev = have ? buttons : 0;
        foc = startmenu_slot_focus();
        save_info_line(foc, line, sizeof(line));
        saybox_set(line, 0);
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        startmenu_fill_black();
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_PT_POLY);
        saybox_draw_pvr(3.0f);
        pvr_list_finish();
        pvr_scene_finish();
    }
    saybox_clear();
    return pick;
}
#endif
