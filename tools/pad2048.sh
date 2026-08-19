#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Pad files to a 2048-byte multiple. KOS fs_iso9660 DMA-streams a
# whole-file read of the sector-rounded size; a file whose size is not a
# 2048 multiple leaves a live GD-ROM stream aborted with data queued and
# can wedge the drive (KOS issue #1492, docs/CD-HANG-ROOTCAUSE.md).
# Text (.c/.ini/.txt) is padded with spaces — the DinkC lexer treats NUL
# as junk before EOF — everything else with zeroes.
set -eu

BLOCK_DIR=""
cleanup() { [ -z "$BLOCK_DIR" ] || rm -rf "$BLOCK_DIR"; }
trap cleanup EXIT

blocks()
{
    if [ -z "$BLOCK_DIR" ]; then
        BLOCK_DIR=$(mktemp -d)
        head -c 2048 /dev/zero > "$BLOCK_DIR/zero"
        printf '%2048s' '' > "$BLOCK_DIR/space"
    fi
}

if [ "$#" -eq 0 ]; then
    echo "usage: pad2048.sh FILE [FILE...]" >&2
    exit 2
fi

padded=0
for f do
    [ -f "$f" ] || continue
    size=$(stat -c %s "$f")
    pad=$(( (2048 - size % 2048) % 2048 ))
    [ "$pad" -eq 0 ] && continue
    blocks
    case $(basename "$f" | tr '[:upper:]' '[:lower:]') in
        *.c | *.ini | *.txt) blk="$BLOCK_DIR/space" ;;
        *) blk="$BLOCK_DIR/zero" ;;
    esac
    dd if="$blk" of="$f" bs="$pad" count=1 conv=notrunc oflag=append 2>/dev/null
    padded=$((padded + 1))
done
echo "pad2048: $padded file(s) padded"
