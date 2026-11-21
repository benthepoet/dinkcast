/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bgbake.h"

int bg_stamps_back(const struct EditorSprite *es, int vision)
{
    return es != NULL && es->active && editor_sprite_on_vision(es, vision) &&
           es->type != 1 && es->hard == 0;
}

int bg_bake(struct EditorSprite *rows, int vision, struct BgRow *out, int cap)
{
    int i, n = 0;

    if (rows == NULL || out == NULL) {
        return 0;
    }
    for (i = 1; i <= 100 && n < cap; i++) {
        struct EditorSprite *es = &rows[i];

        if (!es->active || !editor_sprite_on_vision(es, vision) ||
            (es->type != 0 && es->type != 2) || es->is_warp != 0) {
            continue;
        }
        out[n].ed = i;
        out[n].es = *es;
        n++;
        es->active = 0;
    }
    return n;
}
