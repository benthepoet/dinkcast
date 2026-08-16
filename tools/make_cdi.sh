#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Stage /cd/dink and build a selfboot CDI. Requires mkdcdisc + ELF + DINK_DATA.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ELF=${1:-"$ROOT/build/dinkcast.elf"}
OUT=${2:-"$ROOT/build/dinkcast.cdi"}
DATA=${DINK_DATA:-}

if [ ! -f "$ELF" ]; then
    echo "make cdi: missing $ELF (run: source \$KOS_BASE/environ.sh && make dc)" >&2
    exit 2
fi
if [ -z "$DATA" ] || [ ! -d "$DATA" ]; then
    echo "make cdi: set DINK_DATA to the inner dink/ tree (has Dink.dat)" >&2
    exit 2
fi
if [ ! -e "$DATA/Dink.dat" ] && [ ! -e "$DATA/dink.dat" ]; then
    echo "make cdi: $DATA has no Dink.dat" >&2
    exit 2
fi

MKDCDISC=${MKDCDISC:-}
if [ -z "$MKDCDISC" ]; then
    if command -v mkdcdisc >/dev/null 2>&1; then
        MKDCDISC=$(command -v mkdcdisc)
    elif [ -x /opt/toolchains/dc/mkdcdisc/builddir/mkdcdisc ]; then
        MKDCDISC=/opt/toolchains/dc/mkdcdisc/builddir/mkdcdisc
    else
        echo "make cdi: mkdcdisc not found. See docs/TOOLCHAIN.md" >&2
        exit 2
    fi
fi

ISO="$ROOT/build/iso"
rm -rf "$ISO"
mkdir -p "$ISO"
# Disc path /cd/dink/... — must be a directory named dink, not the tarball root.
ln -sfn "$DATA" "$ISO/dink"

mkdir -p "$(dirname "$OUT")"
# -d adds the staging dir; flags match current mkdcdisc (elf + extra files).
set -x
"$MKDCDISC" -e "$ELF" -o "$OUT" -d "$ISO" -n DINKCAST
set +x
ls -l "$OUT"
echo "OK $OUT (data -> /cd/dink)"
