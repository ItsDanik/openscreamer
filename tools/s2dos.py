#!/usr/bin/env python3
"""
dosrun.py <out prefix> <plan>     plan = "Up:5,Up+Left:1,Up:3" (keys held for so many seconds)
Runs the ORIGINAL Screamer 2 in DOSBox on the hidden display :77 as a child of this script,
goes through its menus with ENTER into the default arcade race, and while the plan's keys
are held reads the game's own memory every few milliseconds: the race's clock and step, the
player's controls, every car's place, heading, speed.  <prefix>.frames = one line a game
frame; <prefix>.block = the data object as the race began.
"""
import sys, os, time, struct, subprocess, re
from Xlib import display, X, XK
from Xlib.ext import xtest
out, plan = sys.argv[1], sys.argv[2]
IMG = open("/home/danik/openscreamer/build/screamer2_flat.bin", "rb").read()
env = dict(os.environ, DISPLAY=":77", SCREAMER_DOS_FLATPAK="--nosocket=wayland --socket=x11 --env=DISPLAY=:77 --env=SDL_AUDIODRIVER=dummy --env=SDL_VIDEODRIVER=x11")
p = subprocess.Popen([os.path.expanduser("~/.local/bin/screamer-dos"), "2"], env=env, stdout=open(out + ".log", "w"), stderr=subprocess.STDOUT)
d = display.Display(":77")
code = lambda n: d.keysym_to_keycode(XK.string_to_keysym(n))
def key(n, down): xtest.fake_input(d, X.KeyPress if down else X.KeyRelease, code(n)); d.sync()
def tap(n, wait): key(n, 1); time.sleep(0.1); key(n, 0); time.sleep(wait)
def shot(name): subprocess.run(["import", "-window", "root", name], env=env)

time.sleep(24)
# (S2TRACK=<n>: so many presses of Right at the track's choice, which is on the screen before the seventh ENTER)
for i in range(9):
    if i == 6:
        for k in range(int(os.environ.get("S2TRACK", "0"))): tap("Right", 0.8)
    tap("Return", 3.0)
# the DOSBox process (the one that is not bwrap)
pid = None
for l in subprocess.run(["pgrep", "-a", "dosbox"], capture_output=True, text=True).stdout.splitlines():
    if l.split()[1] == "dosbox": pid = int(l.split()[0])
print("dosbox pid", pid)
mem = open("/proc/%d/mem" % pid, "rb", 0)
SIG = IMG[0x110d59:0x110d59 + 40]
base = None
for l in open("/proc/%d/maps" % pid):
    m = re.match(r"([0-9a-f]+)-([0-9a-f]+) rw", l)
    if not m: continue
    a, b = int(m.group(1), 16), int(m.group(2), 16)
    if b - a < 0x100000 or b - a > 0x10000000: continue
    try:
        mem.seek(a); buf = mem.read(b - a)
    except OSError: continue
    i = buf.find(SIG)
    if i >= 0: base = a + i - 0xd59; break
print("the game's data object is at host %s" % (hex(base) if base else None))
if not base: shot(out + "_lost.png"); p.terminate(); subprocess.run(["flatpak", "kill", "io.github.dosbox-staging"]); sys.exit(1)
def rd(flat, n): return os.pread(mem.fileno(), n, base + flat - 0x110000)
f32 = lambda a: struct.unpack("<f", rd(a, 4))[0]
i32 = lambda a: struct.unpack("<i", rd(a, 4))[0]
# wait for the race: the clock running
t0 = time.time()
while time.time() - t0 < 40 and not (i32(0x130c48) in (1, 2, 3, 4) and f32(0x13335c) > 0.05): time.sleep(0.05)
# ... that was the attract race; the player's begins when the clock starts again
t0 = time.time(); was = f32(0x13335c)
while time.time() - t0 < 40:
    c = f32(0x13335c)
    if c < was - 1.0 and i32(0x130d10) & 0x80 == 0: break
    was = max(was, c); time.sleep(0.01)
print("cars", i32(0x130c48), "clock", f32(0x13335c), "track", i32(0x130c4c), "mode", i32(0x130d10))
open(out + ".block", "wb").write(rd(0x110000, 0x2a000))
shot(out + "_start.png")
CAR, KEYS = 0x119300, 0x127708
steps = []
for part in plan.split(","):
    k, secs = part.split(":"); steps.append(([x for x in k.split("+") if x != "none"], float(secs)))
rows, last = [], None
extra = open(out + ".walls", "w")
carsf = open(out + ".cars", "wb")
def guest(ptr, n):
    """bytes at a pointer of the game's (its addresses are the data object's moved by one amount)"""
    delta = struct.unpack("<I", rd(CAR + 0xb4, 4))[0] - 0x127708
    if not delta or not ptr: return None
    try: return rd((ptr - delta) & 0xffffffff, n)
    except (OSError, ValueError): return None
