#!/usr/bin/env python3
"""
check_placement.py - is a candidate placement table actually BESIDE the road?

Written after getting this wrong twice.  Two files looked like object placement
tables and both put objects in the middle of the driving line:

    MAPn.MRK      5 of 10 records inside the road
    MAPnOST.SPL   10 of 45 nodes inside the road

The trap is that the road spline has only ~14 control points with segments
around 30,000 units long, so measuring distance to the nearest CONTROL POINT is
meaningless -- a record can read 6,000 units from every node and still be dead
centre of the road.  You must measure against the RESAMPLED centreline.

Usage:
    python3 tools/check_placement.py build/dec/MAP1/MAP1.MRK
    python3 tools/check_placement.py build/dec/MAP1/MAP1OST.SPL
"""

import math
import struct
import sys


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


def road_frame(sla_path, subdiv=24):
    """Dense centreline plus the mean half-width, from the edge-pair spline."""
    E = spline_nodes(sla_path)
    left = [E[i][:2] for i in range(0, len(E), 2)]
    right = [E[i][:2] for i in range(1, len(E), 2)]
    n = len(left)
    centre = [((left[i][0] + right[i][0]) / 2, (left[i][1] + right[i][1]) / 2)
              for i in range(n)]
    half = sum(math.dist(left[i], right[i]) for i in range(n)) / n / 2
    dense = []
    for i in range(n):
        p0, p1, p2, p3 = (centre[(i - 1) % n], centre[i],
                          centre[(i + 1) % n], centre[(i + 2) % n])
        for s in range(subdiv):
            dense.append(catmull(p0, p1, p2, p3, s / subdiv))
    return dense, half


def lateral(dense, px, py):
    """Signed perpendicular offset from the nearest point on the polyline."""
    best, off = 1e30, 0.0
    for i in range(len(dense)):
        ax, ay = dense[i]
        bx, by = dense[(i + 1) % len(dense)]
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy
        t = 0.0 if L2 == 0 else max(0.0, min(1.0,
            ((px - ax) * dx + (py - ay) * dy) / L2))
        cx, cy = ax + dx * t, ay + dy * t
        d = math.hypot(px - cx, py - cy)
        if d < best:
            best = d
            off = (dx * (py - cy) - dy * (px - cx)) / math.sqrt(L2)
    return best, off


def candidates(path):
    """(x, y) pairs from either a .MRK table or an .SPL."""
    b = open(path, "rb").read()
    if path.upper().endswith(".MRK"):
        n = struct.unpack_from("<I", b, 0)[0]
        return [(struct.unpack_from("<i", b, 4 + 32 * k + 20)[0],
                 struct.unpack_from("<i", b, 4 + 32 * k + 28)[0])
                for k in range(n)]
    return [(p[0], p[1]) for p in spline_nodes(path)]


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cand_path, sla_path = sys.argv[1], sys.argv[2]
    dense, half = road_frame(sla_path)
    pts = candidates(cand_path)

    on = 0
    for x, y in pts:
        _, off = lateral(dense, x, y)
        if abs(off) < half:
            on += 1
    print(f"{cand_path}")
    print(f"  half-width {half:,.0f}, {len(pts)} candidates, "
          f"{on} INSIDE the road")
    print("  " + ("VERDICT: not a placement table" if on else
                  "verdict: plausible, none inside the road"))
    if on:
        sys.exit(1)


if __name__ == "__main__":
    main()
