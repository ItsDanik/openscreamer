#!/usr/bin/env python3
"""
Screamer 2: THE GAME'S OWN MACHINE CODE against the port's converted routines.

Runs the race frame (0x2baa4, then the camera 0x12dff) of the program itself in a CPU
emulator (Unicorn), from the memory the port had at the start of a test race, with the same
keys, on the port's test world (level ground of one road kind, no road points: OS_S2FLAT,
OS_S2NONEAR), and compares the player's car's record after every frame with what the port's
routines (tools/s2c.py -> s2_gen.inc) made of it.  Where they part, the conversion is wrong.

    OS_S2FLAT=36 OS_S2NONEAR=1 OS_S2AT=x,y,heading OS_S2STEP=0.04 OS_S2PHYS=<frames> \
      OS_S2KEYS=A OS_S2KEYS2=AL OS_S2KEYS2AT=4 OS_S2DUMP=block.bin OS_S2TRACEFILE=trace.bin \
      port/build/openscreamer -game 2
    python3 tools/s2emu.py block.bin trace.bin --step 0.04 --keys A --keys2 AL --at 4 --kind 36

Needs `unicorn` and `capstone` (pip).  What is not the game's here: the same things the port
answers itself -- the ground (level), the nearest road point (none), sound, smoke, the
replay's recorder, the network, the engine's lists -- each returns at once.
"""
import sys, struct, argparse, os
from unicorn import *
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ap = argparse.ArgumentParser()
ap.add_argument("block"); ap.add_argument("trace")
ap.add_argument("--image", default="build/screamer2_flat.bin")
ap.add_argument("--step", type=float, default=0.04)
ap.add_argument("--keys", default="A"); ap.add_argument("--keys2", default=None); ap.add_argument("--at", type=float, default=0.0)
ap.add_argument("--plan", default=None)          # "A:4,AL:0.6,B:2": keys for so many seconds, as OS_S2PLAN
ap.add_argument("--kind", type=int, default=36)
ap.add_argument("--cars", type=int, default=0)       # compare so many cars (without --hooks: 1)
ap.add_argument("--hooks", default=None)         # OS_S2HOOKLOG's file: the port's answers about the track, replayed
ap.add_argument("--frames", type=int, default=0)
ap.add_argument("--fpcw", type=lambda v: int(v, 0), default=0x127f)   # what the game's runtime loads (0xbb603)
ap.add_argument("--show", default="")           # offsets to print every frame, e.g. 4e8,500,4d4
ap.add_argument("--tol", type=float, default=2e-3)
ap.add_argument("--inttol", type=int, default=0)  # whole numbers may differ by this much (x87 against C arithmetic)
ap.add_argument("--every", type=int, default=0)   # print heading, speed, slip of both every so many frames
ap.add_argument("--loose", action="store_true")  # only where each car is, how it points and how fast, with room for rounding
ap.add_argument("--stop", type=int, default=1)  # stop after this many frames that differ
A = ap.parse_args()

IMG = open(A.image, "rb").read()
BLK = open(A.block, "rb").read()
seed, camera, hookat = struct.unpack_from("<3I", BLK, 0x100000)
HK = open(A.hooks, "rb").read() if A.hooks else None
TR = open(A.trace, "rb").read()
CAR, KEYS, RET = 0x119300, 0x127708, 0x3ff000

mu = Uc(UC_ARCH_X86, UC_MODE_32)
mu.mem_map(0, 0x400000)
mu.mem_write(0, IMG)
# The port's block starts empty: its routines carry the program's constants as numbers and
# only the tables of tools/s2_tables.txt are copied in.  Here the program's own data object
# stays, and the block's bytes go over it where the port has put something.
merged = bytearray(BLK[:0x100000])
for i in range(min(len(IMG), 0x210000) - 0x110000):
    if merged[i] == 0: merged[i] = IMG[0x110000 + i]
mu.mem_write(0x110000, bytes(merged))
mu.reg_write(UC_X86_REG_FPCW, A.fpcw)
md = Cs(CS_ARCH_X86, CS_MODE_32)

