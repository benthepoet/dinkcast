/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_AUDIO_H
#define DINKCAST_AUDIO_H

#include <stddef.h>

/* 12.2/12.3: START.c load_sound table into AICA (DC) or host flags.
 * playsound is SoundPlayEffect; returns channel+1, 0 on fail. */
int audio_init(void);
void audio_shutdown(void);
int audio_playsound(int sound, int min, int plus, int sound3d, int repeat);
int audio_slot_loaded(int sound);
size_t audio_sfx_bytes(void);
int audio_slot_count(void);

#endif
