#!/usr/bin/env python3
"""Draw MAPn.MLB's materials as a PNG sheet, the way the game reads them.

Usage:  python3 tools/mlb_dump.py <track> [out.png] [first] [last]

Material record i, 16 bytes at i*16:
    [routine selector, texture record offset, U extent, V extent]
Texture record: [detail levels, ?, data offset of level 0, ...]
Pixels: 8-bit indices into MAPn.COL (6-bit VGA, x4), in a page that is always
256 bytes per row; the material's picture is its (U extent x V extent)
rectangle at the data offset.  See CLAUDE.md, "How the track is built".
"""
import struct, sys, os
from PIL import Image, ImageDraw

track = int(sys.argv[1]) if len(sys.argv) > 1 else 1
out   = sys.argv[2] if len(sys.argv) > 2 else f"build/png/mlb{track}.png"
lo    = int(sys.argv[3]) if len(sys.argv) > 3 else 0
hi    = int(sys.argv[4]) if len(sys.argv) > 4 else 10**6

d = f"build/dec/MAP{track}L"
mlb = open(f"{d}/MAP{track}.MLB", "rb").read()
col = open(f"{d}/MAP{track}.COL", "rb").read()
pal = [(col[3*i]*4, col[3*i+1]*4, col[3*i+2]*4) for i in range(256)]
i32 = lambda o: struct.unpack_from("<i", mlb, o)[0]
KIND = {16: "solid", 64: "cut-out", 224: "see-through", 128: "none"}

tiles, end, i = [], len(mlb), 0
while (i + 1) * 16 <= end:
    kind, ro, su, sv = (i32(i*16 + 4*k) for k in range(4))
    if 0 < ro < len(mlb) - 12:
        end = min(end, ro)
        doff = i32(ro + 8)
        if lo <= i <= hi and 0 < doff < len(mlb):
            w, h = max(1, min(su, 256)), max(1, min(sv, 256))
            im = Image.new("RGB", (w, h), (60, 0, 60))
            for y in range(h):
                row = mlb[doff + y*256: doff + y*256 + w]
                for x, v in enumerate(row):
                    im.putpixel((x, y), pal[v])
            tiles.append((i, kind, su, sv, i32(ro), im))
            print(f"mat {i:3d}  {KIND.get(kind, kind)!s:11}  {su:3d} x {sv:3d}"
                  f"  levels {i32(ro)}  data {doff}")
    i += 1

if not tiles:
    sys.exit("no textures in range")
COLS, W, H = 6, 264, 276
sheet = Image.new("RGB", (COLS*W, ((len(tiles)+COLS-1)//COLS)*H), (30, 30, 30))
dr = ImageDraw.Draw(sheet)
for n, (i, kind, su, sv, lev, im) in enumerate(tiles):
    x, y = (n % COLS)*W, (n//COLS)*H
    sheet.paste(im, (x+4, y+18))
    dr.text((x+4, y+4), f"{i}: {KIND.get(kind, kind)} {su}x{sv}", fill=(255, 255, 0))
os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
sheet.save(out)
print("wrote", out, sheet.size)
