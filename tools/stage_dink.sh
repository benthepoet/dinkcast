#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Stage DINK_DATA for the disc image: copy the tree, then pad every file
# to a 2048-byte multiple (see tools/pad2048.sh and
# docs/CD-HANG-ROOTCAUSE.md). Never pads the source tree in place — the
# Docker build mounts DINK_DATA read-only.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SRC=${1:-}
DST=${2:-}

if [ -z "$SRC" ] || [ -z "$DST" ] || [ ! -d "$SRC" ]; then
    echo "usage: stage_dink.sh SRCDIR DSTDIR" >&2
    exit 2
fi
SRC_REAL=$(CDPATH= cd -- "$SRC" && pwd -P)
DST_REAL=$(readlink -m "$DST")
case $DST_REAL in
    "$SRC_REAL" | "$SRC_REAL"/*)
        echo "stage_dink: DST must not be SRC or inside SRC" >&2
        exit 2
        ;;
esac
case $SRC_REAL/ in
    "$DST_REAL"/*)
        echo "stage_dink: SRC must not be inside DST (rm -rf would take it)" >&2
        exit 2
        ;;
esac

rm -rf "$DST"
mkdir -p "$DST"
cp -a "$SRC/." "$DST/"
# cp -a keeps source modes; CD-ripped data is often read-only, and the
# pad step must be able to append.
chmod -R u+w "$DST"

HAS_DATA=0
if [ -e "$DST/dink.dat" ] || [ -e "$DST/Dink.dat" ]; then
    HAS_DATA=1
fi
# 14.5: distill in-place on the official staged copy (never DINK_DATA).
# Do not overlay build/distill first — a previous sparse pack would replace
# a skip-no-save original (innwalls nframes 0). Skip fixtures with no dink.dat.
if [ "$HAS_DATA" = 1 ] && command -v python3 >/dev/null 2>&1; then
    python3 "$ROOT/tools/distill_frames.py" --src "$DST" --in-place
fi

# 12.1: overlay host-converted AICA WAVs from build/sfx (never rewrite
# DINK_DATA). Do not run wav_to_adpcm in the KOS image — host glibc
# binaries can fail there, and converting huge stereo clips in-container
# has segfaulted. `make docker-cdi` runs `make sfx-bank` on the host first.
SND_DIR=""
if [ -d "$DST/Sound" ]; then
    SND_DIR="$DST/Sound"
elif [ -d "$DST/sound" ]; then
    SND_DIR="$DST/sound"
fi
SFX_OVER="$ROOT/build/sfx"
if [ "$HAS_DATA" = 1 ] && [ -n "$SND_DIR" ] && [ -d "$SFX_OVER" ]; then
    n=0
    for src in "$SFX_OVER"/*; do
        [ -f "$src" ] || continue
        base=$(basename "$src")
        # Official 1.08 has slots GNU freedink-data omitted (hurt1, pigs, …).
        # Always copy into the staged Sound/ dir, even if the source tree
        # had no file of that name.
        dest="$SND_DIR/$base"
        for cand in "$SND_DIR/$base" "$SND_DIR/$(echo "$base" | tr 'A-Z' 'a-z')" \
                    "$SND_DIR/$(echo "$base" | tr 'a-z' 'A-Z')"; do
            if [ -f "$cand" ]; then
                dest=$cand
                break
            fi
        done
        cp -f "$src" "$dest"
        n=$((n + 1))
    done
    echo "stage_dink: overlay $n sfx from $SFX_OVER"
fi

# Data names are 8.3-safe; a plain find loop is enough.
find "$DST" -type f -exec sh "$ROOT/tools/pad2048.sh" {} +
echo "stage_dink: $SRC -> $DST (sector-padded)"