def ret_bytes(entry):
    """how many bytes of arguments the routine takes off the stack itself (ret n)"""
    for ins in md.disasm(IMG[entry:entry + 0x4000], entry):
        if ins.mnemonic == "ret":
            return int(ins.op_str, 0) if ins.op_str else 0
    return 0

def leave(uc, pop, eax=None):
    """(the routine's first bytes have been made `ret n`: only what it gives back is set here)"""
    if eax is not None: uc.reg_write(UC_X86_REG_EAX, eax & 0xffffffff)

NOTHING = [0x204af, 0x31b00, 0x31c8c, 0x4a455, 0x4b49f, 0x4b4d4, 0x4bfbe, 0x4c01c, 0x4c0cf, 0x4eaed, 0x4feb4,
           0xaea54, 0xaebf0, 0xaee18, 0xaee98, 0xaf168, 0xaf45c, 0x4f6c4, 0xb04a8, 0x132b4]
# (the library's copy and fill load ES, which this emulator does not survive: done here)
POP = {a: ret_bytes(a) for a in NOTHING + [0x28ce6, 0x2a371, 0xb3c09, 0xb3c2d, 0xae6e3, 0xae8b0, 0xb3be4]}
state = {"seed": seed, "calls": {}}

def w32(uc, a, v): uc.mem_write(a, struct.pack("<i", v))

def hook(uc, addr, size, user):
    state["calls"][addr] = state["calls"].get(addr, 0) + 1
    if HK is not None and addr in (0x28ce6, 0x2a371):
        at = state.setdefault("hk", hookat)
        x, z = struct.unpack("<i", uc.mem_read(0x11cdcc, 4))[0], struct.unpack("<i", uc.mem_read(0x11cdd4, 4))[0]
        if addr == 0x28ce6:
            r = struct.unpack_from("<9i", HK, at); state["hk"] = at + 36
            if r[0] != 1 or r[1] != x or r[2] != z:
                if r[0] != 1 or abs(r[1] - x) + abs(r[2] - z) > 40: state.setdefault("desync", "ground asked at %d %d, the port was asked %s" % (x, z, r[:3]))
                state["near"] = state.get("near", 0) + 1
            w32(uc, 0x4cb98, 0)
            if r[3] & 1:
                w32(uc, 0x4cbec, r[8])
                if not r[3] & 2: w32(uc, 0x4cb98, r[4]); w32(uc, 0x4cba0, r[5]); w32(uc, 0x4cba4, r[6]); w32(uc, 0x4cba8, r[7])
            leave(uc, POP[addr], r[3] & 1)
        else:
            r = struct.unpack_from("<13i", HK, at); state["hk"] = at + 52
            walls = uc.reg_read(UC_X86_REG_EAX)
            if r[0] != 2 or r[1] != walls or r[2] != x or r[3] != z:
                if r[0] != 2 or r[1] != walls or abs(r[2] - x) + abs(r[3] - z) > 40: state.setdefault("desync", "nearest(%d) asked at %d %d, the port was asked %s" % (walls, x, z, r[:4]))
                state["near"] = state.get("near", 0) + 1
            found, rec = r[4], r[5] & 0xffffffff
            if found:
                uc.mem_write(rec, HK[at + 24:at + 24 + (28 if walls else 12)])
                uc.mem_write(0x11cdd8 if walls else 0x11cde8, struct.pack("<I", rec))
            was = struct.unpack("<I", uc.mem_read(0x11cde8, 4))[0]
            if not walls and was:
                uc.mem_write(0x11cde0, bytes(uc.mem_read(was + 4, 8)))
                a = struct.unpack("<f", uc.mem_read(was, 4))[0]
                f32r = lambda v: struct.unpack("<f", struct.pack("<f", v))[0]
                if struct.unpack("<I", uc.mem_read(0x130d30, 4))[0] & 8: a = f32r(a + f32r(3.14159274))
                if a > f32r(6.28318548): a = f32r(a + f32r(-6.28318548))
                uc.mem_write(0x11cddc, struct.pack("<f", a))
            leave(uc, POP[addr], found)
        return
    if addr == 0x28ce6:                         # the ground: level, at 0, of the one kind
        w32(uc, 0x4cb98, 0); w32(uc, 0x4cba0, 0); w32(uc, 0x4cba4, 16384); w32(uc, 0x4cba8, 0); w32(uc, 0x4cbec, A.kind)
        leave(uc, POP[addr], 1)
    elif addr == 0x2a371: leave(uc, POP[addr], 0)
    elif addr == 0xb3c09:
        state["seed"] = (state["seed"] * 1103515245 + 12345) & 0xffffffff
        leave(uc, POP[addr], state["seed"] >> 16 & 0x7fff)
    elif addr == 0xb3c2d:
        state["seed"] = uc.reg_read(UC_X86_REG_EAX); leave(uc, POP[addr], 0)
    elif addr == 0xae6e3:                       # memcpy(eax, edx, ebx)
        to, frm, n = (uc.reg_read(r) for r in (UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX))
        uc.mem_write(to, bytes(uc.mem_read(frm, n)))
    elif addr == 0xae8b0:                       # memset(eax, edx, ebx)
        to, v, n = (uc.reg_read(r) for r in (UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX))
        uc.mem_write(to, bytes([v & 255]) * n)
    elif addr == 0xb3be4:                       # strcpy(eax, edx)
        to, frm = uc.reg_read(UC_X86_REG_EAX), uc.reg_read(UC_X86_REG_EDX)
        b = bytes(uc.mem_read(frm, 256)); uc.mem_write(to, b[:b.index(0) + 1])
    else: leave(uc, POP[addr], 0)

