#!/usr/bin/env python3
"""Screamer 2: write the numbers into the decompiled text.

    python3 tools/s2consts.py            (after tools/s2dec.sh / DecompRange.java)

A physics routine reads dozens of constants as `_DAT_00110648`.  Whether one is a float or
a double is in the instruction that reads it (DWORD / QWORD PTR), so the code object is
disassembled once and every `DAT_0011xxxx` that only FPU instructions read, and nothing
writes, gets its value as a comment after it:   _DAT_00110648/*0.25*/
Rewrites build/decomp2/f_*.c in place (idempotent) and lists them all in CONSTS.txt.
"""
import glob, json, re, struct, subprocess
img = open("build/screamer2_flat.bin", "rb").read()
dis = subprocess.run(["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                      "--start-address=0x10000", "--stop-address=0xe4000", "build/screamer2_flat.bin"],
                     capture_output=True, text=True).stdout
size, other = {}, set()
for m in re.finditer(r"\t([a-z0-9]+) +([^\n\t]*?)(DWORD|QWORD|TBYTE|WORD|BYTE) PTR ds:0x(11[0-9a-f]{4})([^\n]*)", dis):
    op, pre, sz, addr, post = m.groups()
    a = int(addr, 16)
    if op.startswith("f") and op not in ("fstp", "fst", "fistp", "fist", "fild", "fiadd", "fimul", "fisub", "ficom", "ficomp", "fidiv", "fisubr", "fidivr") and sz in ("DWORD", "QWORD"):
        size.setdefault(a, set()).add(sz)
    else:
        other.add(a)
val = {}
for a, s in size.items():
    if a in other or len(s) != 1 or not (0x110000 <= a < 0x113700): continue
    val[a] = struct.unpack_from("<d" if "QWORD" in s else "<f", img, a)[0]
def fmt(v):
    t = repr(float("%.9g" % v))
    return t[:-2] if t.endswith(".0") else t
n = 0
for path in glob.glob("build/decomp2/f_*.c"):
    s = open(path).read()
    def sub(m):
        global n
        a = int(m.group(2), 16)
        if a not in val: return m.group(0)
        n += 1
        return f"{m.group(1)}/*{fmt(val[a])}*/"
    t = re.sub(r"(_?DAT_(0011[0-9a-f]{4}))(?:/\*[^*]*\*/)?", sub, s)
    if t != s: open(path, "w").write(t)
with open("build/decomp2/CONSTS.txt", "w") as f:
    for a in sorted(val): f.write(f"{a:06x} {'double' if 'QWORD' in size[a] else 'float '} {fmt(val[a])}\n")
print(len(val), "constants,", n, "places annotated")
