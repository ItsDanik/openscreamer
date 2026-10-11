#!/usr/bin/env python3
"""Make the flat image decompile cleanly: inline Watcom's FPU helper calls.

    call 0x4d7b4                 (round ST0)      ->  frndint
    sub esp,8 ... fstp qword [esp] ; call 0x4d826  ->  fcos   (0x4d826 = cos(double))
    sub esp,8 ... fstp qword [esp] ; call 0x4d832  ->  fsin   (0x4d832 = sin(double))

Ghidra otherwise treats the helpers as opaque calls (and marked the rounding
one "does not return"), which strips the arithmetic out of every physics
routine.  Output is for the decompiler ONLY; it is not a runnable program.

usage: tools/patch_fpu.py build/screamer_flat.bin build/screamer_flat_fpu.bin
"""
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
LO, HI = 0x10000, 0x10000 + 0xDB7A3
n = {'round': 0, 'cos': 0, 'sin': 0, 'skipped': 0}
i = LO
while i < HI - 5:
    if d[i] == 0xE8:
        tgt = (i + 5 + struct.unpack_from('<i', d, i + 1)[0]) & 0xffffffff
        if tgt == 0x4d7b4:
            d[i:i+5] = b'\xd9\xfc\x90\x90\x90'; n['round'] += 1; i += 5; continue
        if tgt in (0x4d826, 0x4d832) and d[i-3:i] == b'\xdd\x1c\x24':
            j = bytes(d[i-48:i-3]).rfind(b'\x83\xec\x08')
            if j < 0: n['skipped'] += 1; i += 5; continue
            j += i - 48
            d[j:j+3] = b'\x90\x90\x90'
            op = b'\xd9\xff' if tgt == 0x4d826 else b'\xd9\xfe'
            d[i-3:i+5] = op + b'\x90' * 6
            n['cos' if tgt == 0x4d826 else 'sin'] += 1; i += 5; continue
    i += 1
open(dst, 'wb').write(d)
print(n)