def fnmark(uc, addr, size, user):
    c = bytes(uc.mem_read(CAR + 0xcc0 + 0x4e4, 12))
    print("  in %x: car1 pitch %.4f head %.4f lean %.4f step %.4f" % ((addr,) + struct.unpack("<3f", c) + struct.unpack("<f", uc.mem_read(0x133350, 4))))
    c_ = CAR + 0xcc0
    g = lambda o: struct.unpack("<f", uc.mem_read(c_ + o, 4))[0]; gi = lambda o: struct.unpack("<i", uc.mem_read(c_ + o, 4))[0]
    print("      corner heights %.1f %.1f %.1f %.1f  last frame's %.1f %.1f %.1f %.1f  wheels %d %d %d %d  speeds up %.1f %.1f %.1f %.1f" % (
          g(0x518), g(0x524), g(0x530), g(0x53c), g(0x774), g(0x780), g(0x78c), g(0x798), gi(0x5a4), gi(0x614), gi(0x684), gi(0x6f4), g(0x548), g(0x554), g(0x560), g(0x56c)))
if os.environ.get("EMUFN"):
    for a in (0x1b36f, 0x16824, 0x1b92b, 0x245a3, 0x22d35, 0x1684e, 0x16878, 0x167e2, 0x1c7f8, 0x1d9fc, 0x1bef1, 0x1e529, 0x1c4fb, 0x19810, 0x29571, 0x28f96):
        mu.hook_add(UC_HOOK_CODE, fnmark, begin=a, end=a)
for a in POP:
    mu.mem_write(a, b"\xc2" + struct.pack("<H", POP[a]) if POP[a] else b"\xc3")
    mu.hook_add(UC_HOOK_CODE, hook, begin=a, end=a)

