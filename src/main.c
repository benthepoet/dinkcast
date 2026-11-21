/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Boot: 640×480. Probe dink.dat. Show official title still (Bite 3.4).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "boot.h"
#include "bgbake.h"
#include "brains.h"
#include "choice.h"
#include "dinkdat.h"
#include "fs.h"
#include "edraw.h"
#include "fade.h"
#include "font.h"
#include "hard.h"
#include "hit.h"
#include "ini.h"
#include "inv.h"
#include "mapscr.h"
#include "mem.h"
#include "pad.h"
#include "player.h"
#include "saybox.h"
#include "screen.h"
#include "script.h"
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "save.h"
#include "overlay.h"
#include "pspr.h"
#include "sprite.h"
#include "start_map.h"
#include "startmenu.h"
#include "status.h"
#include "talk.h"
#include "tiles.h"
#include "title.h"
#include "title_path.h"
#include "world.h"

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/biosfont.h>
#include <dc/cdrom.h>
#include <dirent.h>

/* KOS thread stack is ~32–64 KB. These sum > 32 KB if locals. */
static struct World g_world;
static struct MapScreen g_scr;
static struct EditorSprite *g_spr_ok;
/* The editor rows as loaded (map.dat + spmap): g_spr_ok is rebuilt from
 * this on every draw_screen, then background rows are baked out of it. */
static struct EditorSprite *g_spr_ed;
/* Baked background rows (bgbake.h): freed from the live table. */
static struct BgRow g_bg[100];
static int g_nbg;
static struct TileAtlas g_atlas;
static struct HardMap g_hard;
static struct EdGfx *g_edg;
static struct SeqInfo *g_seqs_play;
static int g_ned;
static struct Player *g_play_pl;
static struct HardMask *g_play_mask;
/* Dink frame from pspr_get; NULL until the first load succeeds. */
static struct SpriteFrame **g_play_dfr;
static int *g_play_map;
static int *g_play_last_seq;
static int *g_play_last_frame;
static int *g_play_have_scene;

static void preload_seq_cb(int seq)
{
    if (g_seqs_play == NULL || g_edg == NULL) {
        return;
    }
    /* Pack is pinned. Frame 1 only — s1-bul 401–409 all-frames overflowed
     * cpu_pixels and play-path evicted live Screen every tick. */
    edraw_load_frame(g_edg, &g_ned, g_seqs_play, seq, 1);
}

static void load_frame_cb(int seq, int frame)
{
    if (g_seqs_play == NULL || g_edg == NULL) {
        return;
    }
    edraw_load_frame(g_edg, &g_ned, g_seqs_play, seq, frame);
}

/* pspr relief: Dink's frame needs VRAM held by non-live screen sprites.
 * Every pspr_get runs after the last scene finished. */
static size_t dink_vram_room(size_t need)
{
    if (g_seqs_play == NULL || g_edg == NULL) {
        return 0;
    }
    return edraw_vram_room(g_edg, &g_ned, g_seqs_play, need);
}

/* Overlay VRAM (choice box ~0.7 MB, inventory menu ~1.1 MB) only while
 * shown (overlay.c). Hooks make room like edraw (#155). */
static struct SeqInfo *g_overlay_seqs;

static size_t ov_screen_evictable(void)
{
    return g_overlay_seqs != NULL && g_edg != NULL
               ? edraw_vram_evictable(g_edg, g_ned, g_overlay_seqs)
               : 0u;
}

static size_t ov_screen_free_one(void)
{
    return g_overlay_seqs != NULL && g_edg != NULL
               ? edraw_vram_free_one(g_edg, &g_ned, g_overlay_seqs)
               : 0u;
}

static const struct OverlayOps g_choice_ops = {
    choice_box_load, choice_box_release, choice_box_nframes,
    choice_box_frame,
};
static const struct OverlayOps g_inv_ops = {
    inv_menu_load, inv_menu_release, inv_menu_nframes, inv_menu_slot,
};
static struct Overlay g_ov_choice = {"choice", &g_choice_ops, OVERLAY_OFF};
static struct Overlay g_ov_inv = {"inv", &g_inv_ops, OVERLAY_OFF};

/* Before pvr_scene_begin: the last scene finished at the top of the tick. */
static void overlay_sync(struct SeqInfo *seqs)
{
#ifdef _arch_dreamcast
    int c0 = g_ov_choice.state, i0 = g_ov_inv.state;

    g_overlay_seqs = seqs;
    overlay_sync_one(&g_ov_choice, dinkc_vm_choice_n() > 0);
    overlay_sync_one(&g_ov_inv, inv_showing());
    if (g_ov_choice.state != c0 || g_ov_inv.state != i0) {
        mem_vram_note("overlay");
    }
#else
    (void)seqs;
#endif
}

static void inv_show_cmd(int on)
{
    if (on) {
        inv_open(0);
    } else {
        inv_close();
    }
}

static void give_start_fists(void)
{
    int args[8];

    memset(args, 0, sizeof(args));
    args[1] = 438;
    args[2] = 1;
    (void)dinkc_cmd("add_item", args, 3, "item-fst", NULL, NULL, NULL);
    dinkc_var_set("&cur_weapon", 1, DINKC_GLOBAL_SCOPE, 1);
    (void)dinkc_cmd("arm_weapon", NULL, 0, NULL, NULL, NULL, NULL);
    dinkc_var_set("&update_status", 1, DINKC_GLOBAL_SCOPE, 1);
    status_draw_all();
}

static int spr_ok_ready(void)
{
    if (g_spr_ok != NULL) {
        return 0;
    }
    g_spr_ok = (struct EditorSprite *)malloc(101u * sizeof(*g_spr_ok));
    g_spr_ed = (struct EditorSprite *)malloc(101u * sizeof(*g_spr_ed));
    return g_spr_ok == NULL || g_spr_ed == NULL ? -1 : 0;
}

static void spr_snap(const char *tag)
{
    if (g_spr_ok == NULL) {
        return;
    }
    memcpy(g_spr_ok, g_scr.sprite, 101u * sizeof(*g_spr_ok));
    if (g_spr_ed != NULL) {
        memcpy(g_spr_ed, g_scr.sprite, 101u * sizeof(*g_spr_ed));
    }
    g_nbg = 0;
    printf("snap %s sprite1 seq=%d y=%d act=%d\n", tag,
           (int)g_spr_ok[1].seq, (int)g_spr_ok[1].y, (int)g_spr_ok[1].active);
}

static void spr_restore(const char *tag)
{
    if (g_spr_ok == NULL) {
        return;
    }
    memcpy(g_scr.sprite, g_spr_ok, 101u * sizeof(*g_spr_ok));
    printf("restore %s sprite1 seq=%d y=%d act=%d script=%s\n", tag,
           (int)g_scr.sprite[1].seq, (int)g_scr.sprite[1].y,
           (int)g_scr.sprite[1].active, g_scr.sprite[26].script);
}

/* draw_screen start: back to the editor rows (all visions, background
 * rows included) so frames, hardness and the bake see the whole screen. */
static void spr_unbake(const char *tag)
{
    if (g_spr_ok == NULL || g_spr_ed == NULL) {
        return;
    }
    memcpy(g_spr_ok, g_spr_ed, 101u * sizeof(*g_spr_ok));
    g_nbg = 0;
    spr_restore(tag);
}

/* After hardness is stamped and frames are loaded, before brains_enter:
 * move this vision's non-warp background rows out of the live table. */
static void spr_bake(int vision)
{
    int i, nfree = 0;

    if (g_spr_ok == NULL || g_spr_ed == NULL) {
        return;
    }
    memcpy(g_spr_ok, g_spr_ed, 101u * sizeof(*g_spr_ok));
    g_nbg = bg_bake(g_spr_ok, vision, g_bg, (int)(sizeof(g_bg) / sizeof(g_bg[0])));
    for (i = 2; i <= 99; i++) {
        nfree += !g_spr_ok[i].active;
    }
    memcpy(g_scr.sprite, g_spr_ok, 101u * sizeof(*g_spr_ok));
    printf("bake bg=%d free_slots=%d\n", g_nbg, nfree);
}

static void on_blood(int slot)
{
    int x, y;

    if (brains_live_xy(slot, &x, &y)) {
        brains_random_blood(x, y - 40, slot);
    }
}

/* FreeDink human_brain draw_damage(1): Dink's frame is the one drawn last
 * (getpic: pseq/pframe), held by pspr in *g_play_dfr, not the editor cache.
 * *g_play_dfr must always be pspr's pinned g_last slot (never evicted). The
 * floater's owner is 1 in FreeDink numbering (Dink), not g_b[1]; nothing
 * reads brain_parm on text sprites. */
static void on_dink_damage(const struct Player *p, int damage)
{
    const struct SpriteFrame *f =
        g_play_dfr != NULL ? *g_play_dfr : NULL;

    if (p == NULL) {
        return;
    }
    if (f != NULL && f->w > 0 && f->h > 0) {
        brains_draw_damage_box(p->x, p->y, f->w, f->h, f->cx, f->cy, damage,
                               1);
    } else {
        brains_draw_damage_box(p->x, p->y, 0, 0, 0, 0, damage, 1);
    }
}

