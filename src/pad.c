/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pad.h"

int pad_title_wants_leave(int have_controller, uint32_t buttons)
{
    if (!have_controller) {
        return 0;
    }
    return (buttons & DINK_PAD_LEAVE) != 0;
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
    if (st->buttons & CONT_START) {
        bits |= DINK_PAD_START;
    }
    *out_buttons = bits;
    return 0;
}
#endif
