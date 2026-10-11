#!/usr/bin/env python3
"""
dat_container.py - reader for Screamer's .DAT asset containers.

Format (reverse-engineered 2026-10-07, verified against all 35 data files)
------------------------------------------------------------------------
    [ asset data .......... ]
    [ directory: N * 32 bytes ]
    [ uint32 directory_size ]        <- last 4 bytes of the file

    directory record (32 bytes):
        +0x00  16 bytes  name, XOR 0x3A encoded, NUL-terminated
                         (a NUL byte is stored as 0x3A, so padding decodes
                          as ':' -- that is an artifact, not data)
        +0x10  uint32    file offset of the asset data
        +0x14  uint32    stored size
        +0x18  uint32    uncompressed size
        +0x1C  uint32    method: 0 = stored verbatim, non-zero = compressed

    The directory is terminated by a record whose name is empty.  The final
    dword equals directory_size, so the directory can be located from the file
    tail alone.

Names carry meaningful type suffixes: .PCX .PKC .RAW .TXT .3DX .HMP .DTR .SPL
.MRK .MMP .UNR .COL .MLB .BNK .FNT

Usage:
    python3 tools/dat_container.py list   <file.dat> [...]
    python3 tools/dat_container.py extract <file.dat> <outdir> [name ...]
"""

import os
import struct
import sys

import lzw

XOR = 0x3A
REC = 32
NAME_LEN = 16


def decode_name(raw: bytes) -> str:
    end = raw.find(XOR)          # stored NUL
    if end < 0:
        end = NAME_LEN
    return bytes(b ^ XOR for b in raw[:end]).decode("latin-1")


def read_container(path: str):
    """Return (data, [entries]). Each entry is a dict."""
    with open(path, "rb") as fh:
        data = fh.read()
    if len(data) < 4 + REC:
        raise ValueError(f"{path}: too small to be a container")

    dir_size = struct.unpack_from("<I", data, len(data) - 4)[0]
    if dir_size == 0 or dir_size % REC != 0 or dir_size > len(data) - 4:
        raise ValueError(f"{path}: bad directory size 0x{dir_size:X} "
                         f"(file {len(data):,} bytes)")
    dir_start = len(data) - 4 - dir_size

    entries = []
    for off in range(dir_start, len(data) - 4, REC):
        name = decode_name(data[off:off + NAME_LEN])
        f0, f1, f2, f3 = struct.unpack_from("<4I", data, off + NAME_LEN)
        if name == "":
            continue                      # terminator record
        entries.append({
            "name": name, "offset": f0, "stored": f1,
            "uncompressed": f2, "method": f3,
        })
    return data, entries


def cmd_list(paths):
    for path in paths:
        data, entries = read_container(path)
        print(f"=== {path}  ({len(data):,} bytes, {len(entries)} entries) ===")
        print(f"  {'name':<22}{'offset':>10}{'stored':>10}{'full':>10}{'m':>3}")
        for e in entries:
            print(f"  {e['name']:<22}{e['offset']:>10}{e['stored']:>10}"
                  f"{e['uncompressed']:>10}{e['method']:>3}")
        print()


def cmd_extract(path, outdir, only, decompress=True):
    data, entries = read_container(path)
    base = os.path.splitext(os.path.basename(path))[0]
    outdir = os.path.join(outdir, base)
    os.makedirs(outdir, exist_ok=True)
    n = bad = 0
    for e in entries:
        if only and e["name"] not in only:
            continue
        blob = data[e["offset"]:e["offset"] + e["stored"]]
        if len(blob) < e["stored"]:
            print(f"  !! {e['name']}: truncated", file=sys.stderr)
        note = ""
        if e["method"] != 0:
            if decompress:
                try:
                    out = lzw.lzw_decode(blob, expected=e["uncompressed"])
                except Exception as exc:                  # noqa: BLE001
                    out = b""
                    note = f"  [LZW error: {exc}]"
                if len(out) != e["uncompressed"]:
                    bad += 1
                    note = (f"  [LZW got {len(out)}, expected "
                            f"{e['uncompressed']}]")
                blob = out
            else:
                note = f"  [method {e['method']} left packed]"
        dest = os.path.join(outdir, e["name"])
        with open(dest, "wb") as fh:
            fh.write(blob)
        print(f"  {e['name']:<24} {len(blob):>9,} bytes{note}")
        n += 1
    print(f"-> {n} file(s) written to {outdir}"
          + (f"  ({bad} did not decode to the declared size)" if bad else ""))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == "list":
        cmd_list(sys.argv[2:])
    elif cmd == "extract":
        if len(sys.argv) < 4:
            sys.exit("usage: extract <file.dat> <outdir> [name ...]")
        cmd_extract(sys.argv[2], sys.argv[3], set(sys.argv[4:]))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