/* FreeDink draw_damage reads k[].box (the bitmap) and xoffset/yoffset. */
static int play_frame_box(int seq, int frame, int *w, int *h, int *cx, int *cy)
{
    struct SpriteFrame *ef;

    if (w == NULL || h == NULL || cx == NULL || cy == NULL) {
        return 0;
    }
    ef = edraw_find(g_edg, g_ned, seq, frame);
    if (ef == NULL || ef->w < 1 || ef->h < 1) {
        return 0;
    }
    *w = ef->w;
    *h = ef->h;
    *cx = ef->cx;
    *cy = ef->cy;
    return 1;
}

/* Stamp one editor row's hardness (FreeDink add_hardness(sprite, 100 + j)
 * for warps, 1 otherwise). es may be a live row or a baked background row. */
static void stamp_editor_row(struct HardMask *mask, struct SeqInfo *seqs,
                             const struct EditorSprite *es, int si)
{
    struct SpriteFrame *ef;
    int hl, ht, hr, hb, cx, cy, hid, seq, fr, type;

    seq = (int)es->seq;
    fr = (int)es->frame < 1 ? 1 : (int)es->frame;
    if (seq < 1 || seq >= DINK_MAX_SEQ) {
        return;
    }
    type = (int)es->type;
    hid = es->is_warp ? 100 + si : 1;
    ef = edraw_find(g_edg, g_ned, seq, fr);
    if (ef != NULL) {
        hard_stamp_editor(mask, (int)es->x, (int)es->y, type, hid, 1, ef->hl,
                          ef->ht, ef->hr, ef->hb);
        return;
    }
    /* Type 0/1: no invisible furniture. Type 2 still uses SET_SPRITE_INFO. */
    if (seqs == NULL || !hard_stamp_without_pixels(type)) {
        return;
    }
    ini_frame_geom(&seqs[seq], seq, fr, 50, 50, &cx, &cy, &hl, &ht, &hr, &hb);
    hard_stamp_editor(mask, (int)es->x, (int)es->y, type, hid, 0, hl, ht, hr,
                      hb);
}

static void stamp_one_editor(struct HardMask *mask, struct SeqInfo *seqs, int si)
{
    stamp_editor_row(mask, seqs, &g_scr.sprite[si], si);
}

static void stamp_editor_hard(struct HardMask *mask, struct SeqInfo *seqs)
{
    int si;

    /* FreeDink dc_draw_hard_map / draw_hard_sprite: update_play_changes,
     * fill_whole_hard, fill_hard_sprites, fill_back_sprites. */
    dinkc_cmd_apply_spmap(&g_scr, dinkc_var_get("&player_map",
                                                DINKC_GLOBAL_SCOPE, 1));
    if (mask == NULL || hard_stamp_tiles(&g_hard, &g_scr, mask) != 0) {
        return;
    }
    for (si = 1; si <= 100; si++) {
        if (!editor_sprite_on_vision(&g_scr.sprite[si],
                                     script_play_vision()) ||
            g_scr.sprite[si].hard != 0 || brains_slot_hard(si) != 0) {
            continue;
        }
        if (!hard_stamp_editor_slot((int)g_scr.sprite[si].type,
                                    brains_slot_live(si))) {
            continue;
        }
        stamp_one_editor(mask, seqs, si);
    }
    /* Baked background rows (fill_back_sprites): hard == 0 only. */
    for (si = 0; si < g_nbg; si++) {
        if (g_bg[si].es.hard != 0) {
            continue;
        }
        stamp_editor_row(mask, seqs, &g_bg[si].es, g_bg[si].ed);
    }
}

/* 14.4c: Screen live is this tick's editor + created draw frames. Unused
 * fire/explo frames become evictable before create_sprite frame-1. */
static void edraw_live_touch_loop(struct SeqInfo *seqs, int ned, int sq, int fr)
{
    int nxt;

    if (fr < 1) {
        fr = 1;
    }
    edraw_live_touch(g_edg, ned, sq, fr);
    nxt = edraw_loop_next_frame(seqs, sq, fr);
    if (nxt != fr) {
        edraw_live_touch(g_edg, ned, sq, nxt);
    }
}

static void edraw_ensure_draw_frame(struct SeqInfo *seqs, int sq, int fr,
                                    int looping)
{
    int nxt;

    if (fr < 1) {
        fr = 1;
    }
    if (edraw_find(g_edg, g_ned, sq, fr) == NULL) {
        (void)edraw_ensure_frame(g_edg, &g_ned, seqs, sq, fr);
    }
    if (!looping) {
        return;
    }
    nxt = edraw_loop_next_frame(seqs, sq, fr);
    if (nxt != fr && edraw_find(g_edg, g_ned, sq, nxt) == NULL) {
        (void)edraw_ensure_frame(g_edg, &g_ned, seqs, sq, nxt);
    }
}

/* Start of this play tick (after the scene wait), for the lookahead
 * budget. */
static unsigned g_tick_t0;

/* Option (b), 2026-11-20: load the next one or two frames of live
 * animations during the tick's spare time, so a frame is resident before
 * it is first drawn (tree fire, explosions, attacks, Dink). Runs last in
 * the tick (after Dink's load and overlays, before the scene), only while
 * the tick is young (DINK_LOOKAHEAD_US since the scene wait), at most
 * DINK_LOOKAHEAD_MAX loads; never evicts and keeps a VRAM margin after
 * the load (edraw_prefetch / pspr_prefetch). Round-robin over sprites, +1
 * before +2. Same work as loading at first draw, spread over idle ticks. */
#define DINK_LOOKAHEAD_US 6000u
#define DINK_LOOKAHEAD_MAX 2

/* Dink loops walk and idle; punch, push, magic, death are one-shots. */
static int dink_seq_loops(const struct Player *pl, int sq)
{
    int bi = pl->base_idle > 0 ? pl->base_idle : DINK_BASE_IDLE;

    return (sq > DINK_BASE_WALK && sq < DINK_BASE_WALK + 10) ||
           (sq > bi && sq < bi + 10);
}

static void anim_lookahead(struct SeqInfo *seqs, const struct Player *pl)
{
    static int rr;
    int ahead, k, loaded = 0, edraw_ok, dink_ok;

    if (seqs == NULL || g_edg == NULL ||
        dink_io_now_us() - g_tick_t0 >= DINK_LOOKAHEAD_US) {
        return;
    }
    /* Once per tick: is there spare room at all? */
    edraw_ok = edraw_prefetch_room(g_edg, g_ned);
    dink_ok = pl != NULL && pl->seq > 0; /* seq 0: a held picture */
    if (!edraw_ok && !dink_ok) {
        return;
    }
    for (ahead = 1; ahead <= 2; ahead++) {
        /* Dink first: his frame is on screen every tick. */
        if (dink_ok) {
            int dsq = pl->seq, nf;

            nf = edraw_anim_ahead(seqs, dsq, player_pic_frame(pl), ahead,
                                  dink_seq_loops(pl, dsq));
            if (nf > 0 && dink_io_now_us() - g_tick_t0 < DINK_LOOKAHEAD_US) {
                unsigned t0 = dink_io_now_us();

                if (pspr_prefetch(seqs, dsq, nf) > 0) {
                    dink_io_note_frame("prefetch-dink", dsq, nf,
                                       dink_io_now_us() - t0, 0);
                    if (++loaded >= DINK_LOOKAHEAD_MAX) {
                        return;
                    }
                }
            }
        }
        for (k = 0; edraw_ok && k < 99; k++) {
            int ei = 1 + (rr + k) % 99, sq, fr, nf;

            if (!brains_slot_animating(ei) ||
                !brains_seq_frame(ei, &sq, &fr)) {
                continue;
            }
            nf = edraw_anim_ahead(seqs, sq, fr, ahead,
                                  brains_slot_brain(ei) == 6);
            if (nf <= 0) {
                continue;
            }
            if (dink_io_now_us() - g_tick_t0 >= DINK_LOOKAHEAD_US) {
                rr = (rr + k) % 99;
                return;
            }
            if (edraw_prefetch(g_edg, &g_ned, seqs, sq, nf) > 0 &&
                ++loaded >= DINK_LOOKAHEAD_MAX) {
                rr = (rr + k + 1) % 99;
                return;
            }
        }
    }
    rr = (rr + 1) % 99;
}

static void edraw_live_this_draw(struct SeqInfo *seqs, int ned)
{
    int ei, sq, fr;

    if (seqs == NULL || g_edg == NULL) {
        return;
    }
    edraw_live_begin(g_edg, ned, seqs);
    for (ei = 1; ei <= 100; ei++) {
        sq = (int)g_scr.sprite[ei].seq;
        fr = (int)g_scr.sprite[ei].frame;
        if (!editor_sprite_draw(&g_scr.sprite[ei], script_play_vision())) {
            continue;
        }
        /* Brain 6 draw uses brains_apply pframe, not the editor snapshot. */
        if ((int)g_scr.sprite[ei].brain == 6 && brains_slot_live(ei)) {
            continue;
        }
        if (fr < 1) {
            fr = 1;
        }
        edraw_live_touch(g_edg, ned, sq, fr);
    }
    for (ei = 1; ei <= 99; ei++) {
        if (!brains_seq_frame(ei, &sq, &fr)) {
            continue;
        }
        if (fr < 1) {
            fr = 1;
        }
        if (brains_slot_brain(ei) == 6) {
            edraw_live_touch_loop(seqs, ned, sq, fr);
        } else {
            edraw_live_touch(g_edg, ned, sq, fr);
        }
    }
}

