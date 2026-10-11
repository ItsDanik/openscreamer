#!/usr/bin/env python3
"""
unle.py - extract a loadable flat image from the DOS/4GW-bound LE
executable STARTL.EXE (Screamer, Graffiti/Virgin 1995).

Why this exists
---------------
STARTL.EXE is an MZ stub (the DOS/4GW real-mode loader, 10,832 bytes)
followed by an LE ("Linear Executable") image.  Ghidra 12.x has no
LE/LX loader, and its MZ loader stops at the end of the stub and
reports the remaining ~1 MB as "extra bytes" -- so a plain import
decompiles nothing.

The LE image itself is already position-correct: addresses inside it
are absolute linear VAs (verified: a pointer in the startup code at VA
0x30121 resolves to executable code at VA 0x27EB4).  So no relocation
pass is needed -- we only have to undo the page packing.

Usage
-----
    python3 tools/unle.py STARTL.EXE -o build/screamer_flat.bin

Then import the flat image into Ghidra as raw x86:LE:32:
    analyzeHeadless <proj> <name> -import build/screamer_flat.bin \
        -processor x86:LE:32:default -loader BinaryLoader \
        -loader-baseAddr 0x0 -noanalysis
and create a function at the entry VA it prints.
"""

import argparse
import json
import struct
import sys
from pathlib import Path


def parse_le(data: bytes):
    if data[:2] != b"MZ":
        sys.exit("not an MZ executable")

    le_off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[le_off:le_off + 2] != b"LE":
        sys.exit(f"no LE signature at 0x{le_off:X} (DOS/4GW stub not present?)")

    def u32(o):  # LE header fields are relative to the LE header start
        return struct.unpack_from("<I", data, le_off + o)[0]

    hdr = {
        "stub_size":    le_off,
        "le_offset":    le_off,
        "format_level": u32(0x08),
        "n_pages":      u32(0x14),
        "eip_obj":      u32(0x18),
        "eip":          u32(0x1C),
        "esp_obj":      u32(0x20),
        "esp":          u32(0x24),
        "page_size":    u32(0x28),
        "last_page":    u32(0x2C),
        "objtab":       u32(0x40),
        "objcnt":       u32(0x44),
        "pagemap":      u32(0x48),
        "datapages":    u32(0x80),
    }

    # sanity: the data pages must exactly fill the rest of the file
    expect = (hdr["datapages"]
              + (hdr["n_pages"] - 1) * hdr["page_size"]
              + hdr["last_page"])
    if expect != len(data):
        print(f"WARNING: page layout implies {expect} bytes, file has {len(data)}",
              file=sys.stderr)

    objects = []
    for i in range(hdr["objcnt"]):
        o = hdr["objtab"] + i * 24
        objects.append({
            "index":    i + 1,
            "vsize":    u32(o + 0x00),
            "base":     u32(o + 0x04),
            "flags":    u32(o + 0x08),
            "pagemap_idx": u32(o + 0x0C),
            "npages":   u32(o + 0x10),
        })
    return hdr, objects


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("-o", "--out", default="screamer_flat.bin")
    ap.add_argument("--map", default=None, help="write a JSON layout map here")
    args = ap.parse_args()

    data = Path(args.exe).read_bytes()
    hdr, objects = parse_le(data)

    print(f"MZ stub / DOS4GW loader : {hdr['stub_size']:,} bytes")
    print(f"LE image at            : 0x{hdr['le_offset']:X}")
    print(f"compiler               : Watcom C/C++32 (per embedded banner)")
    print(f"pages                  : {hdr['n_pages']} x 0x{hdr['page_size']:X}")
    print(f"objects                : {hdr['objcnt']}")
    for ob in objects:
        kind = "code" if ob["flags"] & 0x4 else "data"
        print(f"  obj{ob['index']}: base=0x{ob['base']:08X} "
              f"vsize={ob['vsize']:>9,} pages={ob['npages']:>3} ({kind})")

    entry_va = entry_obj_base = None
    for ob in objects:
        if ob["index"] == hdr["eip_obj"]:
            entry_obj_base = ob["base"]
    entry_va = entry_obj_base + hdr["eip"]
    print(f"entry point            : VA 0x{entry_va:X} "
          f"(obj{hdr['eip_obj']} + 0x{hdr['eip']:X})")

    # materialise every object at its virtual address
    top = max(ob["base"] + ob["vsize"] for ob in objects)
    img = bytearray(top)
    ps, dp = hdr["page_size"], hdr["datapages"]
    for ob in objects:
        for p in range(ob["npages"]):
            fo = dp + (ob["pagemap_idx"] - 1 + p) * ps
            va = ob["base"] + p * ps
            chunk = data[fo:fo + ps]
            # NB: slice-assign by len(chunk), not ps -- the final page of the
            # image is short (e_last_page_size), and assigning a short chunk
            # into a full-size slice would *shrink* the bytearray.
            img[va:va + len(chunk)] = chunk

    assert len(img) == top, f"image shrank: {len(img)} != {top}"

    Path(args.out).write_bytes(img)
    print(f"\nwrote {args.out}: {len(img):,} bytes of flat VA space "
          f"(0x0 .. 0x{top:X})")

    if args.map:
        Path(args.map).write_text(json.dumps(
            {"header": hdr, "objects": objects, "entry_va": entry_va},
            indent=2))
        print(f"wrote {args.map}")


if __name__ == "__main__":
    main()
