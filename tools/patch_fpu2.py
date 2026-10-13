#!/usr/bin/env python3
"""Screamer 2: make the flat image decompile cleanly by inlining Watcom's FPU helpers.

Here they work on ST0 itself (no argument on the stack), so a call is simply replaced:
    call 0x4e99a -> fcos        call 0x4e9a4 -> fsin        call 0x4e9f1 -> fptan, fstp st0
    call 0x4ea12 -> frndint     (the helper sets the control word to TRUNCATE first: Ghidra's
                                 ROUND() is a (int) cast, as with Screamer 1's 0x4d7b4)
    call 0x4ea60 -> fsqrt
Output is for the decompiler ONLY; it is not a runnable program.

usage: tools/patch_fpu2.py build/screamer2_flat.bin build/screamer2_flat_fpu.bin
"""
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
LO, HI = 0x10000, 0x10000 + 869383
OPS = {0x4e99a: (b'\xd9\xff', 'cos'), 0x4e9a4: (b'\xd9\xfe', 'sin'), 0x4e9f1: (b'\xd9\xf2\xdd\xd8', 'tan'),
       0x4ea12: (b'\xd9\xfc', 'trunc'), 0x4ea60: (b'\xd9\xfa', 'sqrt')}
n = {}
i = LO
while i < HI - 5:
    if d[i] == 0xE8:
        tgt = (i + 5 + struct.unpack_from('<i', d, i + 1)[0]) & 0xffffffff
        if tgt in OPS and not (0x4e990 <= i < 0x4eb00):
            op, name = OPS[tgt]
            d[i:i+5] = op + b'\x90' * (5 - len(op)); n[name] = n.get(name, 0) + 1; i += 5; continue
    i += 1
open(dst, 'wb').write(d)
print(n)