static void edraw_created_sprites(struct SeqInfo *seqs, int *ned)
{
    int i, seq, bw, d, br, k;
    static const int walkd[4] = {1, 3, 7, 9};
    static const int duckd[6] = {1, 3, 4, 6, 7, 9};

    if (seqs == NULL || ned == NULL || g_edg == NULL) {
        return;
    }
    edraw_live_this_draw(seqs, *ned);
    /* Combat death pixels first (same class order as edraw_load_screen). */
    for (i = 2; i <= 99; i++) {
        if (!brains_slot_created(i) || brains_slot_brain(i) != 3) {
            continue;
        }
        for (k = 0; k < 6; k++) {
            edraw_load_seq(g_edg, ned, seqs, 110 + duckd[k]);
            edraw_load_seq(g_edg, ned, seqs, 120 + duckd[k]);
        }
    }
    for (i = 2; i <= 99; i++) {
        if (!brains_slot_created(i)) {
            continue;
        }
        seq = brains_slot_pseq(i);
        br = brains_slot_brain(i);
        {
            int fr = 1;

            if (brains_seq_frame(i, &seq, &fr)) {
                if (fr < 1) {
                    fr = 1;
                }
            } else {
                seq = brains_slot_pseq(i);
            }
            if (seq > 0 && br != 3) {
                edraw_load_frame(g_edg, ned, seqs, seq, fr);
            }
        }
        bw = brains_slot_base_walk(i);
        if (bw <= 0) {
            continue;
        }
        if (br == 3) {
            for (k = 0; k < 6; k++) {
                edraw_load_seq(g_edg, ned, seqs, bw + duckd[k]);
            }
        } else {
            /* Pill/pig/dragon created walks: frame 1. Same class as people. */
            for (d = 0; d < 4; d++) {
                edraw_load_frame(g_edg, ned, seqs, bw + walkd[d], 1);
            }
        }
    }
}

static void edraw_mark_created(void)
{
    int i, d;
    static const int walkd[4] = {1, 3, 7, 9};

    for (i = 2; i <= 99; i++) {
        int sq, fr, bw;

        if (!brains_slot_created(i)) {
            continue;
        }
        if (brains_seq_frame(i, &sq, &fr)) {
            edraw_mark_need(sq, fr < 1 ? 1 : fr);
        } else {
            sq = brains_slot_pseq(i);
            if (sq >= 1) {
                edraw_mark_need(sq, 1);
            }
        }
        bw = brains_slot_base_walk(i);
        if (bw >= 1) {
            for (d = 0; d < 4; d++) {
                edraw_mark_need(bw + walkd[d], 1);
            }
        }
    }
}

/* FreeDink game_load_screen: record + thaw + screenlock=0. No kill_all. */
static int g_io_map;

static int play_load_screen(int map)
{
    int rec, rc = -1;
    /* Script load_screen mid-tick returns to play for the rest of it. */
    int io_prev = dink_io_play();

    /* Diagnostic: close the last screen's play-path I/O tally. */
    dink_io_summary(g_io_map);
    g_io_map = map;
    dink_io_set_play(0);
    audio_music_disc_hold(1);
    rec = screen_map_rec(&g_world, map);
    if (rec < 1) {
        printf("load_screen skip map %d loc %d\n", map, rec);
        goto done;
    }
    if (g_play_map != NULL) {
        *g_play_map = map;
    }
    if (g_play_pl != NULL) {
        g_play_pl->freeze = 0;
        g_play_pl->move_active = 0;
        g_play_pl->move_nohard = 0;
    }
    screen_lock_set(0);
    printf("load_screen map %d loc %d\n", map, rec);
    if (map_load_record(rec, &g_scr) != 0) {
        printf("load_screen fail %d\n", rec);
        goto done;
    }
    dinkc_cmd_apply_spmap(&g_scr, map);
    spr_snap("load_screen");
    dinkc_var_set("&player_map", map, DINKC_GLOBAL_SCOPE, 1);
    dinkc_cmd_note_map(map, (int)g_world.indoor[map]);
    /* FreeDink game_load_screen → check_midi. Same name = no reopen. */
    printf("load_screen midi %d\n", (int)g_world.music[map]);
    (void)audio_music_map((int)g_world.music[map]);
    if (!g_hard.ready) {
        printf("load_screen hard load\n");
        if (hard_load(&g_hard) != 0) {
            printf("load_screen hard fail\n");
        }
    }
    /* Prev pack fclose (498) then SEEK_SET this FILE* wedged shop 35. */
    hard_fp_release();
    dink_cd_settle();
    printf("load_screen stamp map %d\n", map);
    if (g_play_mask != NULL &&
        hard_stamp_tiles(&g_hard, &g_scr, g_play_mask) != 0) {
        printf("load_screen stamp fail\n");
    }
    printf("load_screen ready map %d\n", map);
    rc = 0;
done:
    audio_music_disc_hold(0);
    dink_io_set_play(io_prev);
    return rc;
}

/* FreeDink fill_whole_hard: tiles only, drops live sprite hardness. */
static void play_fill_hard(void)
{
    if (g_play_mask == NULL) {
        return;
    }
    if (hard_stamp_tiles(&g_hard, &g_scr, g_play_mask) != 0) {
        printf("force_vision fill_hard fail\n");
    }
}

/* FreeDink draw_screen_game: kill_all except 1000, tiles, MAIN, place. */
static int play_draw_screen(int sprite)
{
    struct SeqInfo *seqs = g_seqs_play;
    int nstamp;
    int io_prev = dink_io_play();

    (void)sprite;
    dink_io_set_play(0);
    audio_music_disc_hold(1);
    audio_music_pump();
    if (g_play_have_scene != NULL && *g_play_have_scene) {
        pvr_wait_ready();
        *g_play_have_scene = 0;
    }
    dinkc_vm_kill_all();
    dinkc_cmd_thaw_if_idle();
    saybox_clear();
    spr_unbake("draw_screen");
    script_bind_screen(&g_scr);
    script_bind_note_script(brains_set_script);
    if (g_play_pl != NULL) {
        saybox_bind(&g_scr, g_play_pl);
    }
    brains_bind_screen(&g_scr);
    brains_reset();
    script_enter_vision();
    if (seqs != NULL) {
        edraw_mark_created();
        (void)edraw_load_screen(g_spr_ok, seqs, g_edg, &g_ned,
                                script_play_vision());
    }
    spr_restore("draw-post-edraw");
    /* (g_spr_ok is unbaked here: background frames load and stamp.) */
    printf("edraw unique %d\n", g_ned);
    if (g_play_mask != NULL) {
        for (nstamp = 1; nstamp <= 100; nstamp++) {
            if (!editor_sprite_on_vision(&g_scr.sprite[nstamp],
                                         script_play_vision()) ||
                g_scr.sprite[nstamp].hard != 0) {
                continue;
            }
            stamp_one_editor(g_play_mask, seqs, nstamp);
        }
    }
    dink_cd_settle();
    /* GPU idle (waited above). Dink's set survives the swap; take only
     * the shortfall the new screen's tiles and sprites need (LRU), so a
     * swap does not throw away the combat set. */
    {
        /* The old atlas is freed inside tiles_upload_pvr before the new
         * one is allocated: count it only if none is held. */
        size_t need = (tiles_tex_held() ? 0u : tiles_atlas_tex_bytes()) +
                      edraw_upload_need(g_edg, g_ned);
        size_t avail = sprite_vram_avail();
        size_t freed = avail < need ? pspr_trim_for(need - avail) : 0u;

        if (freed > 0) {
            printf("pspr trim %u for swap\n", (unsigned)freed);
        }
    }
    {
        struct TileAtlas nxt;

        memset(&nxt, 0, sizeof(nxt));
        if (tiles_build_atlas(&g_scr, &nxt) == 0) {
            tiles_free(&g_atlas);
            g_atlas = nxt;
            printf("swap atlas ok\n");
            audio_music_pump();
            if (tiles_upload_pvr(&g_atlas) != 0) {
                /* Fragmented: total free covered it, no block did. */
                printf("pspr trim %u for tiles\n", (unsigned)pspr_trim());
                if (tiles_upload_pvr(&g_atlas) != 0) {
                    printf("draw_screen tiles upload fail\n");
                }
            }
        } else {
            printf("draw_screen atlas fail keep\n");
        }
    }
    audio_music_pump();
    if (edraw_upload_pvr(g_edg, g_ned) != 0) {
        printf("draw_screen edraw upload fail\n");
    }
    if (seqs != NULL && g_play_pl != NULL && g_play_dfr != NULL) {
        /* Dink frames are kept across screens (pspr); a hit reads nothing. */
        struct SpriteFrame *f = pspr_get(seqs, player_pic_seq(g_play_pl),
                                         player_pic_frame(g_play_pl));

        if (f != NULL) {
            *g_play_dfr = f;
        }
        if (g_play_last_seq != NULL) {
            *g_play_last_seq = player_pic_seq(g_play_pl);
        }
        if (g_play_last_frame != NULL) {
            *g_play_last_frame = player_pic_frame(g_play_pl);
        }
        printf("draw_screen dink seq=%d pspr n=%d bytes=%u misses=%u\n",
               player_pic_seq(g_play_pl), pspr_count(),
               (unsigned)pspr_bytes(), pspr_misses());
    }
    spr_bake(script_play_vision());
    brains_enter(&g_scr, script_play_vision());
    brains_apply(&g_scr);
    if (seqs != NULL) {
        edraw_created_sprites(seqs, &g_ned);
        if (edraw_upload_pvr(g_edg, g_ned) != 0) {
            printf("draw_screen created upload fail\n");
        }
    }
    audio_music_pump();
    script_attach_live();
    audio_music_disc_hold(0);
    /* Script draw_screen mid-tick: the fiber's work after it is play. */
    dink_io_set_play(io_prev);
    return 0;
}

