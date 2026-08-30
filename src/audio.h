/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DINKCAST_AUDIO_H
#define DINKCAST_AUDIO_H

#include <stddef.h>

/* 12.2/12.3: START.c load_sound table into AICA (DC) or host flags.
 * playsound is SoundPlayEffect; returns channel+1, 0 on fail. */
int audio_init(void);
void audio_shutdown(void);
int audio_playsound(int sound, int min, int plus, int sound3d, int repeat);
/* FreeDink special_block: sound 0 → OPEN (7) @ 12000, else that bank @ 22050. */
int audio_warp_sound(int editor_sound);
void audio_halt_owner(int sprite);
void audio_halt_loops(void);
int audio_owner_looping(int sprite);
int audio_slot_loaded(int sound);
size_t audio_sfx_bytes(void);
int audio_slot_count(void);
/* 12.4: one ADPCM/PCM loop via snd_stream. Same name = no reopen. */
int audio_music_play(const char *midi_name);
int audio_music_map(int midi_id);
void audio_music_stop(void);
void audio_music_poll(void);
/* AICA only: drain the PCM ring into snd_stream. No GD-ROM. */
void audio_music_pump(void);
/* During a screen swap, do not fopen/fread the MIDI; keep the current loop. */
void audio_music_disc_hold(int on);
int audio_music_playing(void);

#endif
