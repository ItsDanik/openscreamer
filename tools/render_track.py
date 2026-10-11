#!/usr/bin/env python3
"""
render_track.py - plot a Screamer track's splines to a PNG.

.SPL format (recovered from the binary and verified against MAP1CAM.SPL):

    header   8 bytes: [int32 count][int32 0]
    entry   56 bytes:
        +0x00  int32  marker (1)
        +0x04  int32  X
        +0x08  int32  Y
        +0x0C  int32  Z          elevation -- O(1000s) vs X/Y O(100000s)
        +0x10  int32  NEXT       index of the next node; the chain closes on node 0
        +0x14  int32  X          } two more copies of (X, Y) -- presumably the
        +0x18  int32  Y          } spline's in/out handles
        +0x1C  int32  0
        +0x20  int32  X
        +0x24  int32  Y
        +0x28  ...    0

The nodes are NOT in path order.  Walk NEXT.

Usage:
    python3 tools/render_track.py build/dec/MAP1 out.png
"""

import os
import struct
import sys

from PIL import Image, ImageDraw

ROLES = [("CNG", (120, 255, 150)),   # centreline
         ("SLA", (90, 180, 255)),    # racing line
         ("CAM", (255, 90, 90)),     # camera
         ("OPP", (255, 210, 80))]    # opponents


def read_spline(path):
    """Return the nodes in path order, following the NEXT chain."""
    b = open(path, "rb").read()
    if len(b) < 8:
        return []
    n = struct.unpack_from("<I", b, 0)[0]
    nodes = []
    for k in range(n):
        o = 8 + 56 * k
        if o + 20 > len(b):
            break
        x, y, z = struct.unpack_from("<3i", b, o + 4)
        nxt = struct.unpack_from("<i", b, o + 0x10)[0]
        nodes.append((x, y, z, nxt))
    if not nodes:
        return []
    order, seen, k = [], set(), 0
    while k not in seen and 0 <= k < len(nodes):
        seen.add(k)
        order.append(nodes[k])
        k = nodes[k][3]
    return order


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "build/dec/MAP1"
    out = sys.argv[2] if len(sys.argv) > 2 else "build/png/track_layout.png"

    data = {}
    # the directory name carries the track prefix, e.g. .../MAP3 -> MAP3CNG.SPL
    prefix = os.path.basename(os.path.normpath(src)).upper()
    for name, _ in ROLES:
        p = os.path.join(src, f"{prefix}{name}.SPL")
        if os.path.exists(p):
            pts = read_spline(p)
            if pts:
                data[name] = pts
    if not data:
        sys.exit(f"no {prefix}*.SPL found under {src}")

    allp = [p for v in data.values() for p in v]
    mnx, mxx = min(p[0] for p in allp), max(p[0] for p in allp)
    mny, mxy = min(p[1] for p in allp), max(p[1] for p in allp)
    sc = min(860 / (mxx - mnx or 1), 860 / (mxy - mny or 1))

    def proj(p):
        return (20 + (p[0] - mnx) * sc, 880 - (p[1] - mny) * sc)

    W = H = 900
    img = Image.new("RGB", (W, H), (10, 10, 16))
    dr = ImageDraw.Draw(img)
    for name, col in ROLES:
        pts = data.get(name)
        if not pts or len(pts) < 2:
            continue
        dr.line([proj(p) for p in pts] + [proj(pts[0])], fill=col,
                width=3, joint="curve")
        for p in pts:
            x, y = proj(p)
            dr.ellipse([x - 4, y - 4, x + 4, y + 4], fill=col)
    img.save(out)

    print(f"{out}: " + ", ".join(f"{n}={len(data[n])} nodes" for n, _ in ROLES
                                 if n in data))
    print(f"extent {mxx - mnx:,} x {mxy - mny:,} units")


if __name__ == "__main__":
    main()
