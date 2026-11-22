/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hard.h"
#include "mapscr.h"
#include "pad.h"
#include "player.h"
#include "start_map.h"
#include "tiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct HardMask mask;
    struct SeqInfo seqs[DINK_MAX_SEQ];
    struct Player p;
    int i;

    if (pad_dir_from_buttons(0) != 0 ||
        pad_dir_from_buttons(DINK_PAD_LEFT) != 4 ||
        pad_dir_from_buttons(DINK_PAD_UP | DINK_PAD_LEFT) != 7) {
        fprintf(stderr, "FAIL pad dir\n");
        return 1;
    }
    {
        /* FreeDink game_compute_speed: game time is real time; the frame
         * delta is clamped to 12..68 ms and base_timing = max(4, d / 3). */
        struct GameClock c;
        unsigned real = 1000000u;
        int f;

        memset(&c, 0, sizeof(c));
        if (gameclock_tick(&c, real) != 16 || player_base_timing() != 5 ||
            player_walk_speed() != 3) {
            fprintf(stderr, "FAIL clock start\n");
            return 1;
        }
        /* NTSC 59.94 Hz: 1 000 real ms is 1 000 game ms (was 960). */
        for (f = 1; f <= 60; f++) {
            (void)gameclock_tick(&c, real + (unsigned)(f * 1001 / 60));
        }
        if (c.now != 16 + 1001 || player_base_timing() / 4 != 1 ||
            player_walk_speed() != 3) {
            fprintf(stderr, "FAIL clock ntsc now=%d bt=%d\n", c.now,
                    player_base_timing());
            return 1;
        }
        real += 1001;
        /* A 33 ms frame (30 FPS): Dink and changedir speeds double. */
        real += 33;
        if (gameclock_tick(&c, real) != 16 + 1001 + 33 ||
            player_base_timing() != 11 || player_walk_speed() != 6) {
            fprintf(stderr, "FAIL clock 30fps bt=%d sp=%d\n",
                    player_base_timing(), player_walk_speed());
            return 1;
        }
        /* A 900 ms load inside one frame adds only 68 ms of game time. */
        real += 900;
        if (gameclock_tick(&c, real) != 16 + 1001 + 33 + 68 ||
            c.frame_ms != DINK_FRAME_MS_MAX || player_base_timing() != 22) {
            fprintf(stderr, "FAIL clock load now=%d\n", c.now);
            return 1;
        }
        /* FreeDink draw_screen_game resets thisTickCount: after a resync
         * the load is not the next frame's fps_final (no 5x frame). */
        real += 700;
        gameclock_resync(&c, real);
        real += 17;
        if (gameclock_tick(&c, real) != 16 + 1001 + 33 + 68 + 17 ||
            c.added_ms != 17 || player_base_timing() != 5 ||
            player_walk_speed() != 3 || player_frame_game_ms() != 17) {
            fprintf(stderr, "FAIL clock resync bt=%d add=%d\n",
                    player_base_timing(), c.added_ms);
            return 1;
        }
        /* A short frame clamps to 12 for speed (base_timing floor 4). */
        real += 5;
        if (gameclock_tick(&c, real) != 16 + 1001 + 33 + 68 + 17 + 5 ||
            c.frame_ms != DINK_FRAME_MS_MIN || player_base_timing() != 4) {
            fprintf(stderr, "FAIL clock short bt=%d\n", player_base_timing());
            return 1;
        }
        /* The real ms counter wrapping does not jump game time. */
        memset(&c, 0, sizeof(c));
        (void)gameclock_tick(&c, 0xfffffff0u);
        if (gameclock_tick(&c, 0x00000000u) != 16 + 16) {
            fprintf(stderr, "FAIL clock wrap now=%d\n", c.now);
            return 1;
        }
        /* Back to a 16 ms frame for the fixed-step tests below. */
        memset(&c, 0, sizeof(c));
        (void)gameclock_tick(&c, 0);
        if (player_frame_game_ms() != 16 || player_base_timing() != 5) {
            fprintf(stderr, "FAIL clock reset\n");
            return 1;
        }
    }
    memset(&seqs, 0, sizeof(seqs));
    for (i = 1; i < DINK_MAX_SEQ; i++) {
        seqs[i].delay = 50;
        seqs[i].nframes = 2;
        seqs[i].hl = -4;
        seqs[i].ht = -4;
        seqs[i].hr = 4;
        seqs[i].hb = 4;
    }
    memset(&mask, 0, sizeof(mask));
    mask.pix = calloc((size_t)DINK_PLAY_W * DINK_PLAY_H, 1);
    if (mask.pix == NULL) {
        return 1;
    }
    /* Wall at play-local x=200 (screen 220). */
    for (i = 0; i < DINK_PLAY_H; i++) {
        mask.pix[i * DINK_PLAY_W + 200] = 1;
    }
    player_init(&p);
    if (p.frame_delay != 0) {
        fprintf(stderr, "FAIL init frame_delay %d\n", p.frame_delay);
        hard_mask_free(&mask);
        return 1;
    }
    /* FreeDink human_brain: game_choice.active goto freeze. SAVEBOT
     * unfreeze(1) before the 10-slot menu. */
    if (player_walk_pad(2, 0, 0) != 2 || player_walk_pad(2, 1, 0) != 0 ||
        player_walk_pad(2, 0, 1) != 0) {
        fprintf(stderr, "FAIL walk pad choice/freeze\n");
        hard_mask_free(&mask);
        return 1;
    }
    p.x = 100;
    p.y = 100;
    player_step(&p, 6, &mask, seqs, 0, NULL); /* right */
    if (p.x <= 100 || p.dir != 6 || p.seq != DINK_BASE_WALK + 6) {
        fprintf(stderr, "FAIL walk right %d seq %d\n", p.x, p.seq);
        hard_mask_free(&mask);
        return 1;
    }
    /* Origin sample: wall at play 200 = screen 220. Stop at 219. */
    p.x = DINK_PLAY_LEFT + 200 - 2;
    p.y = 100;
    {
        int ox = p.x;
        player_step(&p, 6, &mask, seqs, 0, NULL);
        if (p.x != ox + 1) {
            fprintf(stderr, "FAIL stop at wall x %d from %d\n", p.x, ox);
            hard_mask_free(&mask);
            return 1;
        }
    }
    {
        int ox = p.x;
        player_step(&p, 6, &mask, seqs, 0, NULL);
        if (p.x != ox) {
            fprintf(stderr, "FAIL walk into wall x %d\n", p.x);
            hard_mask_free(&mask);
            return 1;
        }
        if (p.seq != DINK_BASE_WALK + 6) {
            fprintf(stderr, "FAIL still walk against wall\n");
            hard_mask_free(&mask);
            return 1;
        }
    }
    player_step(&p, 0, &mask, seqs, 0, NULL);
    if (p.seq != DINK_BASE_IDLE + p.dir) {
        fprintf(stderr, "FAIL idle seq %d\n", p.seq);
        hard_mask_free(&mask);
        return 1;
    }
    player_init(&p);
    if (p.frame_delay != 0) {
        fprintf(stderr, "FAIL init frame_delay %d\n", p.frame_delay);
        hard_mask_free(&mask);
        return 1;
    }
    p.x = 100;
    p.y = 100;
    player_step(&p, 9, &mask, seqs, 0, NULL); /* up-right: speed-speed/3 = 2 */
    if (p.x != 102 || p.y != 98) {
        fprintf(stderr, "FAIL diag 9 %d,%d\n", p.x, p.y);
        hard_mask_free(&mask);
        return 1;
    }
    player_step(&p, 0, &mask, seqs, 0, NULL);
    if (p.dir != 8 || p.seq != DINK_BASE_IDLE + 8) {
        fprintf(stderr, "FAIL idle snap 9→8 dir=%d seq=%d\n", p.dir, p.seq);
        hard_mask_free(&mask);
        return 1;
    }
    {
        const char *ini =
            "set_frame_frame 14 5 14 3\n"
            "set_frame_frame 14 6 14 2\n";
        if (ini_parse_mem(ini, strlen(ini), seqs, DINK_MAX_SEQ) != 0) {
            fprintf(stderr, "FAIL idle alias parse\n");
            hard_mask_free(&mask);
            return 1;
        }
        for (i = 1; i < DINK_MAX_SEQ; i++) {
            seqs[i].delay = 50;
            seqs[i].nframes = 4;
        }
        player_init(&p);
        p.x = 100;
        p.y = 100;
        p.frame = 4;
        p.acc = 34;
        player_step(&p, 0, &mask, seqs, 0, NULL);
        if (p.frame != 5 || p.seq != DINK_BASE_IDLE + 4) {
            fprintf(stderr, "FAIL idle ping 4->5 got frame %d seq %d\n",
                    p.frame, p.seq);
            hard_mask_free(&mask);
            return 1;
        }
        p.frame = 6;
        p.acc = 34;
        player_step(&p, 0, &mask, seqs, 0, NULL);
        if (p.frame != 1) {
            fprintf(stderr, "FAIL idle ping 6->1 got %d\n", p.frame);
            hard_mask_free(&mask);
            return 1;
        }
        {
            /* FreeDink live_sprite_animate runs on the clock: a 33 ms
             * frame advances Dink's delay by 33, not a fixed 16. */
            struct GameClock c;

            memset(&c, 0, sizeof(c));
            (void)gameclock_tick(&c, 1000);
            (void)gameclock_tick(&c, 1033);
            p.frame = 4;
            p.acc = 20;
            player_step(&p, 0, &mask, seqs, 0, NULL);
            memset(&c, 0, sizeof(c));
            (void)gameclock_tick(&c, 0);
            player_set_base_timing(5);
            if (p.frame != 5) {
                fprintf(stderr, "FAIL dink anim by game ms got %d\n",
                        p.frame);
                hard_mask_free(&mask);
                return 1;
            }
        }
    }
    {
        int i;

        for (i = 0; i < DINK_PLAY_H; i++) {
            mask.pix[i * DINK_PLAY_W] = 7;
        }
        if (hard_get(&mask, 10, 100) != 0) {
            fprintf(stderr, "FAIL oob walkable unlocked\n");
            hard_mask_free(&mask);
            return 1;
        }
        hard_screenlock_set(1);
        if (hard_get(&mask, 10, 100) != 7) {
            fprintf(stderr, "FAIL lock clamps to edge hid %d\n",
                    hard_get(&mask, 10, 100));
            hard_mask_free(&mask);
            return 1;
        }
        hard_screenlock_set(0);
    }
    {
        int ox = p.x, oy = p.y;

        p.freeze = 1;
        player_step(&p, 6, &mask, seqs, 0, NULL);
        if (p.x != ox || p.y != oy) {
            fprintf(stderr, "FAIL freeze moved %d,%d\n", p.x, p.y);
            hard_mask_free(&mask);
            return 1;
        }
    }
    {
        struct MapScreen scr;
        int ox, wed = -1;

        memset(&scr, 0, sizeof(scr));
        scr.sprite[25].is_warp = 1;
        for (i = 0; i < DINK_PLAY_H; i++) {
            mask.pix[i * DINK_PLAY_W + 300] = 125;
        }
        if (hard_get_play(&mask, &scr, DINK_PLAY_LEFT + 300, 200, &wed) != 0 ||
            wed != 25) {
            fprintf(stderr, "FAIL get_hard_play warp %d ed=%d\n",
                    hard_get_play(&mask, &scr, DINK_PLAY_LEFT + 300, 200, &wed),
                    wed);
            hard_mask_free(&mask);
            return 1;
        }
        player_init(&p);
        p.x = DINK_PLAY_LEFT + 300 - 1;
        p.y = 200;
        p.freeze = 1;
        p.move_active = 1;
        p.move_dir = 6;
        p.move_num = DINK_PLAY_LEFT + 400;
        ox = p.x;
        player_step(&p, 0, &mask, seqs, 0, &scr);
        if (p.x <= ox || p.warp_hit != 25) {
            fprintf(stderr, "FAIL fire-door step x=%d from %d warp=%d\n", p.x,
                    ox, p.warp_hit);
            hard_mask_free(&mask);
            return 1;
        }
    }
    {
        /* fill_whole_hard: tiles-only restamp drops live sprite hardness
         * (S1-BRG2 pay-toll barrier after force_vision). */
        struct HardMap hm;
        struct MapScreen tilescr;

        memset(&hm, 0, sizeof(hm));
        memset(&tilescr, 0, sizeof(tilescr));
        hard_stamp_box(&mask, DINK_PLAY_LEFT + 360, 300, -20, -20, 20, 20, 1);
        if (hard_get(&mask, DINK_PLAY_LEFT + 360, 300) == 0) {
            fprintf(stderr, "FAIL stamp barrier hard\n");
            hard_mask_free(&mask);
            return 1;
        }
        if (hard_stamp_tiles(&hm, &tilescr, &mask) != 0) {
            fprintf(stderr, "FAIL fill_whole_hard stamp_tiles\n");
            hard_mask_free(&mask);
            return 1;
        }
        if (hard_get(&mask, DINK_PLAY_LEFT + 360, 300) != 0) {
            fprintf(stderr, "FAIL fill_whole_hard left sprite hard %d\n",
                    hard_get(&mask, DINK_PLAY_LEFT + 360, 300));
            hard_mask_free(&mask);
            return 1;
        }
    }
    {
        /* S1-HOLE: freeze + nocontrol seq 452 + sp_brain(1,0); no_brain
         * holds last crawl. human_brain freeze: would idle. */
        seqs[452].delay = 50;
        seqs[452].nframes = 3;
        player_init(&p);
        p.x = 274;
        p.y = 195;
        p.freeze = 1;
        p.brain = 0;
        p.seq = 452;
        p.frame = 1;
        p.pseq = 452;
        p.pframe = 1;
        p.nocontrol = 1;
        p.acc = 0;
        while (p.nocontrol) {
            player_step(&p, 6, &mask, seqs, 0, NULL);
        }
        if (p.seq != 0 || p.pseq != 452 || p.pframe != 3 ||
            player_pic_seq(&p) != 452 || player_pic_frame(&p) != 3) {
            fprintf(stderr, "FAIL crawl hold seq=%d pseq=%d pfr=%d pic=%d/%d\n",
                    p.seq, p.pseq, p.pframe, player_pic_seq(&p),
                    player_pic_frame(&p));
            hard_mask_free(&mask);
            return 1;
        }
        player_step(&p, 6, &mask, seqs, 0, NULL);
        if (p.seq != 0 || p.x != 274 || player_pic_seq(&p) != 452) {
            fprintf(stderr, "FAIL crawl freeze no idle seq=%d x=%d pic=%d\n",
                    p.seq, p.x, player_pic_seq(&p));
            hard_mask_free(&mask);
            return 1;
        }
        /* DINFO die(): freeze + sp_seq(1,436) + sp_base_idle(1,-1). */
        player_init(&p);
        seqs[436].delay = 16;
        seqs[436].nframes = 4;
        p.freeze = 1;
        p.base_idle = -1;
        p.seq = 436;
        p.frame = 1;
        p.pseq = 436;
        p.pframe = 1;
        p.nocontrol = 0;
        p.acc = 0;
        {
            int steps = 0;

            while (p.seq == 436 && steps < 32) {
                player_step(&p, 6, &mask, seqs, 0, NULL);
                steps++;
            }
            if (p.seq != 0 || p.pseq != 436 || p.pframe != 4 ||
                player_pic_seq(&p) != 436 || player_pic_frame(&p) != 4 ||
                p.x != DINK_START_X) {
                fprintf(stderr,
                        "FAIL die anim seq=%d pseq=%d pfr=%d pic=%d/%d x=%d\n",
                        p.seq, p.pseq, p.pframe, player_pic_seq(&p),
                        player_pic_frame(&p), p.x);
                hard_mask_free(&mask);
                return 1;
            }
        }
        /* ITEM-PIG + freeze(1): throw ends, freeze: idle (Milder talk). */
        player_init(&p);
        p.dir = 6;
        seqs[526].delay = 16;
        seqs[526].nframes = 3;
        p.seq = 526;
        p.frame = 1;
        p.pseq = 526;
        p.pframe = 1;
        p.nocontrol = 1;
        p.freeze = 1;
        p.acc = 0;
        {
            int steps = 0;

            while (p.nocontrol && steps < 32) {
                player_step(&p, 6, &mask, seqs, 0, NULL);
                steps++;
            }
            player_step(&p, 6, &mask, seqs, 0, NULL);
            if (p.seq != DINK_BASE_IDLE + 6 || player_pic_seq(&p) != DINK_BASE_IDLE + 6) {
                fprintf(stderr, "FAIL feed freeze idle seq=%d pic=%d\n", p.seq,
                        player_pic_seq(&p));
                hard_mask_free(&mask);
                return 1;
            }
        }
        /* Punch: same-tick idle after nocontrol (not frozen). */
        player_init(&p);
        p.dir = 8;
        seqs[DINK_BASE_ATTACK + 8].nframes = 2;
        seqs[DINK_BASE_ATTACK + 8].delay = 16;
        player_attack(&p, seqs);
        while (p.nocontrol) {
            player_step(&p, 0, &mask, seqs, 0, NULL);
        }
        if (p.seq != DINK_BASE_IDLE + 8) {
            fprintf(stderr, "FAIL punch idle seq=%d\n", p.seq);
            hard_mask_free(&mask);
            return 1;
        }
    }
    hard_mask_free(&mask);
    printf("OK test_player\n");
    return 0;
}
