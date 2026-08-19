#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# GD-ROM CHD for Redream: 3 tracks, MODE1/2352, data at LBA 45000.
# Not the Flycast `make emu` image (that is a 2-track MIL-CD CUE CHD).
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ISO=${1:-"$ROOT/build/dinkcast.iso"}
OUT=${2:-"$ROOT/build/dinkcast-redream.chd"}
CHDMAN=${CHDMAN:-chdman}
PYTHON=${PYTHON:-python3}

if [ ! -f "$ISO" ]; then
    echo "make chd-redream: missing $ISO (rebuild with make cdi)" >&2
    exit 2
fi
if ! command -v "$CHDMAN" >/dev/null 2>&1; then
    echo "make chd-redream: chdman not found. Arch: pacman -S mame-tools" >&2
    exit 2
fi

DIR=$(dirname "$OUT")
STAGE="$DIR/redream-gdi"
mkdir -p "$STAGE"

echo "make chd-redream: rebase ISO to LBA 0 + MODE1/2352 on track 1"
"$PYTHON" "$ROOT/tools/gdrom_from_msiso.py" "$ISO" "$STAGE"

GDI="$STAGE/dinkcast-redream.gdi"
set -x
"$CHDMAN" createcd -f -i "$GDI" -o "$OUT"
set +x

INFO=$("$CHDMAN" info -i "$OUT")
echo "$INFO"
case "$INFO" in
    *"Input tracks: 0"*|*"Tracks:          0"*)
        echo "make chd-redream: CHD has 0 tracks" >&2
        exit 1
        ;;
esac
# Redream wants GD-ROM metadata (CHGD), not MIL-CD CHT2.
case "$INFO" in
    *"Tag='CHGD'"*) ;;
    *)
        echo "make chd-redream: expected CHGD (GD-ROM) metadata" >&2
        exit 1
        ;;
esac
ls -l "$OUT"
echo "OK $OUT"
echo "Open this in Redream. Flycast make emu still uses build/dinkcast.chd"
