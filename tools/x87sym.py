#!/usr/bin/env python3
"""x87sym.py -- a routine's floating-point sums written out in the machine code's own order.

    python3 tools/x87sym.py build/screamer2_flat.bin 0x2348b [0x245a3] [name=0xNN ...]

Walks the instructions between the two addresses (objdump's, Intel syntax) as they come and keeps,
as text, what each register, each local of the frame ([ebp-n]) and each place on the FPU stack
holds.  Every store of a float or a double (fstp / fst / fistp) is a rounding and is printed as a
numbered value: `t12 = f32((t3 * t4) + F[car+0x94])`; a store anywhere but the frame is printed as
a store.  Compares and jumps are printed as they come, so that the branches can be followed by
hand.  It does NOT follow jumps: values that differ along two paths are shown as the last one
written.  Calls to the runtime's sine, cosine, square root, cutting and arc cosine are named;
other calls are printed with EAX and EDX.

Names: the first argument (EAX at the entry) is `a0` and the second (EDX) `a1`, unless named on
the command line (`a0=car`); the stack arguments are `s0`, `s1` ... ([ebp+...]).  A constant in
the program's data is shown with its value (`0.3d` a double, `100f` a float).

The point is the ORDER of sums and where the roundings fall, which the decompiler does not keep.
"""
import re, struct, subprocess, sys

