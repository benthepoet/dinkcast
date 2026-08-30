/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_INV_H
#define DINKCAST_INV_H

#include "ini.h"
#include "sprite.h"

#include <stddef.h>
#include <stdint.h>

/* FreeDink inventory.cpp process_item / draw_item. */
#define DINK_INV_SEQ 423
#define DINK_INV_ITEMS 16
#define DINK_INV_MAGIC 8
#define DINK_INV_WEAPON 0
#define DINK_INV_MAGIC_KIND 1
#define DINK_INV_WX 260
#define DINK_INV_WY 83
#define DINK_INV_MX 45
#define DINK_INV_MY 83
#define DINK_INV_DX (18 + 65)
#define DINK_INV_DY (20 + 55)
#define DINK_INV_BG_X 20
#define DINK_INV_BG_Y 0

void inv_cell_xy(int magic, int idx0, int *x, int *y);
int inv_load(struct SeqInfo *seqs);
void inv_free(void);
void inv_reset(void);
void inv_sync_icons(void);
void inv_open(int now_ms);
void inv_close(void);
int inv_showing(void);
int inv_curitem(void);
int inv_item_type(void);
void inv_tick(uint32_t prev, uint32_t buttons, int now_ms);
size_t inv_cpu_bytes(void);
void inv_drop_cpu(void);
struct SpriteFrame *inv_menu_frame(int frame);
struct SpriteFrame *inv_icon_frame(int seq, int frame);

#ifdef _arch_dreamcast
int inv_upload_pvr(void);
void inv_evict_pvr(void);
void inv_draw_pvr(float z);
#endif

#endif
