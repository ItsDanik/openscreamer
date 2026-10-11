#!/usr/bin/env python3
"""
render3dx.py - quick-and-dirty viewer for a Screamer .3DX object.

The geometry, as recovered from the binary (see CLAUDE.md):

  * block+0x10 = `a` = vertex-reference count   (bus: 28 = 7 quads x 4 corners)
  * block+0x14 = `b` = face count               (bus: 7)
  * vertices are **int16 xyz triplets**, 6 bytes each
  * the mesh is **non-indexed**: each quad lists its own four corners, so the
    array is simply 4 vertices per face, in face order

This script locates the vertex array heuristically (longest run of plausible
int16 triplets inside the object block) and draws the quads with a painter's
algorithm.  It is a *viewer for confirming the format*, not a port component.
"""

import struct
import sys

from PIL import Image, ImageDraw


def find_vertex_array(buf, start, end):
    """Longest run of int16 triplets with few distinct values (quantised)."""
    best, best_len = None, 0
    i = start
    while i < end - 6:
        vals = []
        j = i
        while j + 6 <= end:
            x, y, z = struct.unpack_from("<3h", buf, j)
            if max(abs(x), abs(y), abs(z)) > 20000:
                break
            vals.append((x, y, z))
            j += 6
        # require quantisation: real meshes reuse a small set of coordinates
        if len(vals) > best_len:
            uniq = len({v for t in vals for v in t})
            if uniq <= max(4, len(vals) // 3):
                best, best_len = i, len(vals)
        i += 2
    return best, best_len


def find_face_table(buf, off, a, b):
    """Locate the face-record table inside an object block.

    Each record is 36 bytes:
        +0x00  int16   (constant per file)
        +0x02  int16   (constant per file)
        +0x04  int16   n   corner count (4 = quad, 3 = triangle)
        +0x06  (n+1) x int16   closed loop of vertex BYTE OFFSETS -- divide by 6
                               for a vertex index; first == last
        remainder: normal / material data

    The per-octant draw-order table at the end of the block holds these records'
    byte offsets from the block start, so the table position can be recovered.
    """
    best = None
    for start in range(off, off + 0x400):
        hits = 0
        for k in range(b):
            r = start + 36 * k
            if r + 8 > len(buf):
                break
            nv = struct.unpack_from("<h", buf, r + 4)[0]
            if not (3 <= nv <= 8):
                break
            ok = True
            for j in range(nv + 1):
                v = struct.unpack_from("<h", buf, r + 6 + 2 * j)[0]
                if v < 0 or v >= a * 6 or v % 6:
                    ok = False
                    break
            if not ok:
                break
            hits += 1
        if best is None or hits > best[0]:
            best = (hits, start)
    return best


def render(path, out):
    buf = open(path, "rb").read()
    nobj = struct.unpack_from("<I", buf, 0x14)[0]

    all_quads = []
    for oi in range(nobj):
        off = struct.unpack_from("<I", buf, 0x18 + 4 * oi)[0]
        a, bb = struct.unpack_from("<2I", buf, off + 0x10)
        voff = struct.unpack_from("<I", buf, off + 0x1C)[0]   # vertex array offset
        if a < 3 or bb < 1 or voff + 6 * a > len(buf):
            continue
        verts = [struct.unpack_from("<3h", buf, voff + 6 * i) for i in range(a)]

        found = find_face_table(buf, off + 0x24, a, bb)
        if not found or found[0] < bb:
            continue
        start = found[1]
        for k in range(bb):
            r = start + 36 * k
            nv = struct.unpack_from("<h", buf, r + 4)[0]
            idx = [struct.unpack_from("<h", buf, r + 6 + 2 * j)[0] // 6
                   for j in range(nv)]
            if len(set(idx)) >= 3:
                all_quads.append([verts[i] for i in idx])

    if not all_quads:
        print("no geometry found")
        return

    xs = [v[0] for q in all_quads for v in q]
    ys = [v[1] for q in all_quads for v in q]
    zs = [v[2] for q in all_quads for v in q]
    cx, cy, cz = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, (min(zs) + max(zs)) / 2
    span = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)) or 1

    W = H = 520
    img = Image.new("RGB", (W, H), (18, 18, 26))
    dr = ImageDraw.Draw(img)

    # isometric-ish projection, y up
    def proj(v):
        x = (v[0] - cx) / span
        y = (v[1] - cy) / span
        z = (v[2] - cz) / span
        px = (x - z) * 0.72
        py = (x + z) * 0.36 - y * 0.95
        return (W / 2 + px * W * 0.75, H / 2 + py * H * 0.75)

    light = (-0.4, 0.8, -0.45)

    def normal(q):
        ax, ay, az = q[1][0] - q[0][0], q[1][1] - q[0][1], q[1][2] - q[0][2]
        bx, by, bz = q[2][0] - q[0][0], q[2][1] - q[0][1], q[2][2] - q[0][2]
        n = (ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx)
        m = (n[0] ** 2 + n[1] ** 2 + n[2] ** 2) ** 0.5 or 1
        return (n[0] / m, n[1] / m, n[2] / m)

    quads = []
    for q in all_quads:
        depth = sum((v[0] - cx) + (v[1] - cy) + (v[2] - cz) for v in q) / 4
        quads.append((depth, q))
    quads.sort(key=lambda t: t[0])

    for depth, q in quads:
        n = normal(q)
        lum = max(0.15, n[0] * light[0] + n[1] * light[1] + n[2] * light[2])
        col = (int(60 + 170 * lum), int(60 + 150 * lum), int(70 + 160 * lum))
        dr.polygon([proj(v) for v in q], fill=col, outline=(200, 220, 255))

    img.save(out)
    print(f"{len(all_quads)} quads from {path} -> {out}")


if __name__ == "__main__":
    render(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "model.png")
