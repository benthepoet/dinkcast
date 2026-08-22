/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startmenu.h"

#include "pad.h"

#include <string.h>

#ifdef _arch_dreamcast
#include "choice.h"
#include "dinkc_vm.h"
#include "ini.h"
#include "mem.h"
#include "save.h"
#include "saybox.h"
#include "sprite.h"
#include "tiles.h"

#include <kos.h>
#include <stdio.h>
#endif

struct StartHover {
    int seq;
    int x, y;
    int nframes;
    int delay_ms;
    int frame;
    int pframe;
    int reverse;
    int wait;
    int live;
    /* spr[].seq != 0. Forward end clears this (hold last pframe). */
    int anim;
};

static int g_focus;
static int g_pause_focus;
static int g_pause_open;
static int g_slot;
static struct StartHover g_hover[STARTMENU_N];

void startmenu_reset(void)
{
    g_focus = STARTMENU_NEW;
}

static int hover_ok(int i)
{
    return i >= 0 && i < STARTMENU_N;
}

void startmenu_hover_bind(int i, int seq, int x, int y, int nframes, int delay)
{
    if (!hover_ok(i)) {
        return;
    }
    memset(&g_hover[i], 0, sizeof(g_hover[i]));
    g_hover[i].seq = seq;
    g_hover[i].x = x;
    g_hover[i].y = y;
    g_hover[i].nframes = nframes < 1 ? 1 : nframes;
    g_hover[i].delay_ms = delay;
}

void startmenu_hover_on(int i)
{
    if (!hover_ok(i)) {
        return;
    }
    /* start-1.c buttonon: sp_reverse 0; sp_seq. */
    g_hover[i].reverse = 0;
    g_hover[i].frame = 0;
    g_hover[i].anim = 1;
    g_hover[i].live = 1;
}

void startmenu_hover_off(int i)
{
    if (!hover_ok(i) || !g_hover[i].live) {
        return;
    }
    /* start-1.c buttonoff: sp_reverse 1; sp_seq; brain 7. */
    g_hover[i].reverse = 1;
    g_hover[i].frame = 0;
    g_hover[i].anim = 1;
}

static int hover_delay(const struct StartHover *h, int frame)
{
    (void)frame;
    return h->delay_ms;
}

void startmenu_hover_tick(int now_ms)
{
    int i;

    for (i = 0; i < STARTMENU_N; i++) {
        struct StartHover *h = &g_hover[i];
        int n;

        if (!h->live) {
            continue;
        }
        n = h->nframes;
        if (!h->anim) {
            continue;
        }
        if (h->reverse) {
            if (h->frame < 1) {
                h->pframe = n;
                h->frame = n;
                h->wait = now_ms + hover_delay(h, n);
                continue;
            }
            if (now_ms <= h->wait) {
                continue;
            }
            h->frame--;
            h->wait = now_ms + hover_delay(h, h->frame);
            h->pframe = h->frame;
            if (h->frame < 1) {
                /* live_sprite_animate end + one_time_brain_for_real. */
                h->pframe = 1;
                h->frame = 0;
                h->anim = 0;
                h->live = 0;
            }
            continue;
        }
        if (h->frame < 1) {
            h->pframe = 1;
            h->frame = 1;
            h->wait = now_ms + hover_delay(h, 1);
            continue;
        }
        if (now_ms <= h->wait) {
            continue;
        }
        h->frame++;
        h->wait = now_ms + hover_delay(h, h->frame);
        h->pframe = h->frame;
        if (h->frame > n) {
            /* seq.frame[n+1] is 0: hold last, seq=0. Do not restart. */
            h->pframe = n;
            h->frame = 0;
            h->anim = 0;
        }
    }
}

int startmenu_hover_live(int i)
{
    return hover_ok(i) ? g_hover[i].live : 0;
}

int startmenu_hover_pframe(int i)
{
    return hover_ok(i) && g_hover[i].pframe > 0 ? g_hover[i].pframe : 1;
}

int startmenu_hover_x(int i)
{
    return hover_ok(i) ? g_hover[i].x : 0;
}

int startmenu_hover_y(int i)
{
    return hover_ok(i) ? g_hover[i].y : 0;
}

