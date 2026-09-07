#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Patch KOS fs_iso9660.c iso_read: DMA stream this request, not to EOF.

KallistiOS #1492 / Dinkcast 14.6: cdrom_stream_start used (fd->size - ptr)
rounded to 2048, then fread pulled one BMP/rec. The leftover never drained,
so the next SEEK_SET aborted a live GD-ROM stream (bar cup, hard.dat, trees).
"""
from __future__ import annotations

import sys

MARKER = "dinkcast #1492"
OLD = "req_size = (fd->size - fd->ptr);"
NEW_BLOCK = """                /* dinkcast #1492: stream only complete sectors of THIS
                 * read, not to EOF. Tail of the request uses bdread. */
                req_size = toread & ~(size_t)2047;
                if(req_size == 0)
                    goto read_loop;"""


def transform(src: str) -> str:
    if MARKER in src:
        return src
    if OLD not in src:
        raise SystemExit("patch_kos_iso9660: needle not found (KOS iso_read changed)")
    if src.count(OLD) != 1:
        raise SystemExit("patch_kos_iso9660: needle not unique")
    out = src.replace(OLD, NEW_BLOCK, 1)
    # Consume the whole stream so remain_size hits 0 (no abort with data queued).
    old_req = "                toread &= ~31;\n                c = cdrom_stream_request(outbuf, toread, 1);"
    new_req = "                toread = req_size;\n                c = cdrom_stream_request(outbuf, toread, 1);"
    # Only the stream-start path (second occurrence is the continue-stream path).
    first = out.find(old_req)
    if first < 0:
        raise SystemExit("patch_kos_iso9660: stream_request needle not found")
    second = out.find(old_req, first + 1)
    if second < 0:
        raise SystemExit("patch_kos_iso9660: expected two stream_request sites")
    # Continue-stream keeps toread &= ~31; replace only the start-stream site
    # (the one after cdrom_stream_start).
    start = out.find("cdrom_stream_start(sector + 150, req_size / 2048, true);")
    if start < 0:
        raise SystemExit("patch_kos_iso9660: stream_start needle not found")
    site = out.find(old_req, start)
    if site < 0:
        raise SystemExit("patch_kos_iso9660: stream_request after start not found")
    out = out[:site] + new_req + out[site + len(old_req) :]
    return out


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("usage: patch_kos_iso9660.py fs_iso9660.c")
    path = sys.argv[1]
    with open(path, encoding="utf-8", errors="replace") as f:
        src = f.read()
    out = transform(src)
    if out == src:
        return
    with open(path, "w", encoding="utf-8") as f:
        f.write(out)


if __name__ == "__main__":
    main()