pending = None
def commit():
    global pending
    if pending is None: return
    row, cars, lines = pending
    rows.append(row); carsf.write(cars); extra.write(lines)
    pending = None
def sample():
    """Every few milliseconds: everything read between two reads of the clock that agree; the
    frame's numbers are the LAST such reading before the clock moves on (DOSBox takes ~56 ms a
    frame here: a reading early in it can catch the game half way through its sums -- a step of
    0.005 between two of 0.057, a car's +0x4fc set in the original and not yet in the port)."""
    global last, pending
    c = f32(0x13335c)
    n = i32(0x130c48)
    ctl = struct.unpack("<6i", rd(KEYS + 0xa8, 24))
    row = [time.time(), c, f32(0x133350), f32(0x133354)] + list(ctl)
    for k in range(n):
        b = rd(CAR + k * 0xcc0 + 0x4d8, 0x30)
        x, h, z = struct.unpack_from("<3i", b, 0); pit, head, lean = struct.unpack_from("<3f", b, 12)
        row += [x, h, z, pit, head, lean, struct.unpack_from("<f", b, 0x28)[0], f32(CAR + k * 0xcc0 + 0x4d4), f32(CAR + k * 0xcc0 + 0xbc)]
    cars = rd(CAR, n * 0xcc0)          # (every car's whole record: the port's OS_S2TRACEFILE writes the same)
    lines = ""
    wp, dp = struct.unpack("<I", rd(0x11cdd8, 4))[0], struct.unpack("<I", rd(0x11cde8, 4))[0]
    wb, db = guest(wp, 28), guest(dp, 12)
    me = rd(CAR + 0x150, 0x18)
    lines += "%.4f wall %s dir %s car+0x34 %.3f +0x150 %s\n" % (c,
        (struct.unpack("<f6i", wb) if wb and len(wb) == 28 else None), (struct.unpack("<f2i", db) if db and len(db) == 12 else None),
        f32(CAR + 0x34), struct.unpack("<i2ifi", me[:20]))
    if f32(0x13335c) != c: return                    # (the clock moved while reading: not one frame's)
    if pending is not None and pending[0][1] != c: commit()      # (the clock has moved on: the last whole reading of the frame before is its)
    if c == last and pending is None: return         # (already committed)
    pending = (row, cars, lines); last = c
# the grid cell (column 128, row 129) as the game has it in memory: its head, and which objects its lists hold
try:
    grid = struct.unpack("<I", rd(0x11375c, 4))[0]
    rowp = struct.unpack("<I", guest(grid + 129 * 4, 4))[0]
    cellp = struct.unpack("<I", guest(rowp + 128 * 4, 4))[0]
    head = struct.unpack("<10i", guest(cellp, 40))
    delta = struct.unpack("<I", rd(CAR + 0xb4, 4))[0] - 0x127708
    print("cell 128,129 head:", [(h - delta if abs(h) > 0x100000 else h) for h in head], "(pointers less the move; the cell itself at %x)" % (cellp - delta))
    for name, first, cnt in (("first list", head[0], head[5]), ("second list", head[1], head[6])):
        recs = [struct.unpack("<16i", guest(first + k * 0x40, 0x40)) for k in range(min(cnt, 40))] if cnt > 0 else []
        print(" ", name, cnt, "records: objects", [r[0] for r in recs], "kinds", [r[15] for r in recs])
except Exception as e: print("cell read failed:", e)
held = []
for keys, secs in steps:
    for k in held:
        if k not in keys: key(k, 0)
    for k in keys:
        if k not in held: key(k, 1)
    held = keys
    # (the plan's seconds are the RACE'S CLOCK's, so that a run can be repeated)
    c1 = f32(0x13335c); t1 = time.time()
    while f32(0x13335c) - c1 < secs and time.time() - t1 < secs * 3 + 20: sample(); time.sleep(0.002)
commit()
for k in held: key(k, 0)
shot(out + "_end.png")
open(out + ".block2", "wb").write(rd(0x110000, 0x2a000))
with open(out + ".frames", "w") as f:
    for r in rows: f.write(" ".join(("%.9g" % v) if isinstance(v, float) else str(v) for v in r) + "\n")   # (9 digits: a float exactly -- with 6 the port was handed steps dozens of last bits off)
print(len(rows), "frames read")
key("Escape", 1); time.sleep(0.1); key("Escape", 0)
p.terminate(); subprocess.run(["flatpak", "kill", "io.github.dosbox-staging"])