static void hud(const char *line0, const char *line1, const char *line2)
{
    fflush(stdout); /* fatal paths halt next: nothing left in a buffer */
    /* 12×24 BIOS font; opaque so it is readable on red/brown. */
    if (vram_s == NULL) {
        return;
    }
    if (line0 != NULL) {
        bfont_draw_str(vram_s + 24 * 640 + 16, 640, 1, line0);
    }
    if (line1 != NULL) {
        bfont_draw_str(vram_s + 56 * 640 + 16, 640, 1, line1);
    }
    if (line2 != NULL) {
        bfont_draw_str(vram_s + 88 * 640 + 16, 640, 1, line2);
    }
}

static void wait_cd(void)
{
    int st = -1, ty = -1, i;

    for (i = 0; i < 400; i++) {
        if (cdrom_get_status(&st, &ty) == ERR_OK) {
            if (st == CD_STATUS_PAUSED || st == CD_STATUS_STANDBY ||
                st == CD_STATUS_PLAYING) {
                return;
            }
        }
        thd_sleep(20);
    }
}

static void list_cd_hud(void)
{
    DIR *d;
    struct dirent *de;
    char buf[80];
    int n = 0, y = 120;

    d = opendir("/cd");
    if (d == NULL) {
        hud(NULL, NULL, "/cd opendir FAIL");
        return;
    }
    while ((de = readdir(d)) != NULL && n < 8) {
        snprintf(buf, sizeof(buf), "cd:%s", de->d_name);
        if (vram_s != NULL) {
            bfont_draw_str(vram_s + (y + n * 24) * 640 + 16, 640, 1, buf);
        }
        printf("%s\n", buf);
        n++;
    }
    closedir(d);
    if (n == 0 && vram_s != NULL) {
        bfont_draw_str(vram_s + 120 * 640 + 16, 640, 1, "/cd empty");
    }
}

static int g_need_restart;
static int g_need_title;
static int g_show_splash;

static void game_restart_cmd(void)
{
    g_need_restart = 1;
}

static void game_kill_cmd(void)
{
    g_need_title = 1;
}

static void title_show_splash(void)
{
    /* Do not call title_present_pvr here: it pvr_shutdown() and leaves
     * tiles g_pvr_ready set, so tiles_pvr_ensure is a no-op on a dead
     * PVR and wipes saybox/font/inv uploads. Quit/Title return to the
     * START menu (fill_screen 0), not a second Splash.bmp present. */
    tiles_draw_clear_pvr(0xff000000);
}

/* START.c picture + start-1/2 click. Not START.c main() (audio). */
static int title_pick_and_apply(struct SeqInfo *seqs, struct Player *pl,
                                int *player_map)
{
    for (;;) {
        int pick, slot;

        if (g_show_splash) {
            title_show_splash();
            g_show_splash = 0;
        }
        mem_vram_note("pre-startmenu");
        pick = startmenu_present_pvr(seqs);
        mem_vram_note("startmenu");
        if (pick == STARTMENU_QUIT) {
            printf("start-4 click kill_game\n");
            g_show_splash = 1;
            continue;
        }
        if (pick == STARTMENU_LOAD) {
            int args[8];
            int ret = 0;

            slot = startmenu_present_slots_pvr(seqs);
            if (slot < 1 || slot > 10) {
                continue;
            }
            if (save_game_exist(slot) == 0) {
                int yld = 0;

                /* start-2.c click(): Say_xy if &result != 11. */
                printf("start-2 load empty slot=%d\n", slot);
                memset(args, 0, sizeof(args));
                args[0] = 0;
                args[1] = 390;
                (void)dinkc_cmd(
                    "say_xy", args, 2,
                    "`%Try loading a saved game that exists, friend.", NULL,
                    &yld, &ret);
                continue;
            }
            dinkc_vm_reset();
            memset(args, 0, sizeof(args));
            args[0] = slot;
            (void)dinkc_cmd("load_game", args, 1, NULL, NULL, NULL, &ret);
            if (ret != 1) {
                printf("start-2 load_game fail slot=%d\n", slot);
                continue;
            }
            if (player_map != NULL) {
                *player_map = dinkc_var_get("&player_map", DINKC_GLOBAL_SCOPE, 1);
                if (*player_map < 1) {
                    *player_map = DINK_START_PLAYER_MAP;
                }
            }
            printf("start-2 load slot=%d map=%d\n", slot,
                   player_map != NULL ? *player_map : 0);
            return STARTMENU_LOAD;
        }
        dinkc_vm_reset();
        dinkc_var_init();
        dinkc_cmd_reset_inv();
        inv_reset();
        player_init(pl);
        dinkc_cmd_bind_player(pl);
        hit_bind_player(pl);
        script_clear_dink_die();
        if (player_map != NULL) {
            *player_map = DINK_START_PLAYER_MAP;
        }
        give_start_fists();
        printf("start-1 click new game\n");
        return STARTMENU_NEW;
    }
}

static void game_add_exp(int num)
{
    int exp = dinkc_var_get("&exp", DINKC_GLOBAL_SCOPE, 1) + num;

    if (exp > 99999) {
        exp = 99999;
    }
    dinkc_var_set("&exp", exp, DINKC_GLOBAL_SCOPE, 1);
}

/* FreeDink spr[].active for kill_scripts_with_inactive_sprites: Dink (1)
 * is always active; brain slots 2..100 are live or not. Ids 101..999 are
 * treated as alive (FreeDink would sweep them; Dinkcast has no sprites
 * there, so only a script on a bogus id is spared). 1000 is exempt. */
static int script_sprite_alive(int sprite)
{
    if (sprite == 1 || sprite > 100) {
        return 1;
    }
    return brains_slot_live(sprite);
}