void startmenu_highlight_center(int w0, int h0, int cx0, int cy0, int w1,
                                int h1, int *cx1, int *cy1)
{
    if (cx1 != NULL) {
        *cx1 = cx0 + (w1 - w0) / 2;
    }
    if (cy1 != NULL) {
        *cy1 = cy0 + (h1 - h0) / 2;
    }
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
        g_slot = (g_slot >= STARTMENU_SLOT_N) ? 1 : g_slot + 1;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_UP) ||
        pad_just_pressed(prev, now, DINK_PAD_LEFT)) {
        g_slot = (g_slot <= 1) ? STARTMENU_SLOT_N : g_slot - 1;
    }
    if (pad_just_pressed(prev, now, DINK_PAD_B)) {
        return STARTMENU_SLOT_NEVERMIND;
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
    struct SpriteFrame logo, btn[STARTMENU_N][2];
    struct SpriteFrame hover[STARTMENU_N][16];
    static const int bseq[STARTMENU_N] = {STARTMENU_SEQ_NEW, STARTMENU_SEQ_LOAD};
    static const int bx[STARTMENU_N] = {76, 524};
    static const int by[STARTMENU_N] = {40, 40};
    static const int hseq[STARTMENU_N] = {STARTMENU_HOVER_NEW,
                                          STARTMENU_HOVER_LOAD};
    static const int hx[STARTMENU_N] = {STARTMENU_HOVER_NEW_X,
                                        STARTMENU_HOVER_LOAD_X};
    static const int hy[STARTMENU_N] = {STARTMENU_HOVER_NEW_Y,
                                        STARTMENU_HOVER_LOAD_Y};
    int i, f, nfr, fr, pick = -1, last = -1;
    uint32_t prev = 0;

    memset(&logo, 0, sizeof(logo));
    memset(btn, 0, sizeof(btn));
    memset(hover, 0, sizeof(hover));
    (void)tiles_pvr_ensure();
    (void)load_seq_fr(seqs, STARTMENU_SEQ_LOGO, 1, &logo);
    for (i = 0; i < STARTMENU_N; i++) {
        (void)load_seq_fr(seqs, bseq[i], 1, &btn[i][0]);
        if (load_seq_fr(seqs, bseq[i], 2, &btn[i][1]) != 0) {
            btn[i][1] = btn[i][0];
            btn[i][1].tex = btn[i][0].tex;
            btn[i][1].argb1555 = NULL;
        } else {
            startmenu_highlight_center(btn[i][0].w, btn[i][0].h, btn[i][0].cx,
                                       btn[i][0].cy, btn[i][1].w, btn[i][1].h,
                                       &btn[i][1].cx, &btn[i][1].cy);
        }
        nfr = 0;
        if (seqs != NULL && hseq[i] > 0 && hseq[i] < DINK_MAX_SEQ) {
            nfr = seqs[hseq[i]].nframes;
        }
        if (nfr < 1) {
            nfr = 15;
        }
        for (f = 1; f <= nfr && f <= 15; f++) {
            if (load_seq_fr(seqs, hseq[i], f, &hover[i][f]) != 0) {
                nfr = f - 1;
                break;
            }
        }
        if (nfr < 1) {
            nfr = 1;
        }
        /* BLACK/NOTANIM: later frames reuse frame 1 xoffset/yoffset. */
        for (f = 2; f <= nfr && f <= 15; f++) {
            hover[i][f].cx = hover[i][1].cx;
            hover[i][f].cy = hover[i][1].cy;
        }
        startmenu_hover_bind(i, hseq[i], hx[i], hy[i], nfr,
                             seqs != NULL ? seqs[hseq[i]].delay : 0);
    }
    startmenu_reset();
    startmenu_hover_on(STARTMENU_NEW);
    last = STARTMENU_NEW;
    printf("startmenu seq=%d buttons=%d,%d hover=%d,%d\n",
           STARTMENU_SEQ_LOGO, STARTMENU_SEQ_NEW, STARTMENU_SEQ_LOAD,
           STARTMENU_HOVER_NEW, STARTMENU_HOVER_LOAD);
    while (pick < 0) {
        uint32_t buttons = 0;
        int have = (pad_poll_port0(&buttons) == 0);
        int foc;

        if (have) {
            pick = startmenu_tick(prev, buttons);
        }
        prev = have ? buttons : 0;
        foc = startmenu_focus();
        if (foc != last) {
            startmenu_hover_off(last);
            startmenu_hover_on(foc);
            last = foc;
        }
        startmenu_hover_tick((int)mem_now_ms());
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        startmenu_fill_black();
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_PT_POLY);
        /* START.c / buttonon: sp_noclip 1. Logo is on-screen either way. */
        sprite_draw_pvr_noclip(&logo, 320.0f, 240.0f, 2.0f);
        for (i = 0; i < STARTMENU_N; i++) {
            fr = (foc == i) ? 1 : 0;
            sprite_draw_pvr_noclip(&btn[i][fr], (float)bx[i], (float)by[i],
                                   3.0f);
            if (startmenu_hover_live(i)) {
                int pf = startmenu_hover_pframe(i);

                if (pf >= 1 && pf <= 15 &&
                    (hover[i][pf].tex != NULL ||
                     hover[i][pf].argb1555 != NULL)) {
                    sprite_draw_pvr_noclip(&hover[i][pf],
                                           (float)startmenu_hover_x(i),
                                           (float)startmenu_hover_y(i), 3.2f);
                }
            }
        }
        pvr_list_finish();
        pvr_scene_finish();
    }
    sprite_frame_free(&logo);
    for (i = 0; i < STARTMENU_N; i++) {
        sprite_frame_free(&btn[i][0]);
        if (btn[i][1].argb1555 != NULL ||
            (btn[i][1].tex != NULL && btn[i][1].tex != btn[i][0].tex)) {
            sprite_frame_free(&btn[i][1]);
        }
        for (f = 1; f <= 15; f++) {
            if (hover[i][f].argb1555 != NULL || hover[i][f].tex != NULL) {
                sprite_frame_free(&hover[i][f]);
            }
        }
    }
    printf("startmenu click %s\n", startmenu_script(pick));
    return pick;
}

