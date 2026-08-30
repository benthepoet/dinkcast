#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Render Sound/*.mid to 22050 Hz mono, then Yamaha ADPCM WAV in build/music.
# Never rewrite DINK_DATA. Host-only (fluidsynth + ffmpeg + wav_to_adpcm).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SND=${1:-}
OUT=${2:-"$ROOT/build/music"}
SFDIR=${DINK_SOUNDFONT_DIR:-$HOME/.local/share/soundfonts}
SF2=${DINK_SOUNDFONT:-$SFDIR/FluidR3_GM.sf2}
SF2_GS=${DINK_SOUNDFONT_GS:-$SFDIR/FluidR3_GS.sf2}
ADPCM="$ROOT/build/wav_to_adpcm"

if [ -z "$SND" ] || [ ! -d "$SND" ]; then
    echo "usage: midi_bank.sh SOUNDDIR [OUTDIR]" >&2
    exit 2
fi
if [ ! -x "$ADPCM" ]; then
    echo "midi_bank: missing $ADPCM (make build/wav_to_adpcm)" >&2
    exit 2
fi
if [ ! -f "$SF2" ]; then
    echo "midi_bank: no soundfont ($SF2). Set DINK_SOUNDFONT." >&2
    exit 2
fi
if ! command -v fluidsynth >/dev/null || ! command -v ffmpeg >/dev/null; then
    echo "midi_bank: need fluidsynth and ffmpeg on PATH" >&2
    exit 2
fi
mkdir -p "$OUT"
n=0
fail=0
# Windows Dink is Roland GS. GM-only orchestra-hit/brass (1003.mid) sounds
# like a different cue. Load GS *after* GM when the extra bank exists.
SF_ARGS="$SF2"
if [ -f "$SF2_GS" ] && [ "$SF2_GS" != "$SF2" ]; then
    SF_ARGS="$SF2 $SF2_GS"
fi
echo "midi_bank: soundfonts $SF_ARGS"
for mid in "$SND"/*.mid "$SND"/*.MID; do
    [ -f "$mid" ] || continue
    base=$(basename "$mid")
    stem=${base%.*}
    raw="$OUT/${stem}.raw.wav"
    pcm="$OUT/${stem}.pcm.wav"
    dest="$OUT/${stem}.wav"
    # FluidSynth 2.x: -F must precede the MIDI path (`-F` after the file is illegal).
    # -g 0.4 still hard-clips the title theme after stereo→mono; -R/-C off
    # cuts the 4 s release tail so a loop is the MIDI, not a fade-out.
    if [ -f "$SF2_GS" ] && [ "$SF2_GS" != "$SF2" ]; then
        fs_ok=0
        if fluidsynth -ni -a file -R 0 -C 0 -g 0.4 -r 22050 -F "$raw" \
            "$SF2" "$SF2_GS" "$mid" >/dev/null 2>&1; then
            fs_ok=1
        fi
    else
        fs_ok=0
        if fluidsynth -ni -a file -R 0 -C 0 -g 0.4 -r 22050 -F "$raw" \
            "$SF2" "$mid" >/dev/null 2>&1; then
            fs_ok=1
        fi
    fi
    if [ "$fs_ok" -ne 1 ]; then
        echo "midi_bank: fluidsynth fail $base" >&2
        fail=$((fail + 1))
        continue
    fi
    # Sum stereo (not ffmpeg -ac 1 average, which is −6 dB vs SFX at vol 255).
    if ! ffmpeg -y -loglevel error -i "$raw" -ac 1 -ar 22050 -sample_fmt s16 \
        -af "pan=mono|c0=FL+FR,alimiter=limit=0.89:level=enabled" "$pcm"; then
        echo "midi_bank: ffmpeg fail $base" >&2
        fail=$((fail + 1))
        rm -f "$raw"
        continue
    fi
    if ! "$ADPCM" "$pcm" "$dest"; then
        echo "midi_bank: adpcm fail $base" >&2
        fail=$((fail + 1))
        rm -f "$raw" "$pcm"
        continue
    fi
    rm -f "$raw" "$pcm"
    echo "midi_bank: $base -> $dest"
    n=$((n + 1))
done
echo "midi_bank: converted $n midi ($fail fail) -> $OUT"
exit 0
