/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "overlay.h"

#include <stdio.h>

static struct OverlayVram g_v;

void overlay_set_vram(const struct OverlayVram *v)
{
    if (v != NULL) {
        g_v = *v;
    }
}

static size_t sat_add(size_t a, size_t b)
{
    return a > (size_t)-1 - b ? (size_t)-1 : a + b;
}

static size_t call0(size_t (*fn)(void))
{
    return fn != NULL ? fn() : 0u;
}

int overlay_upload_with_room(struct SpriteFrame *f)
{
    size_t need, avail, ev;
    int dink_asked = 0, tries;

    if (f == NULL) {
        return -1;
    }
    if (f->tex != NULL) {
        return 0;
    }
    if (f->argb1555 == NULL || g_v.upload == NULL) {
        return -1;
    }
    need = (size_t)f->tw * (size_t)f->th * 2u;
    for (tries = 0; tries < 256; tries++) {
        if (g_v.upload(f) == 0) {
            sprite_drop_cpu(f);
            return 0;
        }
        avail = g_v.avail != NULL ? g_v.avail() : 0u;
        ev = call0(g_v.screen_evictable);
        if (!dink_asked) {
            dink_asked = 1;
            /* Only the part the screen cannot give. */
            if (sat_add(avail, ev) < need && g_v.dink_trim_for != NULL &&
                g_v.dink_trim_for(need - sat_add(avail, ev)) > 0) {
                continue;
            }
        }
        if (sat_add(avail, ev) < need) {
            return -1; /* would empty the screen and still not fit */
        }
        if (call0(g_v.screen_free_one) == 0) {
            return -1;
        }
    }
    return -1;
}

static void fail(struct Overlay *o, const char *why)
{
    o->ops->release();
    o->state = OVERLAY_FAILED;
    printf("overlay %s fail (%s)\n", o->name, why);
}

static int any_loaded(struct Overlay *o)
{
    int i, n = o->ops->nframes();

    for (i = 0; i < n; i++) {
        struct SpriteFrame *f = o->ops->frame(i);

        if (f != NULL && (f->tex != NULL || f->argb1555 != NULL)) {
            return 1;
        }
    }
    return 0;
}

void overlay_sync_one(struct Overlay *o, int want)
{
    int i, n;

    if (o == NULL || o->ops == NULL) {
        return;
    }
    if (!want) {
        /* Also frees art another screen loaded (start menu Continue). */
        if (o->state == OVERLAY_ON || any_loaded(o)) {
            o->ops->release();
            printf("overlay %s close\n", o->name);
        }
        o->state = OVERLAY_OFF;
        return;
    }
    if (o->state != OVERLAY_OFF) {
        return; /* ON, or FAILED until closed */
    }
    if (o->ops->load() != 0) {
        fail(o, "load");
        return;
    }
    n = o->ops->nframes();
    for (i = 0; i < n; i++) {
        struct SpriteFrame *f = o->ops->frame(i);

        if (f == NULL || (f->tex == NULL && f->argb1555 == NULL)) {
            continue;
        }
        if (overlay_upload_with_room(f) != 0) {
            fail(o, "vram");
            return;
        }
    }
    o->state = OVERLAY_ON;
    printf("overlay %s open\n", o->name);
}
