/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pad.h"

int pad_title_wants_leave(int have_controller, uint32_t buttons)
{
    if (!have_controller) {
        return 0;
    }
    return (buttons & DINK_PAD_LEAVE) != 0;
}

int pad_just_pressed(uint32_t prev, uint32_t now, uint32_t bit)
{
    return (now & bit) != 0 && (prev & bit) == 0;
}

int pad_dir_from_buttons(uint32_t buttons)
{
    int u = (buttons & DINK_PAD_UP) != 0;
    int d = (buttons & DINK_PAD_DOWN) != 0;
    int l = (buttons & DINK_PAD_LEFT) != 0;
    int r = (buttons & DINK_PAD_RIGHT) != 0;

    if (u && l) {
        return 7;
    }
    if (u && r) {
        return 9;
    }
    if (d && l) {
        return 1;
    }
    if (d && r) {
        return 3;
    }
    if (u) {
        return 8;
    }
    if (d) {
        return 2;
    }
    if (l) {
        return 4;
    }
    if (r) {
        return 6;
    }
    return 0;
}

#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

int pad_poll_port0(uint32_t *out_buttons)
{
    maple_device_t *dev;
    cont_state_t *st;
    uint32_t bits = 0;

    if (out_buttons == NULL) {
        return -1;
    }
    *out_buttons = 0;
    /* Plan: Maple port 0 only — not “first controller on the bus”. */
    dev = maple_enum_dev(0, 0);
    if (dev == NULL || !(dev->info.functions & MAPLE_FUNC_CONTROLLER)) {
        return -1;
    }
    st = (cont_state_t *)maple_dev_status(dev);
    if (st == NULL) {
        return -1;
    }
    if (st->buttons & CONT_A) {
        bits |= DINK_PAD_A;
    }
    if (st->buttons & CONT_B) {
        bits |= DINK_PAD_B;
    }
    if (st->buttons & CONT_X) {
        bits |= DINK_PAD_X;
    }
    if (st->buttons & CONT_Y) {
        bits |= DINK_PAD_Y;
    }
    if (st->ltrig > 128) {
        bits |= DINK_PAD_L;
    }
    if (st->buttons & CONT_START) {
        bits |= DINK_PAD_START;
    }
    if (st->buttons & CONT_DPAD_UP) {
        bits |= DINK_PAD_UP;
    }
    if (st->buttons & CONT_DPAD_DOWN) {
        bits |= DINK_PAD_DOWN;
    }
    if (st->buttons & CONT_DPAD_LEFT) {
        bits |= DINK_PAD_LEFT;
    }
    if (st->buttons & CONT_DPAD_RIGHT) {
        bits |= DINK_PAD_RIGHT;
    }
    *out_buttons = bits;
    return 0;
}
#endif
