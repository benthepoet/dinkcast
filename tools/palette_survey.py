# SPDX-License-Identifier: GPL-3.0-or-later
"""Survey BMP bit depths and palettes in DINK_DATA (dir.ff packs + loose).

Usage: python3 tools/palette_survey.py "$DINK_DATA"
PAL8 sprites (2026-11-20) rely on one main palette (Tiles/Esplash.bmp).
"""
import sys, struct, collections, hashlib
sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parent))
from pathlib import Path
import pack_catalog as pc

root = Path(sys.argv[1])

def bmp_info(b):
    if len(b) < 54 or b[:2] != b"BM":
        return None
    off = struct.unpack_from("<I", b, 10)[0]
    hsz = struct.unpack_from("<I", b, 14)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    bpp = struct.unpack_from("<H", b, 28)[0]
    comp = struct.unpack_from("<I", b, 30)[0]
    ncol = struct.unpack_from("<I", b, 46)[0] if hsz >= 40 else 0
    pal = b""
    if bpp <= 8:
        n = ncol or (1 << bpp)
        pal = b[14 + hsz: 14 + hsz + 4 * n]
        # normalize: RGB only
        pal = bytes(x for i, x in enumerate(pal) if i % 4 != 3)
    return dict(w=w, h=abs(h), bpp=bpp, comp=comp, pal=pal)

imgs = []  # (where, info)
for ff in root.rglob("*"):
    if ff.is_file() and ff.name.lower() == "dir.ff":
        data = ff.read_bytes()
        ents = pc.ff_entries(data)
        for i, (name, eoff) in enumerate(ents):
            end = ents[i + 1][1] if i + 1 < len(ents) else len(data)
            if not name.lower().endswith(".bmp") or eoff >= len(data):
                continue
            inf = bmp_info(data[eoff:end])
            if inf:
                imgs.append((str(ff.parent.relative_to(root)) + "/" + name, inf))
    elif ff.is_file() and ff.suffix.lower() == ".bmp":
        inf = bmp_info(ff.read_bytes())
        if inf:
            imgs.append((str(ff.relative_to(root)), inf))

print("images", len(imgs))
print("bpp", collections.Counter(i["bpp"] for _, i in imgs))
print("compression", collections.Counter(i["comp"] for _, i in imgs))
pals = collections.Counter(hashlib.md5(i["pal"]).hexdigest() for _, i in imgs if i["bpp"] == 8)
top = pals.most_common(8)
print("distinct 8bpp palettes", len(pals))
for h, c in top:
    ex = next(n for n, i in imgs if i["bpp"] == 8 and hashlib.md5(i["pal"]).hexdigest() == h)
    print(" ", h[:8], c, "e.g.", ex)
# by top-level dir: share of main palette
main = top[0][0]
bydir = collections.defaultdict(lambda: [0, 0])
for n, i in imgs:
    d = "/".join(n.split("/")[:2])
    bydir[d][1] += 1
    if i["bpp"] == 8 and hashlib.md5(i["pal"]).hexdigest() == main:
        bydir[d][0] += 1
print("dirs not fully on main palette:")
for d, (a, t) in sorted(bydir.items()):
    if a != t:
        print(f"  {d}: {a}/{t}")
# non-8bpp examples
for n, i in imgs:
    if i["bpp"] != 8:
        print("  non8", i["bpp"], n)
