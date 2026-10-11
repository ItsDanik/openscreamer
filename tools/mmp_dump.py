#!/usr/bin/env python3
"""
mmp_dump.py - extract every candidate object list from MAPn.MMP and test each
against the road centreline.

The cell (the thing the 256x256 grid points at) is a 40-byte header:

    +0x00 ptr   records, 64-byte stride   count at +0x18
    +0x04 ptr   (usually equal to +0x00)
    +0x08 ptr   array,   12-byte stride   count at +0x1C
    +0x0C ptr   array,   28-byte stride   count at +0x20
    +0x10 ptr   ?
    +0x14 int   ?
    +0x18 int   n_records
    +0x1C int   n_12
    +0x20 int   n_28

Only +0x00..+0x10 are relocated by FUN_0004c904, which is how the header size is
known.  FUN_000120c0 is a nearest-object query: it walks a 3x3 block of cells and
measures squared distance using  [+4]=X, [+8]=Y  inside each 12- and 28-byte
element -- that is the ground truth for those two arrays.

This script does NOT guess which array is the scenery.  It measures all three
against the road and lets the numbers decide.

Usage:  python3 tools/mmp_dump.py build/dec/MAP1L/MAP1.MMP build/dec/MAP1/MAP1SLA.SPL
"""

import struct
import sys
from collections import Counter

sys.path.insert(0, "tools")
from check_placement import road_frame, lateral


def load(path):
    b = open(path, "rb").read()

    def i32(o):
        return struct.unpack_from("<i", b, o)[0]

    return b, i32


def grid_cells(b, i32):
    out = []
    for y in range(256):
        row = i32(4 * y)
        if row == -1:
            continue
        for x in range(256):
            c = i32(row + 4 * x)
            if c == -1:
                continue
            out.append((x, y, c))
    return out


def main():
    mmp, sla = sys.argv[1], sys.argv[2]
    b, i32 = load(mmp)
    cells = grid_cells(b, i32)

    def sane(v, lo=-300000, hi=300000):
        return lo < v < hi

    # extract the real layers: layer 0 = the first n0 records, layer 1 = the
    # next n1, both from the one 64-byte-record array at cell+0x28.
    sets = {"layer0": [], "layer1": [], "layer2": [], "layer3": [], "layer4": []}
    print(f"{len(cells)} cells in {mmp}\n")

    for (cx, cy, c) in cells:
        n0, n1 = i32(c + 0x14), i32(c + 0x18)
        for k in range(n0 + n1):
            o = c + 0x28 + 64 * k
            sets["layer1" if k >= n0 else "layer0"].append(
                (cx, cy, i32(o + 0x08), i32(o + 0x10)))
        for name, li, stride in (("layer2", 2, 12), ("layer3", 3, 28)):
            p, n = i32(c + 4 * li), i32(c + 0x14 + 4 * li)
            if p == 0 or not (0 < n < 500) or not (0 < p < len(b)):
                continue
            for k in range(n):
                o = p + stride * k
                if o + stride > len(b):
                    break
                sets[name].append((cx, cy, i32(o + 4), i32(o + 8)))
        p, n = i32(c + 0x10), i32(c + 0x24)
        if p and 0 < n < 500 and 0 < p < len(b):
            sets["layer4"].append((cx, cy, i32(p + 4), i32(p + 8)))

    dense, half = road_frame(sla)
    print(f"road half-width {half:,.0f} units (mean; it varies 2202..2919 "
          f"around track 1)\n")

    for name in ("layer0", "layer1", "layer2", "layer3", "layer4"):
        pts = sets[name]
        bad = 0
        if not pts:
            print(f"{name}: no entries")
            continue
        offs = []
        for (x, y, px, py) in pts:
            if not sane(px) or not sane(py):
                continue
            _, o = lateral(dense, px, py)
            offs.append(abs(o) / half)
        offs.sort()
        inside = sum(1 for v in offs if v < 1.0)
        n = len(offs)
        print(f"{name}: {len(pts)} entries ({bad} bad ptr)")
        if n:
            print(f"   lateral offset in half-widths: min {offs[0]:.2f} "
                  f"p25 {offs[n//4]:.2f} median {offs[n//2]:.2f} "
                  f"p75 {offs[3*n//4]:.2f} max {offs[-1]:.2f}")
            print(f"   {inside}/{n} INSIDE the road "
                  f"({100.0*inside/n:.0f}%)  -> "
                  f"{'plausible' if inside == 0 else 'NOT the scenery list' if inside > n*0.1 else 'mixed'}")
        print()


if __name__ == "__main__":
    main()
