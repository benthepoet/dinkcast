#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Rebuild libkallisti.a with iso_read streaming this read, not to EOF.
# Writes build/kos-iso/libkallisti.a and build/kos-iso/env (source it).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/kos-iso"
PY="$ROOT/tools/patch_kos_iso9660.py"

if [ -z "${KOS_BASE:-}" ] || [ ! -f "$KOS_BASE/kernel/arch/dreamcast/fs/fs_iso9660.c" ]; then
    echo "apply_kos_iso_patch: KOS_BASE fs_iso9660.c missing" >&2
    exit 2
fi
if ! command -v kos-cc >/dev/null 2>&1; then
    echo "apply_kos_iso_patch: kos-cc not on PATH" >&2
    exit 2
fi

mkdir -p "$OUT"
SRC="$KOS_BASE/kernel/arch/dreamcast/fs/fs_iso9660.c"
cp "$SRC" "$OUT/fs_iso9660.c"
python3 "$PY" "$OUT/fs_iso9660.c"

STAMP="$OUT/stamp"
SUM=$(wc -c < "$OUT/fs_iso9660.c" | tr -d ' ')
if [ -f "$OUT/libkallisti.a" ] && [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$SUM" ]; then
    echo "apply_kos_iso_patch: reuse $OUT/libkallisti.a"
else
    echo "apply_kos_iso_patch: kos-cc fs_iso9660.c"
    kos-cc -c -o "$OUT/fs_iso9660.o" "$OUT/fs_iso9660.c"
    cp "$KOS_BASE/lib/dreamcast/libkallisti.a" "$OUT/libkallisti.a"
    chmod u+w "$OUT/libkallisti.a"
    AR=${KOS_AR:-sh-elf-ar}
    "$AR" r "$OUT/libkallisti.a" "$OUT/fs_iso9660.o"
    if command -v sh-elf-ranlib >/dev/null 2>&1; then
        sh-elf-ranlib "$OUT/libkallisti.a"
    fi
    echo "$SUM" > "$STAMP"
    echo "apply_kos_iso_patch: wrote $OUT/libkallisti.a"
fi

{
    echo "KOS_LIBS=\"$OUT/libkallisti.a \${KOS_LIBS:-}\""
    echo "export KOS_LIBS"
} > "$OUT/env"