import glob, os, re
ENTRIES = set(int(re.search(r"f_([0-9a-f]+)\.c", p).group(1), 16) for p in glob.glob("build/decomp2/f_*.c"))
entered = []
last = []
def trace(uc, addr, size, user):
    if addr == 0x14dc0 and os.environ.get("EMUDBG"):
        esp = uc.reg_read(UC_X86_REG_ESP)
        r, x, y = struct.unpack("<Iff", uc.mem_read(esp, 12))
        if not (abs(x) < 100 and abs(y) < 100): print("  0x14dc0(%g, %g) called from %x; before: %s" % (x, y, r, " ".join("%x" % a for a in last[-30:])))
    if addr in (0xae6e3, 0xae6ec, 0xae6f1, 0xae700, 0xae701, 0xae702, 0xae707) and os.environ.get("EMUDBG"):
        esp = uc.reg_read(UC_X86_REG_ESP)
        print("  %x: esp %x [esp] %x  eax %x ebx %x ecx %x es %x ds %x ss %x" % (addr, esp, struct.unpack("<I", uc.mem_read(esp, 4))[0],
              uc.reg_read(UC_X86_REG_EAX), uc.reg_read(UC_X86_REG_EBX), uc.reg_read(UC_X86_REG_ECX), uc.reg_read(UC_X86_REG_ES), uc.reg_read(UC_X86_REG_DS), uc.reg_read(UC_X86_REG_SS)))
    if addr in ENTRIES:
        entered.append((addr, uc.reg_read(UC_X86_REG_EAX), uc.reg_read(UC_X86_REG_EDX), uc.reg_read(UC_X86_REG_EBX)))
        if len(entered) > 400: del entered[0]
    if addr >= 0x10000:
        last.append(addr)
        if len(last) > 64: del last[0]
if os.environ.get("EMUDBG"): mu.hook_add(UC_HOOK_CODE, trace)      # (slow: every instruction)

def intr(uc, no, user):
    print("  interrupt %d at eip %x esp %x; last %s" % (no, uc.reg_read(UC_X86_REG_EIP), uc.reg_read(UC_X86_REG_ESP), " ".join("%x" % a for a in last[-6:])))
    uc.emu_stop()
mu.hook_add(UC_HOOK_INTR, intr)
# EMUINS=1: count what is executed (how much work a frame is for an interpreter): all
# instructions, and those of the floating-point unit (first byte 0xd8..0xdf)
INS = [0, 0, set()]
def count_ins(uc, addr, size, user):
    INS[0] += 1
    INS[2].add(addr)
    if 0xd8 <= uc.mem_read(addr, 1)[0] <= 0xdf: INS[1] += 1
if os.environ.get("EMUINS"): mu.hook_add(UC_HOOK_CODE, count_ins)
import atexit
if os.environ.get("EMUINS") and os.environ["EMUINS"] != "1": atexit.register(lambda: open(os.environ["EMUINS"], "a").write("".join("%x\n" % a for a in sorted(INS[2]))))
if os.environ.get("EMUINS"): atexit.register(lambda: print("instructions executed: %d (%d floating-point), at %d different addresses" % (INS[0], INS[1], len(INS[2]))))

def call(entry, eax=0, edx=0, ebx=0, ecx=0):
    esp = 0x3f0000
    mu.mem_write(esp, struct.pack("<I", RET))
    for r, v in ((UC_X86_REG_ESP, esp), (UC_X86_REG_EAX, eax), (UC_X86_REG_EDX, edx), (UC_X86_REG_EBX, ebx), (UC_X86_REG_ECX, ecx)):
        mu.reg_write(r, v)
    mu.emu_start(entry, RET, count=int(os.environ.get("EMUCOUNT", "30000000")))
    if mu.reg_read(UC_X86_REG_EIP) != RET:
        print("  0x%x did not come back: at 0x%x; routines entered last: %s; instructions: %s" % (entry, mu.reg_read(UC_X86_REG_EIP),
              " ".join("%x" % e[0] for e in entered[-12:]), " ".join("%x" % a for a in last[-24:])))
        raise SystemExit(1)

def f32(b, o): return struct.unpack_from("<f", b, o)[0]
def i32(b, o): return struct.unpack_from("<i", b, o)[0]

