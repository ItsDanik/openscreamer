#!/usr/bin/env python3
"""
mmp_map.py - top-down map of a track with every MAPn.MMP record plotted.

The point is to answer "are the placements right?" by eye.  A perspective render
cannot show this: objects on the far side of the track look the same as objects on
the near side, and a wrong parse can still look plausible.

What is drawn
    grey   road edges            (MAPnSLA.SPL, edge pairs)
    white  road centreline
    green  layer 2 - the racing line (12-byte elements, confirmed by FUN_000120c0)
    red    layer 0 records - the first n0 records of a cell
    amber  layer 1 records - the next n1 records of a cell

Cell header (40 bytes, from FUN_000120c0 / FUN_00082e88):
    base[i] = i32(cell + 4i),  count[i] = i32(cell + 0x14 + 4i)
Layers 0 and 1 share ONE array of 64-byte records starting at cell+40:
    layer 0 = records [0, n0),  layer 1 = records [n0, n0+n1)

Record (64 bytes):
    +0x00 V   object index, 0..191 = the 192 objects in MAPn.UNR
    +0x04 1
    +0x08 X
    +0x0C E   (2048 = normal, negative = on-road class)
    +0x10 Y
    +0x14 0
    +0x18 T
    +0x1C 0
    +0x20 -1  (100% of 646 records - this is what pins the 64-byte stride)

Usage: python3 tools/mmp_map.py build/dec/MAP1L/MAP1.MMP build/dec/MAP1/MAP1SLA.SPL out.png
"""

import struct
import sys

from PIL import Image, ImageDraw

W = 1600


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def cells(b):
    out = []
    for y in range(256):
        row = i32(b, 4 * y)
        if row == -1:
            continue
        for x in range(256):
            c = i32(b, row + 4 * x)
            if c == -1:
                continue
            out.append((x, y, c))
    return out


def spline_nodes(path):
    b = open(path, "rb").read()
    n = struct.unpack_from("<I", b, 0)[0]
    return [struct.unpack_from("<3i", b, 8 + 56 * k + 4) for k in range(n)]


def catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return [p1[k] + 0.5 * ((-p0[k] + p2[k]) * t
                           + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                           + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3)
            for k in range(2)]


def densify(pts, subdiv=16):
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2, p3 = (pts[(i - 1) % n], pts[i], pts[(i + 1) % n],
                          pts[(i + 2) % n])
        for s in range(subdiv):
            out.append(catmull(p0, p1, p2, p3, s / subdiv))
    out.append(out[0])
    return out


def main():
    mmp, sla, out = sys.argv[1], sys.argv[2], sys.argv[3]
    b = open(mmp, "rb").read()
    cs = cells(b)

    E = spline_nodes(sla)
    left = densify([E[i][:2] for i in range(0, len(E), 2)])
    right = densify([E[i][:2] for i in range(1, len(E), 2)])
    centre = [((left[i][0] + right[i][0]) / 2, (left[i][1] + right[i][1]) / 2)
              for i in range(min(len(left), len(right)))]

    race = []
    L0, L1 = [], []
    for (x, y, c) in cs:
        n0, n1 = i32(b, c + 0x14), i32(b, c + 0x18)
        for k in range(n0 + n1):
            o = c + 40 + 64 * k
            rec = (i32(b, o + 8), i32(b, o + 16))
            (L0 if k < n0 else L1).append(rec)
        p2, n2 = i32(b, c + 0x08), i32(b, c + 0x1C)
        for k in range(n2):
            if 0 < p2 < len(b):
                race.append((i32(b, p2 + 12 * k + 4), i32(b, p2 + 12 * k + 8)))

    allpts = left + right + centre + race + L0 + L1
    xs = [p[0] for p in allpts]
    ys = [p[1] for p in allpts]
    minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
    span = max(maxx - minx, maxy - miny) * 1.05
    sc = W / span
    H = int((maxy - miny) * sc) + 80
    ox = (W - (maxx - minx) * sc) / 2 - minx * sc
    oy = (H - (maxy - miny) * sc) / 2 - miny * sc

    img = Image.new("RGB", (W, H), (10, 10, 15))
    d = ImageDraw.Draw(img)

    def P(p):
        return (p[0] * sc + ox, H - (p[1] * sc + oy))

    for a, bb in zip(left, left[1:]):
        d.line([P(a), P(bb)], fill=(70, 70, 80), width=1)
    for a, bb in zip(right, right[1:]):
        d.line([P(a), P(bb)], fill=(70, 70, 80), width=1)
    for a, bb in zip(centre, centre[1:]):
        d.line([P(a), P(bb)], fill=(190, 190, 200), width=1)
    for a, bb in zip(race, race[1:]):
        d.line([P(a), P(bb)], fill=(40, 200, 90), width=1)

    for (x, y) in L1:
        px, py = P((x, y))
        d.ellipse([px - 2, py - 2, px + 2, py + 2], fill=(240, 170, 40))
    for (x, y) in L0:
        px, py = P((x, y))
        d.ellipse([px - 3, py - 3, px + 3, py + 3], outline=(255, 60, 60), width=2)

    d.text((12, 10), f"{mmp}   {len(cs)} cells", fill=(255, 255, 255))
    d.text((12, 26), f"road centreline (white)   racing line (green) {len(race)}",
           fill=(190, 190, 200))
    d.text((12, 42), f"layer0 records {len(L0)} (red ring)   "
                     f"layer1 records {len(L1)} (amber)", fill=(240, 170, 40))

    img.save(out)
    print(f"{out}: {W}x{H}  L0={len(L0)} L1={len(L1)} race={len(race)}")


if __name__ == "__main__":
    main()
