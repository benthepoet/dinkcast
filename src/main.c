/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Boot: 640×480. Probe dink.dat. Show official title still (Bite 3.4).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boot.h"
#include "dinkdat.h"
#include "fs.h"
#include "edraw.h"
#include "font.h"
#include "hard.h"
#include "hit.h"
#include "ini.h"
#include "mapscr.h"
#include "pad.h"
#include "player.h"
#include "saybox.h"
#include "screen.h"
#include "script.h"
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "sprite.h"
#include "start_map.h"
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
static struct TileAtlas g_atlas;
static struct HardMap g_hard;
static struct EdGfx *g_edg;

static int spr_ok_ready(void)
{
    if (g_spr_ok != NULL) {
        return 0;
    }
    g_spr_ok = (struct EditorSprite *)malloc(101u * sizeof(*g_spr_ok));
    return g_spr_ok == NULL ? -1 : 0;
}

static void spr_snap(const char *tag)
{
    if (g_spr_ok == NULL) {
        return;
    }
    memcpy(g_spr_ok, g_scr.sprite, 101u * sizeof(*g_spr_ok));
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

static void hud(const char *line0, const char *line1, const char *line2)
{
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
    /* Title tex already pvr_mem_free'd inside present (Bite 4.2). */
    {
        enum GameState st = GAME_STATE_LOADING;

        (void)st;
        printf("leave_title\n");
        vid_set_mode(DM_640x480, PM_RGB565);
        vid_clear(DINK_BOOT_R, DINK_BOOT_G, DINK_BOOT_B);
        hud("leave_title", "GAME_STATE_LOADING", msg);
        /* pvr_shutdown left the splash in the last PVR frame. Re-init and
         * draw brown or Flycast keeps showing the title during house load. */
        tiles_draw_clear_pvr(0xff5a3a1a);
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
            struct SpriteFrame spr;
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
            memset(&spr, 0, sizeof(spr));
            printf("ini load\n");
            fflush(stdout);
            if (seqs != NULL && ini_load(seqs, DINK_MAX_SEQ) == 0) {
                int wseq = DINK_BASE_WALK + DINK_START_DIR;

                /* Do not count 12–18/71–79/101–109 (seq 75 bottles, 101
                 * bow). Pin idle+walk now: first walk after mom unfreeze
                 * opened walk/dir.ff after the house dir.ff storm and hung. */
                if (seqs[DINK_IDLE_SEQ].prefix[0] != '\0') {
                    seqs[DINK_IDLE_SEQ].nframes =
                        ini_count_ff_frames(seqs[DINK_IDLE_SEQ].prefix);
                }
                if (wseq > 0 && wseq < DINK_MAX_SEQ &&
                    seqs[wseq].prefix[0] != '\0') {
                    seqs[wseq].nframes =
                        ini_count_ff_frames(seqs[wseq].prefix);
                }
                printf("ini seq %d prefix %s frames %d cx %d cy %d\n",
                       DINK_IDLE_SEQ, seqs[DINK_IDLE_SEQ].prefix,
                       seqs[DINK_IDLE_SEQ].nframes, seqs[DINK_IDLE_SEQ].cx,
                       seqs[DINK_IDLE_SEQ].cy);
            }
            {
                struct HardMask mask;
                struct Player pl;
                int last_seq = 0, last_frame = 0, si;
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
                /* Keep hard.dat: swap reopen of the 2 MiB file hangs on /cd. */
                player_init(&pl);
                dinkc_cmd_bind_player(&pl);
                saybox_bind(&g_scr, &pl);
                if (seqs != NULL) {
                    sprite_load_seq_frame(&seqs[pl.seq], pl.seq, pl.frame, &spr);
                }
                {
                    int ned = 0;

                    printf("pre-edraw live seq=%d y=%d act=%d snap seq=%d y=%d act=%d\n",
                           (int)g_scr.sprite[1].seq, (int)g_scr.sprite[1].y,
                           (int)g_scr.sprite[1].active, (int)g_spr_ok[1].seq,
                           (int)g_spr_ok[1].y, (int)g_spr_ok[1].active);
                    printf("edraw start\n");
                    fflush(stdout);
                    if (seqs != NULL) {
                        (void)edraw_load_screen(g_spr_ok, seqs, g_edg, &ned);
                    }
                    /* memset(g_edg) smashes g_scr; scripts/talk read g_scr. */
                    spr_restore("post-edraw");
                    printf("edraw unique %d\n", ned);
                    for (si = 1; si <= 100; si++) {
                        struct SpriteFrame *ef;
                        int hl, ht, hr, hb, cx, cy;

                        if (!editor_sprite_on_vision(&g_scr.sprite[si],
                                                     DINK_VISION_DEFAULT) ||
                            g_scr.sprite[si].hard != 0) {
                            continue;
                        }
                        ef = edraw_find(g_edg, ned, (int)g_scr.sprite[si].seq,
                                        (int)g_scr.sprite[si].frame < 1
                                            ? 1
                                            : (int)g_scr.sprite[si].frame);
                        if (ef != NULL) {
                            hard_stamp_box(&mask, (int)g_scr.sprite[si].x,
                                           (int)g_scr.sprite[si].y, ef->hl,
                                           ef->ht, ef->hr, ef->hb,
                                           g_scr.sprite[si].is_warp
                                               ? 100 + si
                                               : 1);
                            continue;
                        }
                        if (seqs == NULL) {
                            continue;
                        }
                        ini_frame_geom(&seqs[g_scr.sprite[si].seq],
                                       (int)g_scr.sprite[si].seq,
                                       (int)g_scr.sprite[si].frame < 1
                                           ? 1
                                           : (int)g_scr.sprite[si].frame,
                                       50, 50, &cx, &cy, &hl, &ht, &hr, &hb);
                        hard_stamp_box(&mask, (int)g_scr.sprite[si].x,
                                       (int)g_scr.sprite[si].y, hl, ht, hr, hb,
                                       g_scr.sprite[si].is_warp ? 100 + si : 1);
                    }
                if (tiles_upload_pvr(&g_atlas) != 0) {
                    hud("TILE UPLOAD FAIL", NULL, msg);
                    sprite_frame_free(&spr);
                    edraw_free(g_edg, ned);
                    hard_mask_free(&mask);
                    free(seqs);
                    for (;;) {
                        vid_waitvbl();
                    }
                }
                if (edraw_upload_pvr(g_edg, ned) != 0) {
                    printf("edraw upload none\n");
                }
                if (spr.argb1555 != NULL) {
                    (void)sprite_upload_pvr(&spr);
                }
                last_seq = pl.seq;
                last_frame = pl.frame;
                printf("play walk %d,%d seq %d\n", pl.x, pl.y, pl.seq);
                if (font_init() == 0) {
                    printf("font atlas %dx%d bytes=%d\n", font_atlas_w(),
                           font_atlas_h(), font_atlas_bytes());
                    if (saybox_upload() != 0) {
                        printf("saybox upload fail\n");
                    }
                }
                spr_restore("pre-attach");
                printf("pre-attach spr26 script=%s type=%d act=%d\n",
                       g_scr.sprite[26].script, (int)g_scr.sprite[26].type,
                       (int)g_scr.sprite[26].active);
                script_bind_screen(&g_scr);
                script_on_main(0); /* also preloads unique sprite story files */
                {
                    uint32_t prev_buttons = 0;
                    int have_scene = 0;
                    int now_ms = 0;

                    for (;;) {
                        uint32_t buttons = 0;
                        int have, pdir;

                    if (swap) {
                        int rec2, nstamp;

                        rec2 = (int)g_world.loc[player_map];
                        if (rec2 < 1) {
                            printf("swap skip map %d loc %d\n", player_map,
                                   rec2);
                            swap = 0;
                            continue;
                        }
                        if (have_scene) {
                            pvr_wait_ready();
                            have_scene = 0;
                        }
                        dinkc_vm_kill_all();
                        pl.freeze = 0;
                        dinkc_cmd_thaw_if_idle();
                        saybox_clear();
                        /* Keep editor gfx that the next screen still uses. */
                        /* Keep tile CPU+PVR until a new atlas is ready. */
                        sprite_frame_free(&spr);
                        hard_mask_free(&mask);
                        printf("enter map %d loc %d\n", player_map, rec2);
                        if (map_load_record(rec2, &g_scr) != 0) {
                            printf("map load fail %d\n", rec2);
                            swap = 0;
                            continue;
                        }
                        spr_snap("swap");
                        dinkc_var_set("&player_map", player_map,
                                      DINKC_GLOBAL_SCOPE, 1);
                        memset(&mask, 0, sizeof(mask));
                        if (!g_hard.ready) {
                            printf("swap hard load\n");
                            if (hard_load(&g_hard) != 0) {
                                printf("hard reload fail\n");
                            }
                            printf("swap hard loaded\n");
                        } else {
                            printf("swap hard keep\n");
                        }
                        /* Stamp even if reload failed: empty hid still
                         * allocates the mask so sprite/warp boxes apply. */
                        if (hard_stamp_tiles(&g_hard, &g_scr, &mask) != 0) {
                            printf("hard restamp fail\n");
                        }
                        spr_restore("swap-edraw");
                        if (seqs != NULL) {
                            (void)edraw_load_screen(g_spr_ok, seqs, g_edg, &ned);
                        }
                        spr_restore("swap-post-edraw");
                        printf("edraw unique %d\n", ned);
                        for (nstamp = 1; nstamp <= 100; nstamp++) {
                            struct SpriteFrame *ef;
                            int hl, ht, hr, hb, cx, cy, hid, seq, fr;

                            if (!editor_sprite_on_vision(&g_scr.sprite[nstamp],
                                                         DINK_VISION_DEFAULT) ||
                                g_scr.sprite[nstamp].hard != 0) {
                                continue;
                            }
                            seq = (int)g_scr.sprite[nstamp].seq;
                            fr = (int)g_scr.sprite[nstamp].frame < 1
                                     ? 1
                                     : (int)g_scr.sprite[nstamp].frame;
                            if (seq < 1 || seq >= DINK_MAX_SEQ) {
                                continue;
                            }
                            hid = g_scr.sprite[nstamp].is_warp ? 100 + nstamp
                                                               : 1;
                            ef = edraw_find(g_edg, ned, seq, fr);
                            if (ef != NULL) {
                                hard_stamp_box(&mask,
                                               (int)g_scr.sprite[nstamp].x,
                                               (int)g_scr.sprite[nstamp].y,
                                               ef->hl, ef->ht, ef->hr, ef->hb,
                                               hid);
                                continue;
                            }
                            if (seqs == NULL) {
                                continue;
                            }
                            ini_frame_geom(&seqs[seq], seq, fr, 50, 50, &cx,
                                           &cy, &hl, &ht, &hr, &hb);
                            hard_stamp_box(&mask, (int)g_scr.sprite[nstamp].x,
                                           (int)g_scr.sprite[nstamp].y, hl, ht,
                                           hr, hb, hid);
                        }
                        printf("swap stamp ok\n");
                        fflush(stdout);
                        dink_cd_settle();
                        printf("swap tiles build\n");
                        fflush(stdout);
                        {
                            struct TileAtlas nxt;

                            memset(&nxt, 0, sizeof(nxt));
                            if (tiles_build_atlas(&g_scr, &nxt) == 0) {
                                tiles_free(&g_atlas);
                                g_atlas = nxt;
                                printf("swap atlas ok\n");
                                printf("swap tiles upload\n");
                                if (tiles_upload_pvr(&g_atlas) != 0) {
                                    printf("swap tiles upload fail\n");
                                } else {
                                    printf("swap tiles upload ok\n");
                                }
                            } else {
                                printf("swap atlas fail keep\n");
                            }
                        }
                        if (edraw_upload_pvr(g_edg, ned) != 0) {
                            printf("swap edraw upload fail\n");
                        } else {
                            printf("swap edraw upload ok n=%d\n", ned);
                        }
                        if (seqs != NULL) {
                            sprite_load_seq_frame(&seqs[pl.seq], pl.seq,
                                                  pl.frame, &spr);
                            if (spr.argb1555 != NULL) {
                                (void)sprite_upload_pvr(&spr);
                            }
                            last_seq = pl.seq;
                            last_frame = pl.frame;
                            printf("swap dink seq=%d\n", pl.seq);
                        }
                        spr_restore("swap-pre-attach");
                        script_bind_screen(&g_scr);
                        saybox_bind(&g_scr, &pl);
                        script_on_main(0);
                        swap = 0;
                        continue;
                    }

                    /* Finish the last scene before evicting its sprite tex.
                     * Punch cx (~58) vs idle (~36): freeing mid-frame shows
                     * idle pixels at the punch quad (ghost to the left).
                     * Skip wait until one play scene has been submitted. */
                    if (have_scene) {
                        pvr_wait_ready();
                    }
                    if (g_spr_ok != NULL) {
                        memcpy(g_scr.sprite, g_spr_ok, 101u * sizeof(*g_spr_ok));
                    }
                    have = (pad_poll_port0(&buttons) == 0);
                    if (have && dinkc_vm_waiting_say() &&
                        (pad_just_pressed(prev_buttons, buttons, DINK_PAD_A) ||
                         pad_just_pressed(prev_buttons, buttons, DINK_PAD_B))) {
                        dinkc_vm_advance_say();
                        if (!dinkc_vm_waiting_say()) {
                            saybox_clear();
                        }
                    } else if (have && dinkc_vm_waiting_choice()) {
                        if (pad_just_pressed(prev_buttons, buttons,
                                             DINK_PAD_UP)) {
                            dinkc_vm_choice_move(-1);
                        } else if (pad_just_pressed(prev_buttons, buttons,
                                                    DINK_PAD_DOWN)) {
                            dinkc_vm_choice_move(1);
                        } else if (pad_just_pressed(prev_buttons, buttons,
                                                    DINK_PAD_A)) {
                            dinkc_vm_choice_pick(dinkc_vm_choice_cur());
                        }
                    } else if (have && pl.freeze == 0 && pl.nocontrol == 0 &&
                        pad_just_pressed(prev_buttons, buttons, DINK_PAD_A)) {
                        int slot = talk_probe(&g_scr, g_edg, ned, seqs, pl.x,
                                              pl.y, pl.dir);

                        script_on_talk(slot);
                    }
                    if (have && pl.freeze == 0 && !dinkc_vm_waiting_say() &&
                        !dinkc_vm_waiting_choice() &&
                        pad_just_pressed(prev_buttons, buttons, DINK_PAD_B)) {
                        player_attack(&pl, seqs);
                    }
                    prev_buttons = have ? buttons : 0;
                    dinkc_vm_set_now(now_ms);
                    dinkc_vm_tick(now_ms);
                    dinkc_cmd_thaw_if_idle();
                    now_ms += DINKC_TICK_MS;
                    pdir = have ? pad_dir_from_buttons(buttons) : 0;
                    if (seqs != NULL) {
                        player_step(&pl, pdir, &mask, seqs);
                        if (pl.freeze == 0 && pl.warp_hit > 0 &&
                            screen_try_warp(&g_world, &g_scr, pl.warp_hit,
                                            &player_map, &pl) == 0) {
                            swap = 1;
                            continue;
                        }
                        if (pl.freeze == 0 &&
                            screen_try_cross(&g_world, &player_map, &pl)) {
                            swap = 1;
                            continue;
                        }
                        if (pl.just_hit) {
                            int slot = hit_probe(&g_scr, g_edg, ned, seqs,
                                                 pl.x, pl.y, pl.dir);

                            script_on_hit(slot);
                        }
                        if (pl.seq != last_seq || pl.frame != last_frame) {
                            struct SpriteFrame nxt;

                            memset(&nxt, 0, sizeof(nxt));
                            if (sprite_load_seq_frame(&seqs[pl.seq], pl.seq,
                                                      pl.frame, &nxt) == 0) {
                                sprite_frame_free(&spr);
                                spr = nxt;
                                (void)sprite_upload_pvr(&spr);
                                last_seq = pl.seq;
                                last_frame = pl.frame;
                            }
                        }
                    }
                    pvr_scene_begin();
                    pvr_list_begin(PVR_LIST_OP_POLY);
                    tiles_draw_pvr(&g_atlas);
                    pvr_list_finish();
                    pvr_list_begin(PVR_LIST_PT_POLY);
                    {
                        struct {
                            int rank, x, y, bg;
                            int al, at, ar, ab;
                            struct SpriteFrame *fr;
                        } draw[101];
                        int nd = 0, a, b;

                        if (g_spr_ok != NULL) {
                        memcpy(g_scr.sprite, g_spr_ok, 101u * sizeof(*g_spr_ok));
                    }
                        for (si = 1; si <= 100 && nd < 100; si++) {
                            int seq, fr;
                            struct SpriteFrame *ef;

                            if (!editor_sprite_draw(&g_scr.sprite[si],
                                                    DINK_VISION_DEFAULT)) {
                                continue;
                            }
                            seq = (int)g_scr.sprite[si].seq;
                            fr = (int)g_scr.sprite[si].frame;
                            if (fr < 1) {
                                fr = 1;
                            }
                            ef = edraw_find(g_edg, ned, seq, fr);
                            if (ef == NULL) {
                                continue;
                            }
                            /* que is depth only (screen_rank). x,y stay map. */
                            draw[nd].rank =
                                editor_sprite_rank_y(&g_scr.sprite[si]);
                            draw[nd].x = (int)g_scr.sprite[si].x;
                            draw[nd].y = (int)g_scr.sprite[si].y;
                            draw[nd].bg = (g_scr.sprite[si].type == 0);
                            draw[nd].al = (int)g_scr.sprite[si].alt_l;
                            draw[nd].at = (int)g_scr.sprite[si].alt_t;
                            draw[nd].ar = (int)g_scr.sprite[si].alt_r;
                            draw[nd].ab = (int)g_scr.sprite[si].alt_b;
                            draw[nd].fr = ef;
                            nd++;
                        }
                        if (spr.argb1555 != NULL && nd < 100) {
                            draw[nd].rank = pl.y;
                            draw[nd].x = pl.x;
                            draw[nd].y = pl.y;
                            draw[nd].bg = 0;
                            draw[nd].al = draw[nd].at = 0;
                            draw[nd].ar = draw[nd].ab = 0;
                            draw[nd].fr = &spr;
                            nd++;
                        }
                        for (a = 0; a < nd; a++) {
                            for (b = a + 1; b < nd; b++) {
                                int ba = draw[a].bg ? 0 : 1;
                                int bb = draw[b].bg ? 0 : 1;

                                if (bb < ba ||
                                    (bb == ba && draw[b].rank < draw[a].rank)) {
                                    int tr = draw[a].rank, tx = draw[a].x,
                                        ty = draw[a].y, tb = draw[a].bg;
                                    int tal = draw[a].al, tat = draw[a].at,
                                        tar = draw[a].ar, tab = draw[a].ab;
                                    struct SpriteFrame *tf = draw[a].fr;

                                    draw[a].rank = draw[b].rank;
                                    draw[a].x = draw[b].x;
                                    draw[a].y = draw[b].y;
                                    draw[a].bg = draw[b].bg;
                                    draw[a].al = draw[b].al;
                                    draw[a].at = draw[b].at;
                                    draw[a].ar = draw[b].ar;
                                    draw[a].ab = draw[b].ab;
                                    draw[a].fr = draw[b].fr;
                                    draw[b].rank = tr;
                                    draw[b].x = tx;
                                    draw[b].y = ty;
                                    draw[b].bg = tb;
                                    draw[b].al = tal;
                                    draw[b].at = tat;
                                    draw[b].ar = tar;
                                    draw[b].ab = tab;
                                    draw[b].fr = tf;
                                }
                            }
                        }
                        for (a = 0; a < nd; a++) {
                            sprite_draw_pvr_alt(draw[a].fr, (float)draw[a].x,
                                                (float)draw[a].y,
                                                1.5f + (float)a * 0.01f,
                                                draw[a].al, draw[a].at,
                                                draw[a].ar, draw[a].ab);
                        }
                        saybox_draw_pvr(3.0f);
                        saybox_draw_choices_pvr(3.1f);
                    }
                    pvr_list_finish();
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
