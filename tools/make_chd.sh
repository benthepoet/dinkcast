#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Wrap mkdcdisc's data-track ISO as a two-track CUE and compress to CHD.
# Do NOT use a GDI: chdman then emits GD-ROM metadata (CHGD) and Flycast
# rejects it ("Invalid CHD: less than 3 tracks"). A CUE is a MIL-CD CHD.
# Hardware still uses the .cdi.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ISO=${1:-"$ROOT/build/dinkcast.iso"}
OUT=${2:-"$ROOT/build/dinkcast.chd"}
CHDMAN=${CHDMAN:-chdman}

# mkdcdisc blank audio when no -c: 2352*302 (4 s). Flycast MIL-CD then
# inserts SESSION_GAP 11400 + 150 lead-in → data FAD 11852 = LBA 11702.
MKDCDISC_AUDIO_SECTORS=302

if [ ! -f "$ISO" ]; then
    echo "make chd: missing $ISO (rebuild with make cdi; mkdcdisc -I dumps the iso)" >&2
    exit 2
fi
if ! command -v "$CHDMAN" >/dev/null 2>&1; then
    echo "make chd: chdman not found (CHDMAN=$CHDMAN)." >&2
    echo "  Arch/CachyOS: pacman -S mame-tools" >&2
    echo "  Then: make chd" >&2
    exit 2
fi

DIR=$(dirname "$OUT")
mkdir -p "$DIR"
BASE=$(basename "$OUT" .chd)
AUDIO="$DIR/${BASE}-track01.bin"
CUE="$DIR/${BASE}.cue"
ISO_LINK="$DIR/${BASE}.iso"

if [ "$(CDPATH= cd -- "$(dirname "$ISO")" && pwd)" != "$(CDPATH= cd -- "$DIR" && pwd)" ] ||
    [ "$(basename "$ISO")" != "$(basename "$ISO_LINK")" ]; then
    ln -sf "$ISO" "$ISO_LINK"
    ISO_NAME=$(basename "$ISO_LINK")
else
    ISO_NAME=$(basename "$ISO")
fi
AUDIO_NAME=$(basename "$AUDIO")

dd if=/dev/zero of="$AUDIO" bs=2352 count="$MKDCDISC_AUDIO_SECTORS" status=none

{
    echo "FILE \"$AUDIO_NAME\" BINARY"
    echo "  TRACK 01 AUDIO"
    echo "    INDEX 01 00:00:00"
    echo "FILE \"$ISO_NAME\" BINARY"
    echo "  TRACK 02 MODE1/2048"
    echo "    INDEX 01 00:00:00"
} > "$CUE"

echo "make chd: cue MIL-CD audio ${MKDCDISC_AUDIO_SECTORS} sectors + iso"
set -x
"$CHDMAN" createcd -f -i "$CUE" -o "$OUT"
set +x

INFO=$("$CHDMAN" info -i "$OUT")
echo "$INFO"
case "$INFO" in
    *"Input tracks: 0"*|*"Tracks:          0"*)
        echo "make chd: CHD has 0 tracks" >&2
        exit 1
        ;;
    *"Tag='CHGD'"*)
        echo "make chd: CHD has GD-ROM metadata; Flycast needs a MIL-CD CUE" >&2
        exit 1
        ;;
esac
ls -l "$OUT"
echo "OK $OUT"