NCARS = A.cars or (struct.unpack_from("<i", BLK, 0x130c48 - 0x110000)[0] if A.hooks else 1)
REC = 0xcc0 * NCARS
frames = A.frames or len(TR) // REC
show = [int(x, 16) for x in A.show.split(",") if x]
bad = 0
for f in range(frames):
    keys = A.keys2 if (A.keys2 and f * A.step >= A.at) else A.keys
    if A.plan:
        # (as the port's test does it, in single precision)
        tnow, frm = struct.unpack("<f", struct.pack("<f", struct.unpack("<f", struct.pack("<f", f))[0] * struct.unpack("<f", struct.pack("<f", A.step))[0]))[0], 0.0
        for part in A.plan.split(","):
            k, d = part.split(":")
            if tnow >= frm: keys = k
            frm = struct.unpack("<f", struct.pack("<f", frm + float(d)))[0]
    for k, off in (("A", 0xa8), ("B", 0xac), ("L", 0xb0), ("R", 0xb4)):
        w32(mu, KEYS + off, 1023 if k in keys else 0)
    w32(mu, KEYS + 0xb8, 0); w32(mu, KEYS + 0xbc, 0)
    mu.mem_write(0x133350, struct.pack("<2f", A.step, A.step))
    try:
        call(0x2baa4)
        call(0x12dff, eax=camera, edx=1)
    except UcError as e:
        print("frame %d: the emulator stopped: %s at 0x%x" % (f, e, mu.reg_read(UC_X86_REG_EIP)))
        print("   the last routines entered:", " ".join("%x(%x,%x,%x)" % e for e in entered[-14:]))
        print("   the last instructions:", " ".join("%x" % a for a in last[-40:])); break
    game = bytes(mu.mem_read(CAR, REC))
    port = TR[f * REC:(f + 1) * REC]
    if "desync" in state: print("frame %d: %s" % (f, state["desync"])); break
    if show:
        print("f %4d " % f + "  ".join("+%03x game %11.4f port %11.4f" % (o, f32(game, o), f32(port, o)) for o in show))
    if A.every and f % A.every == 0:
        for c in range(NCARS):
            g, q = game[c * 0xcc0:(c + 1) * 0xcc0], port[c * 0xcc0:(c + 1) * 0xcc0]
            print("f %4d %5.2f s car %d | game: heading %7.3f speed %6.1f at %7d %7d %6d | port: heading %7.3f speed %6.1f at %7d %7d %6d" % (
                  f, f * A.step, c, f32(g, 0x4e8), f32(g, 0x500), i32(g, 0x4d8), i32(g, 0x4e0), i32(g, 0x4dc),
                  f32(q, 0x4e8), f32(q, 0x500), i32(q, 0x4d8), i32(q, 0x4e0), i32(q, 0x4dc)))
    diff = []
    if A.loose:
        import math
        for c in range(NCARS):
            b = c * 0xcc0
            for o in (0x4d8, 0x4dc, 0x4e0):
                if abs(i32(game, b + o) - i32(port, b + o)) > 25: diff.append((b + o, "%d" % i32(game, b + o), "%d" % i32(port, b + o)))
            for o in (0x4e4, 0x4e8, 0x4ec):
                d = (f32(game, b + o) - f32(port, b + o) + math.pi) % (2 * math.pi) - math.pi
                if not abs(d) <= 0.02: diff.append((b + o, "%.4f" % f32(game, b + o), "%.4f" % f32(port, b + o)))
            if not abs(f32(game, b + 0x500) - f32(port, b + 0x500)) <= 1.0: diff.append((b + 0x500, "%.2f" % f32(game, b + 0x500), "%.2f" % f32(port, b + 0x500)))
    for o in ([] if A.loose else range(0, REC, 4)):
        if game[o:o + 4] == port[o:o + 4]: continue
        a, b = f32(game, o), f32(port, o)
        ia, ib = i32(game, o), i32(port, o)
        if abs(ia) < 0x100000 and abs(ib) < 0x100000:                 # whole numbers
            if abs(ia - ib) > A.inttol: diff.append((o, "%d" % ia, "%d" % ib))
        elif a == a and b == b and abs(a - b) <= A.tol * max(1.0, abs(a), abs(b)): continue
        else: diff.append((o, "%.6g" % a, "%.6g" % b))
    if diff:
        bad += 1
        print("frame %d (%.2f s): %d words differ; the first: " % (f, f * A.step, len(diff)) +
              "; ".join("car %d +0x%03x game %s port %s" % (d[0] // 0xcc0, d[0] % 0xcc0, d[1], d[2]) for d in diff[:14]))
        if bad >= A.stop: break
else:
    print("%d frames: the same" % frames)
print("questions about the track not asked at quite the same place:", state.get("near", 0))
print("hooks called:", " ".join("%x:%d" % (a, n) for a, n in sorted(state["calls"].items())))