img_path, lo = sys.argv[1], int(sys.argv[2], 16)
hi = int(sys.argv[3], 16) if len(sys.argv) > 3 and not "=" in sys.argv[3] else None
names = dict(a.split("=", 1) for a in sys.argv[3:] if "=" in a)
IMG = open(img_path, "rb").read()
if hi is None: hi = lo + 0x800
dis = subprocess.run(["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                      "--start-address=0x%x" % lo, "--stop-address=0x%x" % hi, img_path],
                     capture_output=True, text=True).stdout
RUNTIME = {0x4e9a4: "sin", 0x4e99a: "cos", 0x4ea60: "sqrt", 0x4ea12: "trunc", 0x4ea98: "acos"}

regs = {"eax": names.get("a0", "a0"), "edx": names.get("a1", "a1"), "ebx": "a2", "ecx": "a3"}
mem, st, n = {}, [], [0]
pushes = 0
def tmp(e, kind):
    n[0] += 1
    t = "t%d" % n[0]
    print("    %s = %s(%s)" % (t, kind, e))
    return t
def simp(e):
    return e if re.fullmatch(r"[\w.\[\]+\-]+", e) else "(" + e + ")"
saved = [0]          # registers pushed before `mov ebp,esp`: the stack arguments start after them
def frame(off):
    if off > 0:
        k = (off - 4 * (saved[0] + 1)) // 4
        return "s%d" % k if "s%d" % k not in names else names["s%d" % k]
    return "ebp%+#x" % off
def addr(op):
    """the place an operand names, as text"""
    m = re.search(r"\[(.*)\]", op)
    if m:
        inner = m.group(1)
        if inner.startswith("ebp"):
            return frame(int(inner[3:], 16) if inner != "ebp" else 0)
        m2 = re.fullmatch(r"(e[a-d]x|esi|edi)([+-]0x[0-9a-f]+)?", inner)
        if m2:
            base = regs.get(m2.group(1), m2.group(1))
            m3 = re.fullmatch(r"&ebp([+-]0x[0-9a-f]+)", base)
            if m3:     # a pointer to the frame: the frame's own place
                return frame(int(m3.group(1), 16) + (int(m2.group(2), 16) if m2.group(2) else 0))
            return "%s%s" % (base, m2.group(2) or "")
        m2 = re.fullmatch(r"(e[a-d]x)\*(\d)\+(0x[0-9a-f]+)", inner)
        if m2: return "%s+%s*%s" % (m2.group(3), regs.get(m2.group(1)), m2.group(2))
        m2 = re.fullmatch(r"(e[a-d]x)\+(0x[0-9a-f]+)", inner)
        if m2: return "%s+%s" % (regs.get(m2.group(1)), m2.group(2))
        return "[" + inner + "]"
    m = re.search(r"ds:(0x[0-9a-f]+)", op)
    if m: return "ds:" + m.group(1)
    return op
def show_imm(v):
    """an immediate that is a float's bits (0x3f000000) shown as the float"""
    if re.fullmatch(r"0x[0-9a-f]{8}", v):
        x = int(v, 16)
        f = struct.unpack("<f", struct.pack("<I", x))[0]
        if 1e-6 < abs(f) < 1e7 and (x & 0xffff) == 0 or f in (0.5, 0.25, 2.0, 1.0, -1.0): return "%rf" % f
    return v
def load(op, kind):
    """a memory operand read as a float (F), a double (D) or a whole number (I)"""
    a = addr(op)
    if a.startswith("ds:"):
        x = int(a[3:], 16)
        if 0x110000 <= x < 0x118000:      # the program's constants
            if "QWORD" in op: return "%sd" % repr(struct.unpack_from("<d", IMG, x)[0])
            if "DWORD" in op and kind != "I": return "%sf" % repr(struct.unpack_from("<f", IMG, x)[0])
        return "%s[%s]" % (kind, a[3:])
    if a in mem: return mem[a]
    return "%s[%s]" % (kind, a)
def size(op): return "QWORD" if "QWORD" in op else "TBYTE" if "TBYTE" in op else "DWORD"

for line in dis.splitlines():
    m = re.match(r"\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t(\S+)\s*(.*)$", line)
    if not m: continue
    at, op, args = int(m.group(1), 16), m.group(2), m.group(3).strip()
    a = [x.strip() for x in re.split(r",(?![^\[]*\])", args)] if args else []
    try:
        if op == "fld":
            if a[0].startswith("st("): st.append(st[-1 - int(a[0][3])])
            else: st.append(load(a[0], "F" if size(a[0]) == "DWORD" else "D"))
        elif op == "fild": st.append("(long)" + load(a[0], "I") if not addr(a[0]) in mem else "(long)" + mem[addr(a[0])])
        elif op == "fldz": st.append("0")
        elif op == "fld1": st.append("1")
        elif op in ("fstp", "fst", "fistp", "fist"):
            v = st[-1]
            if op.endswith("p"): st.pop()
            if a[0].startswith("st("):
                k = int(a[0][3]); st[-k] = v if op == "fst" else v
                continue
            kind = "int" if op.startswith("fi") else "f32" if size(a[0]) == "DWORD" else "f64"
            p = addr(a[0])
            if p.startswith("ebp"): mem[p] = tmp(v, kind)
            else: print("%05x STORE %s = %s(%s)" % (at, p, kind, v)); mem[p] = "%s(%s)" % (kind, v)
        elif op in ("fadd", "fsub", "fsubr", "fmul", "fdiv", "fdivr", "faddp", "fsubp", "fsubrp", "fmulp", "fdivp", "fdivrp", "fiadd", "fimul", "fisub", "fidiv"):
            sym = {"add": "+", "sub": "-", "mul": "*", "div": "/"}[re.sub(r"^fi?|r?p?$", "", op).rstrip("r")]
            rev = op.rstrip("p").endswith("r")
            if op.endswith("p") or (len(a) == 2 and a[0].startswith("st(") and a[0] != "st"):
                # st(i) = st(i) op st0  (fsubp st(1),st: st1 - st0; fsubrp: st0 - st1)
                k = int(a[0][3]) if a else 1
                x, y = st[-1 - k], st[-1]
                r = "%s %s %s" % (simp(y), sym, simp(x)) if rev else "%s %s %s" % (simp(x), sym, simp(y))
                if op.endswith("p"): st.pop(); st[-k] = r
                else: st[-1 - k] = r
            else:
                y = st[-1 - int(a[1][3])] if len(a) == 2 and a[1].startswith("st(") else load(a[0], "I" if op.startswith("fi") else "F" if size(a[0]) == "DWORD" else "D")
                x = st[-1]
                st[-1] = "%s %s %s" % (simp(y), sym, simp(x)) if rev else "%s %s %s" % (simp(x), sym, simp(y))
        elif op == "fchs": st[-1] = "-" + simp(st[-1])
        elif op == "fabs": st[-1] = "|%s|" % st[-1]
        elif op == "fsqrt": st[-1] = "sqrt(%s)" % st[-1]
        elif op == "fxch":
            k = int(a[0][3]) if a else 1
            st[-1], st[-1 - k] = st[-1 - k], st[-1]
        elif op in ("fcomp", "fcom", "fcompp", "fucompp", "fucomp"):
            y = load(a[0], "F" if size(a[0]) == "DWORD" else "D") if a else st[-2]
            print("%05x COMPARE %s with %s" % (at, st[-1], y))
            st.pop()
            if op.endswith("pp"): st.pop()
        elif op == "ftst": print("%05x COMPARE %s with 0" % (at, st[-1]))
        elif op in ("fnstsw", "sahf", "fwait"): pass
        elif op == "call":
            t = int(a[0], 16)
            if t in RUNTIME:
                st[-1] = "%s(%s)" % (RUNTIME[t], st[-1])
            else:
                print("%05x CALL 0x%x (eax=%s, edx=%s, ebx=%s)%s" % (at, t, regs["eax"], regs["edx"], regs["ebx"],
                      (" fpu: " + ", ".join(st)) if st else ""))
                regs["eax"] = "ret_%x" % t
                st[:] = ["fret_%x" % t]
        elif op == "mov":
            if a[0] in regs or a[0] in ("esi", "edi"):
                regs[a[0]] = load(a[1], "W") if "[" in a[1] or "ds:" in a[1] else regs.get(a[1], a[1])
            else:
                p = addr(a[0]); v = regs.get(a[1], show_imm(a[1]))
                if p.startswith("ebp"): mem[p] = v
                elif not p.startswith("s"): print("%05x STORE %s = %s" % (at, p, v))
                else: mem[p] = v
        elif op == "lea":
            p = addr(a[1])
            regs[a[0]] = "&" + p
        elif op == "imul" and len(a) == 3 and a[0] in regs:
            v = regs.get(a[1], load(a[1], "W") if "[" in a[1] or "ds:" in a[1] else a[1])
            regs[a[0]] = "%s * %s" % (simp(v), a[2])
        elif op in ("add", "sub", "imul", "and", "or", "shl", "sar", "shr") and a[0] in regs:
            sym = {"add": "+", "sub": "-", "imul": "*", "and": "&", "or": "|", "shl": "<<", "sar": ">>", "shr": ">>>"}[op]
            src = a[-1]
            v = regs.get(src, load(src, "W") if "[" in src or "ds:" in src else src)
            base = regs[a[1]] if len(a) == 3 else regs[a[0]]
            regs[a[0]] = "%s %s %s" % (simp(base), sym, simp(v))
        elif op == "mov" and a == ["ebp", "esp"]:
            saved[0] = pushes
        elif op in ("push",):
            pushes += 1
            v = regs.get(a[0], load(a[0], "W") if "[" in a[0] or "ds:" in a[0] else a[0])
            if "PTR" in a[0] or a[0].startswith("0x"): print("%05x PUSH %s" % (at, v))
        elif op.startswith("j") or op in ("cmp", "test", "ret", "inc", "dec"):
            print("%05x %s %s" % (at, op, args.replace("DWORD PTR ", "").replace("BYTE PTR ", "b")))
        else:
            pass
    except (IndexError, ValueError, KeyError) as e:
        print("%05x ?? %s %s (%s)" % (at, op, args, e))
