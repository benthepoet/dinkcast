#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "patch_kos_iso9660", ROOT / "tools" / "patch_kos_iso9660.py"
)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

SNIP = r"""
            else if(thissect == 2048) {
                req_size = (fd->size - fd->ptr);

                if(req_size & 2047) {
                    req_size = (req_size + 2048) & ~2047;
                }
                if(stream_fd) {
                    iso_abort_stream(false);
                }
                c = cdrom_stream_start(sector + 150, req_size / 2048, true);

                if(c) {
                    goto read_loop;
                }
                fd->stream_part = 0;
                stream_fd = fd;

                toread &= ~31;
                c = cdrom_stream_request(outbuf, toread, 1);
            }
            if(stream_fd == fd) {
                toread &= ~31;
                c = cdrom_stream_request(outbuf, toread, 1);
            }
"""


def main() -> None:
    out = mod.transform(SNIP)
    assert "dinkcast #1492" in out, "marker"
    assert "fd->size - fd->ptr" not in out, "eof size gone"
    assert "toread = req_size;" in out, "drain stream"
    assert out.count("toread &= ~31;") == 1, "continue-stream keeps mask"
    again = mod.transform(out)
    assert again == out, "idempotent"
    print("OK test_kos_iso_patch")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print("FAIL", e, file=sys.stderr)
        sys.exit(1)