int startmenu_present_slots_pvr(struct SeqInfo *seqs)
{
    struct SpriteFrame logo, start, cont[2];
    struct SpriteFrame *cont_fr;
    int pick = -1;
    uint32_t prev = 0, held = 0;

    memset(&logo, 0, sizeof(logo));
    memset(&start, 0, sizeof(start));
    memset(cont, 0, sizeof(cont));
    (void)load_seq_fr(seqs, STARTMENU_SEQ_LOGO, 1, &logo);
    (void)load_seq_fr(seqs, STARTMENU_SEQ_NEW, 1, &start);
    (void)load_seq_fr(seqs, STARTMENU_SEQ_LOAD, 1, &cont[0]);
    if (load_seq_fr(seqs, STARTMENU_SEQ_LOAD, 2, &cont[1]) == 0) {
        startmenu_highlight_center(cont[0].w, cont[0].h, cont[0].cx, cont[0].cy,
                                   cont[1].w, cont[1].h, &cont[1].cx,
                                   &cont[1].cy);
        cont_fr = &cont[1];
    } else {
        cont_fr = &cont[0];
    }
    if (choice_frame(DINK_CHOICE_SEQ, 2) == NULL) {
        (void)choice_load(seqs);
        (void)choice_upload_pvr();
    }
    dinkc_vm_choice_open_saves();
    /* start-2 load() is a choice. Continue A is still down. */
    if (pad_poll_port0(&held) == 0) {
        prev = held;
    }
    printf("startmenu load choice n=%d\n", dinkc_vm_choice_n());
    while (pick < 0) {
        uint32_t buttons = 0;
        int have = (pad_poll_port0(&buttons) == 0);

        if (have) {
            if (pad_just_pressed(prev, buttons, DINK_PAD_DOWN) ||
                pad_just_pressed(prev, buttons, DINK_PAD_RIGHT)) {
                dinkc_vm_choice_move(1);
            }
            if (pad_just_pressed(prev, buttons, DINK_PAD_UP) ||
                pad_just_pressed(prev, buttons, DINK_PAD_LEFT)) {
                dinkc_vm_choice_move(-1);
            }
            if (pad_just_pressed(prev, buttons, DINK_PAD_B)) {
                pick = STARTMENU_SLOT_NEVERMIND;
            } else if (pad_just_pressed(prev, buttons, DINK_PAD_A)) {
                pick = dinkc_vm_choice_cur();
            }
        }
        prev = have ? buttons : 0;
        choice_tick((int)mem_now_ms());
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        startmenu_fill_black();
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_PT_POLY);
        sprite_draw_pvr_noclip(&logo, 320.0f, 240.0f, 2.0f);
        sprite_draw_pvr_noclip(&start, 76.0f, 40.0f, 3.0f);
        sprite_draw_pvr_noclip(cont_fr, 524.0f, 40.0f, 3.0f);
        saybox_draw_choices_pvr(3.4f);
        pvr_list_finish();
        pvr_scene_finish();
    }
    dinkc_vm_choice_close_saves();
    sprite_frame_free(&logo);
    sprite_frame_free(&start);
    sprite_frame_free(&cont[0]);
    if (cont[1].argb1555 != NULL ||
        (cont[1].tex != NULL && cont[1].tex != cont[0].tex)) {
        sprite_frame_free(&cont[1]);
    }
    printf("startmenu load pick %d\n", pick);
    return pick;
}
#endif
