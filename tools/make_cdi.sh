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
# Docker mounts data at /dink; local.mk often has a host path that is empty here.
if [ -z "$DATA" ] || [ ! -d "$DATA" ]; then
    if [ -d /dink ]; then
        DATA=/dink
    fi
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

# mkdcdisc -d DIR puts a folder named basename(DIR) on the disc.
# Staging as build/iso produced /cd/iso (Flycast listing). Pass the
# data tree itself so the folder is "dink" → /cd/dink.
mkdir -p "$(dirname "$OUT")"
set -x
# -I dumps the data track .iso next to the .cdi for tools/make_chd.sh.
"$MKDCDISC" -e "$ELF" -o "$OUT" -d "$DATA" -n DINKCAST -I
set +x
ls -l "$OUT"
echo "OK $OUT (data -> /cd/$(basename "$DATA"))"