int main(int argc, char **argv)
{
    struct TitleStill title;
    int64_t dat_sz = -1;
    char msg[80];

    (void)argc;
    (void)argv;

    vid_set_mode(DM_640x480, PM_RGB565);
    vid_clear(DINK_BOOT_R, DINK_BOOT_G, DINK_BOOT_B);
    hud(DINK_BOOT_MSG, "waiting for GD-ROM...", NULL);
    printf("%s\n", DINK_BOOT_MSG);
    vid_waitvbl();

    wait_cd();
    hud(DINK_BOOT_MSG, "probing /cd ...", NULL);

    if (dink_fs_init() != 0) {
        printf("dink_fs_root unresolved\n");
        vid_clear(0xC0, 0x00, 0x00);
        hud("NO DATA ROOT", "need /cd/dink or /cd/DINK", NULL);
        list_cd_hud();
        for (;;) {
            vid_waitvbl();
        }
    }
    snprintf(msg, sizeof(msg), "root %s", dink_fs_root());
    printf("dink_fs_root %s\n", dink_fs_root());
    (void)audio_init();
    save_set_pump(audio_music_poll);

    if (dink_dat_size(&dat_sz) != 0) {
        vid_clear(0xC0, 0x00, 0x80);
        hud("NO dink.dat", msg, NULL);
        list_cd_hud();
        for (;;) {
            vid_waitvbl();
        }
    }
    printf("found dink.dat %lld bytes\n", (long long)dat_sz);

    memset(&title, 0, sizeof(title));
    if (title_load(&title) != 0) {
        printf("title load failed\n");
        vid_clear(DINK_BOOT_R, DINK_BOOT_G, DINK_BOOT_B);
        hud("TITLE LOAD FAIL", DINK_TITLE_REL, msg);
        for (;;) {
            vid_waitvbl();
        }
    }
    printf("title %dx%d\n", title.w, title.h);
    if (title_present_pvr(&title) != 0) {
        title_free(&title);
        vid_clear(DINK_BOOT_R, DINK_BOOT_G, DINK_BOOT_B);
        hud("TITLE PRESENT FAIL", DINK_TITLE_REL, msg);
        for (;;) {
            vid_waitvbl();
        }
    }
    title_free(&title);
    /* Splash frame stays on PVR while we load. No pad. Then START. */
    {
        enum GameState st = GAME_STATE_LOADING;

        (void)st;
        printf("leave_title\n");
        mem_vram_note("leave_title");
        {
            /* PAL8 sprites: one main palette (bank 0) for every colour-keyed
             * frame on it; others stay ARGB1555. */
            static uint8_t pal[768];

            if (sprite_load_main_palette(pal) == 0) {
                sprite_set_main_palette(pal);
                tiles_cache_clear(); /* reload sheets as PAL8 */
                (void)tiles_pvr_ensure();
                sprite_pal_init_pvr();
                printf("pal8 on (Tiles/Esplash.bmp)\n");
            } else {
                printf("pal8 off: no main palette\n");
            }
        }
    }
    {
        int rec;

        if (world_load(&g_world) != 0) {
            hud("WORLD LOAD FAIL", "dink.dat", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        rec = (int)g_world.loc[DINK_START_PLAYER_MAP];
        printf("start map %d loc %d music %d\n", DINK_START_PLAYER_MAP, rec,
               (int)g_world.music[DINK_START_PLAYER_MAP]);
        if (rec < 1 || map_load_record(rec, &g_scr) != 0) {
            hud("MAP LOAD FAIL", "map.dat", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        dinkc_cmd_apply_spmap(&g_scr, DINK_START_PLAYER_MAP);
        if (spr_ok_ready() != 0) {
            hud("SPR SNAP FAIL", "malloc", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        g_edg = edraw_gfx_alloc();
        if (g_edg == NULL) {
            hud("EDG ALLOC FAIL", "malloc", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        spr_snap("load");
        memset(&g_atlas, 0, sizeof(g_atlas));
        if (tiles_build_atlas(&g_scr, &g_atlas) != 0) {
            hud("TILE ATLAS FAIL", "tiles/tsNN.bmp", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        printf("atlas cells %d sprite1 seq=%d y=%d esz=%d\n", g_atlas.used,
               (int)g_scr.sprite[1].seq, (int)g_scr.sprite[1].y,
               (int)sizeof(struct EditorSprite));
        {
            struct SeqInfo *seqs;
            struct SpriteFrame *dfr = NULL;
            int hid;

            memset(&g_hard, 0, sizeof(g_hard));
            printf("hard load\n");
            fflush(stdout);
            if (hard_load(&g_hard) != 0) {
                hud("HARD LOAD FAIL", "hard.dat", msg);
                for (;;) {
                    vid_waitvbl();
                }
            }
            hid = hard_id_for_tile(&g_hard, g_scr.t[0].square_full_idx0,
                                   g_scr.t[0].althard);
            printf("hard tile00 id %d\n", hid);
            fflush(stdout);
            seqs = (struct SeqInfo *)calloc(DINK_MAX_SEQ, sizeof(*seqs));
            printf("ini load\n");
            fflush(stdout);
            if (seqs != NULL && ini_load(seqs, DINK_MAX_SEQ) == 0) {
                int wseq = DINK_BASE_WALK + DINK_START_DIR;

                /* Do not count 12–18/71–79/101–109 (seq 75 bottles, 101
                 * bow). Pin idle+walk now: first walk after mom unfreeze
                 * opened walk/dir.ff after the house dir.ff storm and hung. */
                if (seqs[DINK_IDLE_SEQ].prefix[0] != '\0') {
                    seqs[DINK_IDLE_SEQ].nframes = ini_seq_len(
                        DINK_IDLE_SEQ,
                        ini_count_ff_frames(seqs[DINK_IDLE_SEQ].prefix));
                }
                if (wseq > 0 && wseq < DINK_MAX_SEQ &&
                    seqs[wseq].prefix[0] != '\0') {
                    seqs[wseq].nframes = ini_seq_len(
                        wseq, ini_count_ff_frames(seqs[wseq].prefix));
                }
                {
                    static const int pd[4] = {2, 4, 6, 8};
                    int pi;

                    for (pi = 0; pi < 4; pi++) {
                        int ps = DINK_BASE_PUSH + pd[pi];

                        if (ps > 0 && ps < DINK_MAX_SEQ &&
                            seqs[ps].prefix[0] != '\0') {
                            seqs[ps].nframes = ini_seq_len(
                                ps, ini_count_ff_frames(seqs[ps].prefix));
                        }
                    }
                }
                printf("ini seq %d prefix %s frames %d cx %d cy %d\n",
                       DINK_IDLE_SEQ, seqs[DINK_IDLE_SEQ].prefix,
                       seqs[DINK_IDLE_SEQ].nframes, seqs[DINK_IDLE_SEQ].cx,
                       seqs[DINK_IDLE_SEQ].cy);
                g_seqs_play = seqs;
                dinkc_cmd_bind_seqs(seqs);
                dinkc_cmd_bind_preload(preload_seq_cb);
                dinkc_cmd_bind_load_frame(load_frame_cb);
                dinkc_cmd_bind_seq_redefined(pspr_forget_seq);
                edraw_set_vram_relief(pspr_trim_for);
                pspr_set_vram_relief(dink_vram_room);
                dinkc_cmd_bind_inv(inv_show_cmd);
                /* Arrows stay resident; the box loads on open (overlay). */
                if (choice_arrows_load(seqs) != 0) {
                    printf("choice arrows load fail\n");
                }
#ifdef _arch_dreamcast
                {
                    static const struct OverlayVram ov = {
                        sprite_upload_pvr, sprite_vram_avail,
                        ov_screen_evictable, ov_screen_free_one,
                        pspr_trim_for, pspr_free_one, audio_music_pump,
                    };

                    overlay_set_vram(&ov);
                }
#endif
            }
            {
                struct HardMask mask;
                struct Player pl;
                int last_seq = 0, last_frame = 0, si;
                unsigned last_gen = pspr_gen();
                int player_map = DINK_START_PLAYER_MAP;
                int swap = 0;

                memset(&mask, 0, sizeof(mask));
                if (hard_stamp_tiles(&g_hard, &g_scr, &mask) != 0) {
                    hud("HARD STAMP FAIL", "hard.dat", msg);
                    hard_free(&g_hard);
                    free(seqs);
                    for (;;) {
                        vid_waitvbl();
                    }
                }
                /* Keep the hard.dat FILE*; do not slurp 2 MiB again on swap. */
                player_init(&pl);
                dinkc_cmd_bind_player(&pl);
                dinkc_cmd_new_game();
                dinkc_cmd_bind_sprite_freeze(brains_set_freeze);
                dinkc_cmd_bind_sprite_change(brains_change_prop);
                dinkc_cmd_bind_create(brains_create);
                dinkc_cmd_bind_move(brains_move);
                dinkc_cmd_bind_moving(brains_moving);
                dinkc_cmd_bind_hurt(brains_hurt);
                dinkc_cmd_bind_brain_lookup(brains_first_with_brain,
                                            brains_rand_with_brain);
                dinkc_cmd_bind_blood(on_blood);
                dinkc_cmd_bind_restart(game_restart_cmd);
                dinkc_cmd_bind_kill_game(game_kill_cmd);
                hit_bind_player(&pl);
                hit_bind_hit(script_on_hit_from);
                hit_bind_push(script_on_push);
                hit_bind_touch(script_on_touch);
                brains_bind_kill(script_on_kill);
                brains_bind_reuse(dinkc_vm_sprite_reused);
                dinkc_vm_bind_sprite_alive(script_sprite_alive);
                brains_bind_proc(script_try_proc);
                brains_bind_exp(game_add_exp);
                brains_bind_player(&pl);
                brains_bind_frame_box(play_frame_box);
                player_bind_damage_draw(on_dink_damage);
                saybox_bind(&g_scr, &pl);
                saybox_bind_live_xy(brains_live_xy);
                script_bind_screen(&g_scr);
                script_bind_note_script(brains_set_script);
                mem_vram_note("pre-font");
                if (font_init() == 0) {
                    printf("font atlas %dx%d bytes=%d\n", font_atlas_w(),
                           font_atlas_h(), font_atlas_bytes());
                    if (saybox_upload() != 0) {
                        printf("saybox upload fail\n");
                    }
                }
                mem_vram_note("saybox");
                if (seqs != NULL && inv_load(seqs) != 0) {
                    printf("inv gfx load fail\n");
                }
                if (seqs != NULL && status_load(seqs) != 0) {
                    printf("status gfx load fail\n");
                }
                dinkc_cmd_bind_status(status_draw_all, status_show_bmp);
                brains_bind_screen(&g_scr);
                brains_reset();
                script_enter_vision();
                {
                    g_ned = 0;

                    printf("pre-edraw live seq=%d y=%d act=%d snap seq=%d y=%d act=%d\n",
                           (int)g_scr.sprite[1].seq, (int)g_scr.sprite[1].y,
                           (int)g_scr.sprite[1].active, (int)g_spr_ok[1].seq,
                           (int)g_spr_ok[1].y, (int)g_spr_ok[1].active);
                    printf("edraw start\n");
                    fflush(stdout);
                    if (seqs != NULL) {
                        edraw_mark_created();
                        (void)edraw_load_screen(g_spr_ok, seqs, g_edg, &g_ned,
                                                script_play_vision());
                    }
                    /* memset(g_edg) smashes g_scr; scripts/talk read g_scr. */
                    spr_restore("post-edraw");
                    printf("edraw unique %d\n", g_ned);
                    mem_log("play",
                            edraw_cpu_bytes(g_edg, g_ned) + inv_cpu_bytes() +
                                status_cpu_bytes(),
                            g_ned, tiles_cache_bytes(), tiles_cache_sheets());
                    for (si = 1; si <= 100; si++) {
                        if (!editor_sprite_on_vision(&g_scr.sprite[si],
                                                     script_play_vision()) ||
                            g_scr.sprite[si].hard != 0) {
                            continue;
                        }
                        stamp_one_editor(&mask, seqs, si);
                    }
                mem_vram_note("pre-tiles");
                if (tiles_upload_pvr(&g_atlas) != 0) {
                    hud("TILE UPLOAD FAIL", NULL, msg);
                    pspr_clear();
                    edraw_free(g_edg, g_ned);
                    hard_mask_free(&mask);
                    free(seqs);
                    for (;;) {
                        vid_waitvbl();
                    }
                }
                mem_vram_note("tiles");
                if (edraw_upload_pvr(g_edg, g_ned) != 0) {
                    printf("edraw upload none\n");
                }
                mem_vram_note("edraw");
                if (seqs != NULL) {
                    dfr = pspr_get(seqs, player_pic_seq(&pl),
                                   player_pic_frame(&pl));
                }
                last_seq = player_pic_seq(&pl);
                last_frame = player_pic_frame(&pl);
                printf("play walk %d,%d seq %d\n", pl.x, pl.y, pl.seq);
                mem_vram_note("dink");
                /* Icons and choice arrows; menu art and the choice box
                 * load on open (overlay). */
                (void)inv_upload_pvr();
                (void)choice_upload_pvr();
                mem_vram_note("inv icons + arrows");
                if (status_upload_pvr() != 0) {
                    printf("status upload fail\n");
                } else {
                    status_drop_cpu();
                }
                mem_vram_note("status");
                spr_restore("pre-attach");
                spr_bake(script_play_vision());
                printf("pre-attach spr26 script=%s type=%d act=%d\n",
                       g_scr.sprite[26].script, (int)g_scr.sprite[26].type,
                       (int)g_scr.sprite[26].active);
                brains_enter(&g_scr, script_play_vision());
                brains_apply(&g_scr);
                if (seqs != NULL) {
                    edraw_created_sprites(seqs, &g_ned);
                    if (edraw_upload_pvr(g_edg, g_ned) != 0) {
                        printf("edraw created upload none\n");
                    }
                }
                /* Do not attach/run house MAIN (mom playmidi dance.mid)
                 * before START. START.c playmidi is 1003.mid on the menu. */
                g_play_pl = &pl;
                g_play_mask = &mask;
                g_play_dfr = &dfr;
                g_play_map = &player_map;
                g_play_last_seq = &last_seq;
                g_play_last_frame = &last_frame;
                dinkc_cmd_bind_load_screen(play_load_screen);
                dinkc_cmd_bind_draw_screen(play_draw_screen);
                dinkc_cmd_bind_fill_hard(play_fill_hard);
                {
                    uint32_t prev_buttons = 0;
                    int have_scene = 0;
                    int now_ms = 0;
                    int need_menu = 1;

                    g_play_have_scene = &have_scene;
                    startpause_reset();
                    for (;;) {
                        uint32_t buttons = 0;
                        int have, pdir, paused, frozen = 0;

                        /* Diagnostic: menu/restart/swap are not play-path
                         * until the gameplay section below; print last
                         * tick's queued ioplay lines outside any window. */
                        dink_io_set_play(0);
                        dink_io_flush();

                    if (need_menu) {
                        /* Boot/place_sprites runs before START; halt hearth. */
                        audio_halt_loops();
                        /* Title -> New game starts from FreeDink defaults
                         * (MAIN.c set_dink_speed(3) is not run here);
                         * Continue's load_game resets them too. */
                        dinkc_cmd_new_game();
                        (void)title_pick_and_apply(seqs, &pl, &player_map);
                        brains_place_sounds();
                        inv_sync_icons();
#ifdef _arch_dreamcast
                        (void)inv_upload_pvr();
#endif
                        g_need_title = 0;
                        startpause_reset();
                        dinkc_vm_choice_close_saves();
                        swap = 1;
                        need_menu = 0;
                    }
                    if (g_need_title) {
                        g_need_title = 0;
                        g_show_splash = 1;
                        startpause_reset();
                        dinkc_vm_choice_close_saves();
                        saybox_clear();
                        need_menu = 1;
                        continue;
                    }

                    if (g_need_restart) {
                        dinkc_vm_reset();
                        dinkc_var_init();
                        dinkc_cmd_reset_inv();
                        inv_reset();
                        player_init(&pl);
                        dinkc_cmd_bind_player(&pl);
                        dinkc_cmd_new_game();
                        hit_bind_player(&pl);
                        script_clear_dink_die();
                        player_map = DINK_START_PLAYER_MAP;
                        g_need_restart = 0;
                        mem_swap_reset();
                        give_start_fists();
                        inv_sync_icons();
#ifdef _arch_dreamcast
                        (void)inv_upload_pvr();
#endif
                        swap = 1;
                    }

                    if (swap) {
                        unsigned swap_t0 = mem_now_ms();

                        /* Keep the current loop on the AICA ring; do not
                         * fopen a new MIDI until packs are done. Pump is
                         * dink_fread_n → audio_music_pump (no GD-ROM). */
                        audio_music_disc_hold(1);
                        /* Edge/warp is load then draw (FreeDink). DinkC
                         * calls them separately (S1-HOLE / S1-LTR). */
                        if (play_load_screen(player_map) != 0) {
                            audio_music_disc_hold(0);
                            swap = 0;
                            continue;
                        }
                        if (play_draw_screen(0) != 0) {
                            audio_music_disc_hold(0);
                            swap = 0;
                            continue;
                        }
                        audio_music_disc_hold(0);
                        mem_log("swap",
                                edraw_cpu_bytes(g_edg, g_ned) + inv_cpu_bytes() +
                                    status_cpu_bytes(),
                                g_ned, tiles_cache_bytes(), tiles_cache_sheets());
                        /* DinkC: live scripts and their shared code (§1.2). */
                        printf("mem dinkc live=%d code_n=%d code_bytes=%u\n",
                               dinkc_vm_live(), dinkc_vm_code_count(),
                               (unsigned)dinkc_vm_code_bytes());
                        printf("swap_ms %u\n", mem_now_ms() - swap_t0);
                        swap = 0;
                        continue;
                    }

                    /* Diagnostic: from here to the next swap is play-path.
                     * Any disc/frame load now logs `ioplay`. */
                    dink_io_set_play(1);
                    /* Finish the last scene before evicting its sprite tex.
                     * Punch cx (~58) vs idle (~36): freeing mid-frame shows
                     * idle pixels at the punch quad (ghost to the left).
                     * Skip wait until one play scene has been submitted. */
                    if (have_scene) {
                        pvr_wait_ready();
                        /* Scene finished. Warp/edge continue skips
                         * pvr_scene_finish; play_draw_screen must not
                         * wait again on an idle GPU (bar map 34 lock). */
                        have_scene = 0;
                    }
                    /* Diagnostic: per-tick pacing and VRAM low-water mark
                     * (one pvr_mem_available + a 128-slot sum per tick). */
                    g_tick_t0 = dink_io_now_us();
                    dink_io_tick(g_tick_t0);
                    dink_io_note_vram((unsigned)sprite_vram_avail(),
                                      (unsigned)edraw_tex_bytes(g_edg, g_ned),
                                      (unsigned)pspr_bytes());
                    if (g_spr_ok != NULL) {
                        memcpy(g_scr.sprite, g_spr_ok, 101u * sizeof(*g_spr_ok));
                    }
                    brains_apply(&g_scr);
                    audio_music_poll();
                    have = (pad_poll_port0(&buttons) == 0);
                    paused = startpause_open();
                    if (have && status_map_active()) {
#ifdef _arch_dreamcast
                        (void)status_upload_pvr();
                        if (!status_map_ready()) {
                            printf("map upload fail\n");
                            status_map_dismiss();
                            dinkc_vm_advance_bmp();
                        }
#endif
                        status_map_tick(now_ms);
                        if (pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_A) ||
                            pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_B) ||
                            pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_X) ||
                            pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_Y) ||
                            pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_START) ||
                            pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_L)) {
                            status_map_dismiss();
                            dinkc_vm_advance_bmp();
                        }
                    } else if (have && inv_showing()) {
                        inv_tick(prev_buttons, buttons, now_ms);
                    } else if (have && pl.freeze != 0 && pl.nocontrol == 0 &&
                               !dinkc_vm_waiting_choice() &&
                               dinkc_vm_last_talk() &&
                               pad_just_pressed(prev_buttons, buttons,
                                                DINK_PAD_A)) {
                        /* FreeDink human_brain: nocontrol returns first, a
                         * choice wins, then ACTION_TALK while frozen kills
                         * play.last_talk's text only. Unfrozen A is talk.
                         * B is attack; say_stop_npc does not set last_talk. */
                        dinkc_vm_advance_last_talk();
                        if (!dinkc_vm_waiting_say()) {
                            saybox_clear();
                        }
                    } else if (have && dinkc_vm_waiting_choice()) {
                        if (pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_UP) ||
                            pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_LEFT)) {
                            dinkc_vm_choice_move(-1);
                            (void)audio_playsound(11, 22050, 0, 0, 0);
                        } else if (pad_just_pressed(prev_buttons, buttons,
                                                    DINK_PAD_DOWN) ||
                                   pad_just_pressed(prev_buttons, buttons,
                                                    DINK_PAD_RIGHT)) {
                            dinkc_vm_choice_move(1);
                            (void)audio_playsound(11, 22050, 0, 0, 0);
                        } else if (pad_just_pressed(prev_buttons, buttons,
                                                    DINK_PAD_A)) {
                            dinkc_vm_choice_pick(dinkc_vm_choice_cur());
                            (void)audio_playsound(17, 22050, 0, 0, 0);
                        }
                    } else if (have && !inv_showing() && !status_map_active() &&
                               !dinkc_vm_waiting_say() &&
                               !dinkc_vm_waiting_choice() &&
                               startpause_eats_pad(prev_buttons, buttons)) {
                        int pr = startpause_tick(prev_buttons, buttons);

                        if (pr == STARTPAUSE_TITLE) {
                            dinkc_vm_choice_close_saves();
                            g_need_title = 1;
                            g_show_splash = 1;
                        } else if (pr == STARTPAUSE_CONTINUE || pr == -2) {
                            dinkc_vm_choice_close_saves();
                            saybox_clear();
                        } else if (startpause_open()) {
                            dinkc_vm_choice_open_pause(startpause_focus() + 1);
                        }
                    } else if (have && !paused && pl.freeze == 0 &&
                        pl.nocontrol == 0 &&
                        !dinkc_vm_waiting_choice() &&
                        pad_just_pressed(prev_buttons, buttons, DINK_PAD_L)) {
                        (void)script_on_button(6);
#ifdef _arch_dreamcast
                        if (status_map_active()) {
                            if (status_upload_pvr() != 0 ||
                                !status_map_ready()) {
                                printf("map upload fail\n");
                                status_map_dismiss();
                                dinkc_vm_advance_bmp();
                            }
                        }
#endif
                    } else if (have && !paused && pl.freeze == 0 &&
                        pl.nocontrol == 0 &&
                        !dinkc_vm_waiting_choice() &&
                        pad_just_pressed(prev_buttons, buttons, DINK_PAD_Y)) {
                        inv_open(now_ms);
#ifdef _arch_dreamcast
                        (void)inv_upload_pvr(); /* new icons */
#endif
                    } else if (have && !paused && pl.freeze == 0 &&
                        pl.nocontrol == 0 &&
                        pad_just_pressed(prev_buttons, buttons, DINK_PAD_A)) {
                        int slot = talk_probe(&g_scr, g_edg, g_ned, seqs, pl.x,
                                              pl.y, pl.dir, script_play_vision());

                        if (slot < 1) {
                            saybox_set(talk_miss_line((rand() % 6) + 1), 1);
                        } else {
                            script_on_talk(slot);
                        }
                    } else if (have && !paused && pl.freeze == 0 &&
                        pl.nocontrol == 0 &&
                        !dinkc_vm_waiting_choice() &&
                        pad_just_pressed(prev_buttons, buttons, DINK_PAD_B)) {
                        if (dinkc_cmd_weapon_armed() && pl.base_hit > 0) {
                            (void)dinkc_cmd_weapon_use();
                        } else {
                            player_attack(&pl, seqs);
                        }
                    }
                    if (have && !paused && !inv_showing() &&
                        !status_map_active() &&
                        pl.freeze == 0 &&
                        pl.nocontrol == 0 &&
                        !dinkc_vm_waiting_choice()) {
                        if (dinkc_cmd_magic_armed() && (buttons & DINK_PAD_X)) {
                            int lv = dinkc_var_get("&magic_level",
                                                   DINKC_GLOBAL_SCOPE, 1);
                            int cost = dinkc_var_get("&magic_cost",
                                                     DINKC_GLOBAL_SCOPE, 1);

                            if (lv >= cost) {
                                (void)dinkc_cmd_magic_use();
                            }
                        } else if (pad_just_pressed(prev_buttons, buttons,
                                                    DINK_PAD_X)) {
                            saybox_set(magic_miss_line((rand() % 6) + 1), 1);
                        }
                    }
                    prev_buttons = have ? buttons : 0;
                    if (dinkc_var_get("&update_status", DINKC_GLOBAL_SCOPE, 1) ==
                        1) {
                        status_update(now_ms);
                    }
                    /* FreeDink update_frame: while stop_entire_game is set
                     * and a choice is up, only the choice runs (no
                     * sprites, Dink, callbacks or waits). */
                    frozen = dinkc_cmd_stop_entire_game(
                        dinkc_vm_waiting_choice());
                    dinkc_vm_set_now(now_ms);
                    if (!frozen) {
                        dinkc_vm_tick(now_ms);
                    }
                    /* Stopped game: no say timeout, no thaw (FreeDink
                     * update_frame returns before text sprites run). */
                    if (!frozen && saybox_tick(now_ms) &&
                        dinkc_vm_waiting_say()) {
                        dinkc_vm_advance_say();
                    }
                    if (!frozen) {
                        dinkc_cmd_thaw_if_idle();
                    }
                    if (seqs != NULL && !inv_showing() && !status_map_active() &&
                        !frozen) {
                        pl.defense = dinkc_var_get("&defense", DINKC_GLOBAL_SCOPE,
                                                   1);
                        brains_tick(&g_scr, seqs, &mask, now_ms,
                                    script_play_vision());
                        if (dinkc_cmd_hard_redraw_take()) {
                            stamp_editor_hard(&mask, seqs);
                        }
                        {
                            int ei, sq, fr;

                            edraw_live_this_draw(seqs, g_ned);
                            for (ei = 1; ei <= 100; ei++) {
                                sq = (int)g_scr.sprite[ei].seq;
                                fr = (int)g_scr.sprite[ei].frame;

                                if (!editor_sprite_draw(&g_scr.sprite[ei],
                                                        script_play_vision())) {
                                    continue;
                                }
                                if ((int)g_scr.sprite[ei].brain == 6 &&
                                    brains_slot_live(ei)) {
                                    continue;
                                }
                                edraw_ensure_draw_frame(seqs, sq, fr, 0);
                            }
                            for (ei = 0; ei < g_nbg; ei++) {
                                const struct EditorSprite *bes = &g_bg[ei].es;

                                if (bes->type != 0 ||
                                    !editor_sprite_draw(bes,
                                                        script_play_vision())) {
                                    continue;
                                }
                                edraw_ensure_draw_frame(seqs, (int)bes->seq,
                                                        (int)bes->frame, 0);
                            }
                            for (ei = 1; ei <= 99; ei++) {
                                if (!brains_seq_frame(ei, &sq, &fr)) {
                                    continue;
                                }
                                edraw_ensure_draw_frame(
                                    seqs, sq, fr, brains_slot_brain(ei) == 6);
                            }
                            edraw_reap_unused(g_edg, &g_ned, seqs);
                        }
                    }
                    now_ms += DINKC_TICK_MS;
                    if (dinkc_vm_choice_n() > 0) {
                        choice_tick(now_ms);
                    }
                    pdir = have ? pad_dir_from_buttons(buttons) : 0;
                    pdir = player_walk_pad(pdir, pl.freeze,
                                           dinkc_vm_waiting_choice());
                    if (seqs != NULL) {
                        int wed = (inv_showing() || status_map_active() ||
                                   paused)
                                      ? 0
                                      : screen_process_warp();

                        if (!inv_showing() && !status_map_active() &&
                            !paused && !frozen) {
                            dinkc_vm_resume_move();
                        }
                        if (wed > 0) {
                            /* process_warp_man: anim done or sprite gone. */
                            if (!brains_slot_live(wed) ||
                                brains_slot_seq(wed) == 0) {
                                if (screen_try_warp(&g_world, &g_scr, wed,
                                                    &player_map, &pl) == 0) {
                                    dinkc_cmd_note_map(player_map,
                                                       (int)g_world.indoor[player_map]);
                                    swap = 1;
                                    continue;
                                }
                                screen_warp_clear();
                            }
                        } else if (!inv_showing() && !status_map_active() &&
                                   !paused && !frozen) {
                            player_step(&pl, pdir, &mask, seqs, now_ms, &g_scr);
                            hit_touch_list(pl.x, pl.y, now_ms, g_edg, g_ned,
                                           seqs);
                            /* FreeDink special_block: no freeze check.
                             * s1-h1-s move_stop onto the door while frozen. */
                            if (pl.warp_hit > 0) {
                                int wr = screen_special_block(
                                    &g_world, &g_scr, pl.warp_hit, &player_map,
                                    &pl);

                                if (wr == 0) {
                                    dinkc_cmd_note_map(player_map,
                                                       (int)g_world.indoor[player_map]);
                                    swap = 1;
                                    continue;
                                }
                                if (wr == 1) {
                                    int pseq = (int)g_scr.sprite[pl.warp_hit]
                                                   .parm_seq;

                                    (void)brains_change_prop(pl.warp_hit,
                                                             DINKC_SP_SEQ,
                                                             pseq);
                                }
                            }
                            /* FreeDink did_player_cross_screen: off while
                             * dink_can_walk_off_screen(1) (S2-CULG). */
                            if (pl.freeze == 0 &&
                                !dinkc_cmd_walk_off_screen() &&
                                screen_try_cross(&g_world, &player_map, &pl)) {
                                dinkc_cmd_note_map(player_map,
                                                   (int)g_world.indoor[player_map]);
                                swap = 1;
                                continue;
                            }
                        }
                        if (pl.just_hit) {
                            int str = dinkc_var_get("&strength",
                                                    DINKC_GLOBAL_SCOPE, 1);

                            hit_tag_list(1, pl.x, pl.y, pl.dir, str, pl.range,
                                         g_edg, g_ned, seqs);
                        }
                        if (pl.just_push) {
                            hit_tag_list_push(pl.x, pl.y, g_edg, g_ned, seqs);
                        }
                        {
                            int hi;

                            for (hi = 1; hi <= 99; hi++) {
                                if (brains_take_just_hit(hi)) {
                                    int hx, hy;

                                    if (brains_live_xy(hi, &hx, &hy)) {
                                        hit_tag_list(hi, hx, hy,
                                                     brains_change_prop(
                                                         hi, DINKC_SP_DIR, -1),
                                                     brains_strength(hi),
                                                     brains_range(hi), g_edg,
                                                     g_ned, seqs);
                                    }
                                }
                            }
                        }
                        /* FreeDink human_brain is the only place Dink's
                         * queued damage comes off &life (and draws the
                         * number). It does not run during a scripted move,
                         * a warp, a frozen game, or with menus up. */
                        if (!pl.move_active && wed == 0 && !frozen &&
                            !inv_showing() && !status_map_active() &&
                            !paused) {
                            int life = dinkc_var_get("&life", DINKC_GLOBAL_SCOPE,
                                                     1);

                            if (player_apply_life(&pl, &life)) {
                                dinkc_var_set("&life", life, DINKC_GLOBAL_SCOPE,
                                              1);
                                (void)script_on_dink_die();
                            } else {
                                dinkc_var_set("&life", life, DINKC_GLOBAL_SCOPE,
                                              1);
                            }
                        }
                        /* pspr_gen: DinkC init redefined a Dink seq (arm
                         * sword/bow/fists); re-get the same seq/frame. */
                        if (player_pic_seq(&pl) != last_seq ||
                            player_pic_frame(&pl) != last_frame ||
                            pspr_gen() != last_gen) {
                            int dseq = player_pic_seq(&pl);
                            int dframe = player_pic_frame(&pl);
                            struct SpriteFrame *nxt = NULL;
                            struct DinkIoStats io0, io1;
                            unsigned io_t0 = dink_io_now_us();
                            unsigned miss0 = pspr_misses();

                            /* Last scene finished at the top of play, so
                             * pspr may free an old Dink texture here. */
                            dink_io_stats(&io0);
                            dink_io_ctx("dink");
                            if (dseq > 0) {
                                nxt = pspr_get(seqs, dseq, dframe);
                            }
                            if (nxt != NULL) {
                                dfr = nxt;
                            }
                            dink_io_ctx(NULL);
                            dink_io_stats(&io1);
                            /* Note loads only; a cache hit is not a frame
                             * load. A failed load is noted as seq<0. */
                            if (dseq > 0 &&
                                (nxt == NULL || pspr_misses() != miss0)) {
                                dink_io_note_frame(
                                    "dink", nxt != NULL ? dseq : -dseq, dframe,
                                    dink_io_now_us() - io_t0,
                                    io1.ops - io0.ops);
                            }
                            if (dseq > 0 && nxt == NULL) {
                                printf("dink seq load fail seq=%d fr=%d\n",
                                       dseq, dframe);
                            }
                            last_seq = dseq;
                            last_frame = dframe;
                            last_gen = pspr_gen();
                        }
                    }
                    overlay_sync(seqs);
                    /* After every real load of this tick (sprites, Dink,
                     * overlays) and before the scene: spare time only. */
                    if (seqs != NULL && !inv_showing() &&
                        !status_map_active()) {
                        anim_lookahead(seqs, &pl);
                    }
                    pvr_scene_begin();
                    pvr_list_begin(PVR_LIST_OP_POLY);
                    tiles_draw_pvr(&g_atlas);
                    pvr_list_finish();
                    pvr_list_begin(PVR_LIST_PT_POLY);
                    {
                        /* Static: 100 rows + 100 baked background + Dink
                         * is ~11 KB, too much for the KOS thread stack. */
                        static struct DrawSpr {
                            int rank, x, y, bg, size, slot;
                            int al, at, ar, ab;
                            struct SpriteFrame *fr;
                        } draw[202];
                        int nd = 0, a, b;

                        if (g_spr_ok != NULL) {
                        memcpy(g_scr.sprite, g_spr_ok, 101u * sizeof(*g_spr_ok));
                    }
                        brains_apply(&g_scr);
                        /* Baked background rows: under everything. */
                        for (si = 0; si < g_nbg && nd < 200; si++) {
                            const struct EditorSprite *es = &g_bg[si].es;
                            struct SpriteFrame *ef;
                            int fr = (int)es->frame < 1 ? 1 : (int)es->frame;

                            if (es->type != 0 ||
                                !editor_sprite_draw(es, script_play_vision())) {
                                continue;
                            }
                            ef = edraw_find(g_edg, g_ned, (int)es->seq, fr);
                            if (ef == NULL) {
                                continue;
                            }
                            draw[nd].rank = editor_sprite_rank_y(es);
                            draw[nd].x = (int)es->x;
                            draw[nd].y = (int)es->y;
                            draw[nd].bg = 1;
                            draw[nd].slot = g_bg[si].ed;
                            draw[nd].size = (int)es->size;
                            draw[nd].al = (int)es->alt_l;
                            draw[nd].at = (int)es->alt_t;
                            draw[nd].ar = (int)es->alt_r;
                            draw[nd].ab = (int)es->alt_b;
                            draw[nd].fr = ef;
                            nd++;
                        }
                        for (si = 1; si <= 100 && nd < 200; si++) {
                            int seq, fr;
                            struct SpriteFrame *ef;

                            if (!editor_sprite_draw(&g_scr.sprite[si],
                                                    script_play_vision()) ||
                                brains_slot_disabled(si) ||
                                brains_slot_nodraw(si)) {
                                continue;
                            }
                            seq = (int)g_scr.sprite[si].seq;
                            fr = (int)g_scr.sprite[si].frame;
                            if (fr < 1) {
                                fr = 1;
                            }
                            ef = edraw_find(g_edg, g_ned, seq, fr);
                            if (ef == NULL) {
                                continue;
                            }
                            /* que is depth only (screen_rank). x,y stay map. */
                            draw[nd].rank =
                                editor_sprite_rank_y(&g_scr.sprite[si]);
                            draw[nd].x = (int)g_scr.sprite[si].x;
                            draw[nd].y = (int)g_scr.sprite[si].y;
                            draw[nd].bg = (g_scr.sprite[si].type == 0);
                            draw[nd].slot = si;
                            draw[nd].size = brains_slot_live(si)
                                                ? brains_slot_size(si)
                                                : (int)g_scr.sprite[si].size;
                            draw[nd].al = (int)g_scr.sprite[si].alt_l;
                            draw[nd].at = (int)g_scr.sprite[si].alt_t;
                            draw[nd].ar = (int)g_scr.sprite[si].alt_r;
                            draw[nd].ab = (int)g_scr.sprite[si].alt_b;
                            draw[nd].fr = ef;
                            nd++;
                        }
                        if (dfr != NULL && nd < 201) {
                            draw[nd].rank = pl.y;
                            draw[nd].x = pl.x;
                            draw[nd].y = pl.y;
                            draw[nd].bg = 0;
                            draw[nd].slot = DINK_DRAW_PLAYER_SLOT;
                            draw[nd].size = 100;
                            draw[nd].al = draw[nd].at = 0;
                            draw[nd].ar = draw[nd].ab = 0;
                            draw[nd].fr = dfr;
                            nd++;
                        }
                        for (a = 0; a < nd; a++) {
                            for (b = a + 1; b < nd; b++) {
                                if (editor_draw_behind(draw[b].bg, draw[b].rank,
                                                       draw[b].slot, draw[a].bg,
                                                       draw[a].rank,
                                                       draw[a].slot)) {
                                    struct DrawSpr tmp = draw[a];

                                    draw[a] = draw[b];
                                    draw[b] = tmp;
                                }
                            }
                        }
                        for (a = 0; a < nd; a++) {
                            /* Shared z: PT GEQUAL later-wins. Not 1.5+i*0.01. */
                            sprite_draw_pvr_alt_size(
                                draw[a].fr, (float)draw[a].x,
                                (float)draw[a].y, 1.6f, draw[a].al, draw[a].at,
                                draw[a].ar, draw[a].ab, draw[a].size);
                        }
                        /* FreeDink: status chrome is on the background blit;
                         * text_draw / choice run after sprites. PT GEQUAL
                         * later-wins: HUD at 3.5 was covering say at 3.0. */
                        status_draw_pvr(3.5f);
                        {
                            int fi, fx, fy, fnum;

                            for (fi = 1; fi <= 99; fi++) {
                                if (brains_floater_num(fi, &fx, &fy, &fnum)) {
                                    saybox_draw_num_pvr(fx, fy, fnum, 3.6f);
                                }
                            }
                        }
                        saybox_draw_pvr(3.7f);
                        saybox_draw_choices_pvr(3.8f);
                        inv_draw_pvr(4.0f);
                        status_draw_map_pvr(5.0f);
                    }
                    pvr_list_finish();
                    if (fade_brightness() < FADE_FULL) {
                        pvr_list_begin(PVR_LIST_TR_POLY);
                        fade_draw_pvr();
                        pvr_list_finish();
                    }
                    /* Diagnostic: the tick's own work, no vsync or log. */
                    dink_io_work(dink_io_now_us() - g_tick_t0);
                    pvr_scene_finish();
                    have_scene = 1;
                    }
                }
                }
            }
        }
    }
    for (;;) {
        vid_waitvbl();
    }
}

#else

int dinkcast_host_boot_rgb565(void)
{
    return (int)DINK_BOOT_RGB565;
}

#endif
