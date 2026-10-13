#!/usr/bin/env python3
"""Who points at a string?  For images made with `unle.py --fixups`.

    python3 tools/strrefs.py build/screamer2_layout.json build/screamer2_flat.bin .3dx VIS.TAB

For every string containing one of the words: its start address and the addresses of the
pointers to it (the relocation records list every pointer, so nothing is guessed).  A site
inside the code object is the operand of an instruction: decompile around it.
"""
import json, struct, sys
L = json.load(open(sys.argv[1])); img = open(sys.argv[2], "rb").read()
byval = {}
for s in L["fixup_sites"]:
    byval.setdefault(struct.unpack_from("<I", img, s)[0], []).append(s)
for w in sys.argv[3:]:
    k, seen = 0, set()
    while (k := img.find(w.encode(), k)) >= 0:
        a = k
        while a > 0 and 32 <= img[a - 1] < 127: a -= 1
        e = img.find(b"\0", k); k += 1
        if a in seen: continue
        seen.add(a)
        # a pointer may be to any byte of the run (a shorter string sharing the tail)
        for st in range(a, k):
            if st in byval:
                print(f"0x{st:06X} {img[st:e].decode('latin-1')!r:44} <- " +
                      " ".join(f"0x{x:X}" for x in byval[st]))
