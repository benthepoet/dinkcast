/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Boot: 640×480. Probe dink.dat. Show official title still (Bite 3.4).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boot.h"
#include "dinkdat.h"
#include "fs.h"
#include "edraw.h"
#include "hard.h"
#include "ini.h"
#include "mapscr.h"
#include "pad.h"
#include "player.h"
#include "sprite.h"
#include "start_map.h"
#include "tiles.h"
#include "title.h"
#include "title_path.h"
#include "world.h"

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/biosfont.h>
#include <dc/cdrom.h>
#include <dirent.h>

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
    }
    {
        struct World world;
        struct MapScreen scr;
        struct TileAtlas atlas;
        int rec;

        if (world_load(&world) != 0) {
            hud("WORLD LOAD FAIL", "dink.dat", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        rec = (int)world.loc[DINK_START_PLAYER_MAP];
        printf("start map %d loc %d music %d\n", DINK_START_PLAYER_MAP, rec,
               (int)world.music[DINK_START_PLAYER_MAP]);
        if (rec < 1 || map_load_record(rec, &scr) != 0) {
            hud("MAP LOAD FAIL", "map.dat", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        memset(&atlas, 0, sizeof(atlas));
        if (tiles_build_atlas(&scr, &atlas) != 0) {
            hud("TILE ATLAS FAIL", "tiles/tsNN.bmp", msg);
            for (;;) {
                vid_waitvbl();
            }
        }
        printf("atlas cells %d\n", atlas.used);
        {
            struct HardMap hard;
            struct SeqInfo *seqs;
            struct SpriteFrame spr;
            int hid;

            memset(&hard, 0, sizeof(hard));
            if (hard_load(&hard) != 0) {
                hud("HARD LOAD FAIL", "hard.dat", msg);
                for (;;) {
                    vid_waitvbl();
                }
            }
            hid = hard_id_for_tile(&hard, scr.t[0].square_full_idx0,
                                   scr.t[0].althard);
            printf("hard tile00 id %d\n", hid);
            seqs = (struct SeqInfo *)calloc(DINK_MAX_SEQ, sizeof(*seqs));
            memset(&spr, 0, sizeof(spr));
            if (seqs != NULL && ini_load(seqs, DINK_MAX_SEQ) == 0) {
                int s;
                for (s = 1; s < DINK_MAX_SEQ; s++) {
                    if (seqs[s].prefix[0] != '\0' &&
                        ((s >= 12 && s <= 18) || (s >= 71 && s <= 79))) {
                        seqs[s].nframes = ini_count_ff_frames(seqs[s].prefix);
                    }
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

                memset(&mask, 0, sizeof(mask));
                if (hard_stamp_tiles(&hard, &scr, &mask) != 0) {
                    hud("HARD STAMP FAIL", "hard.dat", msg);
                    hard_free(&hard);
                    free(seqs);
                    for (;;) {
                        vid_waitvbl();
                    }
                }
                hard_free(&hard); /* stamp done; drop 2 MiB file buffer */
                if (seqs != NULL) {
                    for (si = 1; si <= 100; si++) {
                        int sq = (int)scr.sprite[si].seq;
                        if (!scr.sprite[si].active || sq < 1 ||
                            sq >= DINK_MAX_SEQ || seqs[sq].hr <= seqs[sq].hl) {
                            continue;
                        }
                        hard_stamp_box(&mask, (int)scr.sprite[si].x,
                                       (int)scr.sprite[si].y, seqs[sq].hl,
                                       seqs[sq].ht, seqs[sq].hr, seqs[sq].hb);
                    }
                }
                player_init(&pl);
                if (seqs != NULL) {
                    sprite_load_seq_frame(&seqs[pl.seq], pl.frame, &spr);
                }
                {
                    struct EdGfx edg[DINK_EDGFX_MAX];
                    int ned = 0;

                    memset(edg, 0, sizeof(edg));
                    if (seqs != NULL) {
                        (void)edraw_load_screen(&scr, seqs, edg, &ned);
                    }
                    printf("edraw unique %d\n", ned);
                if (tiles_upload_pvr(&atlas) != 0) {
                    hud("TILE UPLOAD FAIL", NULL, msg);
                    sprite_frame_free(&spr);
                    edraw_free(edg, ned);
                    hard_mask_free(&mask);
                    free(seqs);
                    for (;;) {
                        vid_waitvbl();
                    }
                }
                if (edraw_upload_pvr(edg, ned) != 0) {
                    printf("edraw upload none\n");
                }
                if (spr.argb1555 != NULL) {
                    (void)sprite_upload_pvr(&spr);
                }
                last_seq = pl.seq;
                last_frame = pl.frame;
                printf("play walk %d,%d seq %d\n", pl.x, pl.y, pl.seq);
                for (;;) {
                    uint32_t buttons = 0;
                    int have, pdir;

                    have = (pad_poll_port0(&buttons) == 0);
                    pdir = have ? pad_dir_from_buttons(buttons) : 0;
                    if (seqs != NULL) {
                        player_step(&pl, pdir, &mask, seqs);
                        if (pl.seq != last_seq || pl.frame != last_frame) {
                            sprite_frame_free(&spr);
                            if (sprite_load_seq_frame(&seqs[pl.seq], pl.frame,
                                                      &spr) == 0) {
                                (void)sprite_upload_pvr(&spr);
                                last_seq = pl.seq;
                                last_frame = pl.frame;
                            }
                        }
                    }
                    pvr_wait_ready();
                    pvr_scene_begin();
                    pvr_list_begin(PVR_LIST_OP_POLY);
                    tiles_draw_pvr(&atlas);
                    pvr_list_finish();
                    pvr_list_begin(PVR_LIST_PT_POLY);
                    {
                        struct {
                            int y, x;
                            struct SpriteFrame *fr;
                        } draw[101];
                        int nd = 0, a, b;

                        for (si = 1; si <= 100 && nd < 100; si++) {
                            int seq, fr;
                            struct SpriteFrame *ef;

                            if (!scr.sprite[si].active) {
                                continue;
                            }
                            seq = (int)scr.sprite[si].seq;
                            fr = (int)scr.sprite[si].frame;
                            if (fr < 1) {
                                fr = 1;
                            }
                            ef = edraw_find(edg, ned, seq, fr);
                            if (ef == NULL) {
                                continue;
                            }
                            draw[nd].y = (int)scr.sprite[si].y;
                            draw[nd].x = (int)scr.sprite[si].x;
                            draw[nd].fr = ef;
                            nd++;
                        }
                        if (spr.argb1555 != NULL && nd < 100) {
                            draw[nd].y = pl.y;
                            draw[nd].x = pl.x;
                            draw[nd].fr = &spr;
                            nd++;
                        }
                        for (a = 0; a < nd; a++) {
                            for (b = a + 1; b < nd; b++) {
                                if (draw[b].y < draw[a].y) {
                                    int ty = draw[a].y, tx = draw[a].x;
                                    struct SpriteFrame *tf = draw[a].fr;
                                    draw[a] = draw[b];
                                    draw[b].y = ty;
                                    draw[b].x = tx;
                                    draw[b].fr = tf;
                                }
                            }
                        }
                        for (a = 0; a < nd; a++) {
                            sprite_draw_pvr(draw[a].fr, (float)draw[a].x,
                                            (float)draw[a].y, 2.0f +
                                                                  (float)a * 0.01f);
                        }
                    }
                    pvr_list_finish();
                    pvr_scene_finish();
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
