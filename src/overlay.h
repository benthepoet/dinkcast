/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_OVERLAY_H
#define DINKCAST_OVERLAY_H

#include "sprite.h"

#include <stddef.h>

/* Big UI overlays (choice box seq 30, inventory menu seq 423) hold VRAM
 * only while shown (plan §1.2). State per overlay:
 *   OFF    -> want: load CPU, upload each frame making room, ON
 *              (any failure: free everything, FAILED, log once)
 *   ON     -> !want: free, OFF
 *   FAILED -> !want: OFF (retry on the next open, never per tick)
 * overlay_sync runs before pvr_scene_begin: the last scene finished at the
 * top of the tick, so freeing and making room are safe. */
enum { OVERLAY_OFF = 0, OVERLAY_ON = 1, OVERLAY_FAILED = 2 };

struct OverlayOps {
    int (*load)(void); /* CPU decode; 0 = every essential frame present */
    void (*release)(void); /* free every frame (CPU and texture) */
    int (*nframes)(void);
    struct SpriteFrame *(*frame)(int i); /* 0..nframes-1, NULL = unused */
};

struct Overlay {
    const char *name;
    const struct OverlayOps *ops;
    int state;
};

/* VRAM making room, same order as edraw (#155): Dink's other seqs for the
 * shortfall, then non-live Screen textures one at a time (fragmentation:
 * total free is not the largest block). Hooks are injectable for tests. */
struct OverlayVram {
    int (*upload)(struct SpriteFrame *f); /* 0 ok */
    size_t (*avail)(void);
    size_t (*screen_evictable)(void);
    size_t (*screen_free_one)(void);
    size_t (*dink_trim_for)(size_t extra);
    size_t (*dink_free_one)(void); /* fragmented: one Dink frame */
    void (*pump)(void); /* AICA/RAM only between uploads; NULL ok */
};
void overlay_set_vram(const struct OverlayVram *v);

/* Upload f (drops CPU on success). 0 ok, -1 refused (f untouched). */
int overlay_upload_with_room(struct SpriteFrame *f);

void overlay_sync_one(struct Overlay *o, int want);

#endif
