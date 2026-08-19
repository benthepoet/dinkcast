#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Wrap mkdcdisc's data-track ISO as a GDI and compress to CHD for Flycast.
# chdman cannot parse DiscJuggler .cdi (0 tracks). Hardware still uses the .cdi.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ISO=${1:-"$ROOT/build/dinkcast.iso"}
OUT=${2:-"$ROOT/build/dinkcast.chd"}
CHDMAN=${CHDMAN:-chdman}

# mkdcdisc src/main.cpp: 4s blank audio (2352*302) when no -c CDDA.
# disc_image.c first session: LEAD_IN 4500 + LEAD_OUT 6750 + pregap 150.
MKDCDISC_AUDIO_SECTORS=302
MKDCDISC_DATA_LBA=$((4500 + 6750 + 150 + MKDCDISC_AUDIO_SECTORS))

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
GDI="$DIR/${BASE}.gdi"
ISO_LINK="$DIR/${BASE}.iso"

# chdman resolves names relative to the .gdi. Keep all three in DIR.
if [ "$(CDPATH= cd -- "$(dirname "$ISO")" && pwd)" != "$(CDPATH= cd -- "$DIR" && pwd)" ] ||
    [ "$(basename "$ISO")" != "$(basename "$ISO_LINK")" ]; then
    ln -sf "$ISO" "$ISO_LINK"
    ISO_NAME=$(basename "$ISO_LINK")
else
    ISO_NAME=$(basename "$ISO")
fi
AUDIO_NAME=$(basename "$AUDIO")

dd if=/dev/zero of="$AUDIO" bs=2352 count="$MKDCDISC_AUDIO_SECTORS" status=none

# GDI: track type 0 = audio, 4 = data. Data LBA matches mkdcdisc ms_block.
{
    echo 2
    echo "1 0 0 2352 $AUDIO_NAME 0"
    echo "2 $MKDCDISC_DATA_LBA 4 2048 $ISO_NAME 0"
} > "$GDI"

echo "make chd: gdi data LBA $MKDCDISC_DATA_LBA (mkdcdisc session0)"
set -x
"$CHDMAN" createcd -f -i "$GDI" -o "$OUT"
set +x

INFO=$("$CHDMAN" info -i "$OUT")
echo "$INFO"
case "$INFO" in
    *"Input tracks: 0"*|*"Tracks:          0"*)
        echo "make chd: CHD has 0 tracks" >&2
        exit 1
        ;;
esac
ls -l "$OUT"
echo "OK $OUT"
