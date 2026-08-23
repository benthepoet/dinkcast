/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_FADE_H
#define DINKCAST_FADE_H

/* FreeDink truecolor: 256 = full, 0 = black. Visual lerp 400 ms. */
#define FADE_FULL 256
#define FADE_MS 400
/* fade_down: cycle_clock = now+1000; script waits the clock, not 400 ms. */
#define FADE_DOWN_YIELD_MS 1000

void fade_reset(void);
void fade_tick(int now_ms);
/* dc_fade_down: ignored during fade_up. Always yield. */
void fade_down_start(int now_ms);
/* dc_fade_up: forces if fade_down is live. */
void fade_up_start(int now_ms);
int fade_busy(void);
int fade_brightness(void);
/* TR black quad. No-op on host. Skip when brightness is full. */
void fade_draw_pvr(void);

#endif
