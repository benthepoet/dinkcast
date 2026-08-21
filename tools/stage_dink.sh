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
# 14.5: overlay host-built subset dir.ff, then distill in-place on the
# staged copy (never DINK_DATA). Skip fixtures with no dink.dat.
if [ "$HAS_DATA" = 1 ] && [ -d "$ROOT/build/distill" ]; then
    cp -a "$ROOT/build/distill/." "$DST/"
fi
if [ "$HAS_DATA" = 1 ] && command -v python3 >/dev/null 2>&1; then
    python3 "$ROOT/tools/distill_frames.py" --src "$DST" --in-place
fi

# Data names are 8.3-safe; a plain find loop is enough.
find "$DST" -type f -exec sh "$ROOT/tools/pad2048.sh" {} +
echo "stage_dink: $SRC -> $DST (sector-padded)"
