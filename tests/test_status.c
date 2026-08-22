/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dinkc_cmd.h"
#include "dinkc_var.h"
#include "dinkc_vm.h"
#include "fs.h"
#include "ini.h"
#include "inv.h"
#include "pad.h"
#include "status.h"
#include "residency.h"
#include "script.h"
#include "sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    struct SeqInfo *seqs;
    int sl = 0, st = 0, sr = 0, sb = 0, adv = 0, i, yld = 0, rv = 0;
    int args[8];

    expect(status_next_raise(1) == 100, "raise 1");
    expect(status_next_raise(2) == 400, "raise 2");
    expect(status_next_raise(10) == 10000, "raise 10");
    expect(status_next_raise(32) == 99999, "raise cap");
    expect(status_atlas_bytes() == DINK_HUD_ATLAS_BYTES, "128k atlas");
    expect(DINK_HUD_ATLAS_BYTES == 131072, "256x256x2");
    expect((DINK_PAD_L & DINK_PAD_Y) == 0, "L not Y");
    expect((DINK_PAD_L & DINK_PAD_X) == 0, "L not X");

    if (dink_fs_init() != 0) {
        fprintf(stderr, "FAIL no DINK_DATA\n");
        return 1;
    }
    seqs = calloc(DINK_MAX_SEQ, sizeof(*seqs));
    expect(seqs != NULL, "seqs");
    expect(ini_load(seqs, DINK_MAX_SEQ) == 0, "ini");
    expect(status_load(seqs) == 0, "status_load");
    expect(status_glyph(181, 10, &sl, &st, &sr, &sb, &adv) == 0 && adv > 0,
           "exp 0 glyph");
    {
        int area = (sr - sl) * (sb - st);
        int op = status_glyph_opaque_n(181, 10);

        expect(area > 0 && op > area / 2, "exp 0 paper opaque");
    }
    expect(status_glyph(181, 11, &sl, &st, &sr, &sb, &adv) == 0, "exp slash");
    {
        int area = (sr - sl) * (sb - st);
        int op = status_glyph_opaque_n(181, 11);

        expect(area > 0 && op > area / 2, "exp slash paper opaque");
    }
    expect(status_glyph(442, 1, &sl, &st, &sr, &sb, &adv) == 0, "level 1");
    {
        uint16_t px = SPRITE_ARGB1555_OPAQUE;

        expect(status_glyph_argb(181, 10, 0, 0, &px) == 0 &&
                   sprite_pixel_opaque(px),
               "exp 0 corner paper");
        expect(status_glyph_argb(442, 1, 0, 0, &px) == 0 &&
                   !sprite_pixel_opaque(px),
               "level 1 white keyed");
        expect(status_glyph_argb(442, 1, (sr - sl) / 2, (sb - st) / 2, &px) ==
                   0 &&
                   sprite_pixel_opaque(px),
               "level 1 ink");
    }
    expect(status_chrome_opaque_n() > (DINK_HUD_ATLAS * 80) / 2,
           "chrome paper opaque");
    expect(status_glyph(190, 1, &sl, &st, &sr, &sb, &adv) == 0, "lifemax chunk");
    expect(status_glyph(451, 2, &sl, &st, &sr, &sb, &adv) == 0, "life chunk");
    expect(status_glyph(180, 6, &sl, &st, &sr, &sb, &adv) == 0, "magic vert");
    expect(status_cpu_bytes() >= (size_t)DINK_HUD_ATLAS_BYTES, "cpu atlas");
    expect(residency_is_always("graphics/inter/status/dir.ff"), "status always");
    expect(residency_is_always("graphics/inter/numbers/dir.ff"), "numbers always");

    dinkc_vm_reset();
    dinkc_var_init();
    dinkc_cmd_bind_status(status_draw_all, status_show_bmp);
    dinkc_var_set("&magic", 20, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&magic_cost", 100, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&magic_level", 0, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&update_status", 1, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&life", 10, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&lifemax", 10, DINKC_GLOBAL_SCOPE, 1);
    dinkc_var_set("&gold", 80, DINKC_GLOBAL_SCOPE, 1);
    status_draw_all();
    expect(status_fgold() == 80, "draw_all gold");
    dinkc_var_set("&gold", 0, DINKC_GLOBAL_SCOPE, 1);
    for (i = 0; i < 10; i++) {
        status_update(i * 100);
    }
    expect(status_fgold() == 0, "gold tick down");
    for (i = 0; i < 6; i++) {
        status_update(1000 + i * 100);
    }
    expect(dinkc_var_get("&magic_level", DINKC_GLOBAL_SCOPE, 1) == 100,
           "magic fill");

    expect(dinkc_cmd("draw_status", NULL, 0, "", "", &yld, &rv) == 1, "draw known");
    memset(args, 0, sizeof(args));
    args[0] = 1;
    yld = 0;
    expect(dinkc_cmd("show_bmp", args, 1, "tiles/map1.bmp", "", &yld, &rv) == 1,
           "show_bmp known");
    expect(yld == 6 && status_map_active() && status_map_ready(), "yield bmp");
    expect(status_cpu_bytes() > 1000000u, "map cpu while shown");
    status_map_dismiss();
    expect(!status_map_active() && !status_map_ready(), "dismiss");
    expect(status_cpu_bytes() < 400000u, "map cpu gone");

    {
        const char *wait = "void main(void)\n{\nwait(5000);\n}\n";
        int slot;

        expect(DINKC_ENGINE_SPRITE == 1001, "engine id");
        slot = dinkc_vm_start_proc(wait, strlen(wait), 1, "main");
        expect(slot > 0 && dinkc_vm_used(slot), "sprite1 fiber");
        expect(script_on_button(6) == 0, "button6");
        expect(dinkc_vm_used(slot), "L must not kill editor 1");
    }

    status_drop_cpu();
    expect(status_cpu_bytes() == 0, "drop cpu");
    status_free();
    free(seqs);
    printf("ok status hud atlas=%d\n", DINK_HUD_ATLAS_BYTES);
    return 0;
}
