#!/usr/bin/env python3
"""Screamer 2: turn decompiled routines into C the port compiles.

    python3 tools/s2c.py <out.c> <root entry>... [--hook <entry>...] [--take <entry>...]
                         [--fix tools/s2c_fix.txt]

The car's physics is some 5,000 lines of decompiled text.  Rewriting it from an understanding
of it is how the feel of a game gets lost, so it is carried over as it is: every routine
keeps its address for a name and works on the game's own memory layout -- one block of
bytes standing for the program's data object (0x110000 up), in which the car records sit at
the addresses they have in the game.  An address is a number, never a C pointer; MP(a) is
the byte it names.  What the routines call outside the set (the track, sound, the keys,
files) is left as a call to a HOOK the port answers (port/src/s2_hooks.c).

Input: build/decomp2/f_<entry>.c as tools/DecompRange.java writes them (parameters as plain
ints, a `// GLOBAL` line for every global with its type, constants annotated by
tools/s2consts.py).  The roots and every routine they reach inside 0x10000-0x4a000 (the game's logic) are
taken, except the hooks; `--take` names routines above that range to take as well.  `--fix` names a file of corrections for what the decompiler gets
wrong (a float argument passed on the stack, mostly):
    @<entry>                      the routine the following lines are for
    s/<regex>/<replacement>/      applied to its converted text
Functions with `in_stack_`, `unaff_` or `extraout_` in them are listed: each needs a look.
"""
import re, struct, sys, os

DEC = "build/decomp2"
IMG = open("build/screamer2_flat.bin", "rb").read()
LO, HI = 0x10000, 0x4a000        # above: the engine and the libraries, always hooks

args = sys.argv[1:]
out = args.pop(0)
roots, hooks, fixfile, take, tables, dispatch = [], set(), None, set(), None, set()
mode = "root"
for a in args:
    if a == "--hook": mode = "hook"
    elif a == "--fix": mode = "fix"
    elif a == "--take": mode = "take"
    elif a == "--dispatch": mode = "dispatch"
    elif mode == "dispatch": dispatch.add(int(a, 16)); roots.append(int(a, 16))
    elif a == "--tables": mode = "tables"
    elif mode == "tables": tables = a; mode = "root"
    elif mode == "take": take.add(int(a, 16))
    elif mode == "fix": fixfile = a; mode = "root"
    elif mode == "hook": hooks.add(int(a, 16))
    else: roots.append(int(a, 16))

fixes = {}
if fixfile and os.path.exists(fixfile):
    cur = None
    for line in open(fixfile):
        line = line.rstrip("\n")
        if line.startswith("@"): cur = int(line[1:].split()[0], 16); fixes.setdefault(cur, [])
        elif line.startswith("s") and cur is not None and len(line) > 2:
            d = line[1]
            parts = re.split(r"(?<!\\)" + re.escape(d), line[2:])
            fixes[cur].append((parts[0].replace("\\" + d, d), parts[1].replace("\\" + d, d)))

consts = {}
for l in open(f"{DEC}/CONSTS.txt"):
    a, kind, v = l.split()
    consts[int(a, 16)] = (kind, v)

CT = {"int": "int32_t", "uint": "uint32_t", "undefined4": "uint32_t", "float": "float", "char": "char",
      "byte": "uint8_t", "undefined1": "uint8_t", "undefined": "uint8_t", "undefined2": "uint16_t",
      "short": "int16_t", "ushort": "uint16_t", "double": "double", "undefined8": "uint64_t",
      "bool": "uint8_t", "pointer": "uint32_t", "string": "char"}

# what the decompiler prints out of order is put back (tools/s2order.py)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s2order
reordered = {}
def read(entry):
    p = f"{DEC}/f_{entry:05x}.c"
    if not os.path.exists(p): return None
    t, moved, stuck = s2order.reorder(re.sub(r"\bfunc_0x([0-9a-f]{8})\b", r"FUN_\1", open(p).read().replace("thunk_FUN_", "FUN_")))
    if moved or stuck: reordered[entry] = (len(moved), stuck)
    return t

# ---- which routines
todo, funcs, externs = list(roots), {}, set()
while todo:
    e = todo.pop()
    if e in funcs or e in hooks: continue
    t = read(e)
    if t is None: externs.add(e); continue
    funcs[e] = t
    for c in re.findall(r"FUN_([0-9a-f]{8})", t):
        c = int(c, 16)
        if c == e: continue
        if c in hooks or not (LO <= c < HI or c in take): externs.add(c)
        elif c not in funcs: todo.append(c)
externs |= hooks

def lit(kind, v):
    if kind == "double": return v if ("." in v or "e" in v or "n" in v) else v + ".0"
    return (v if ("." in v or "e" in v) else v + ".0") + "f"

def close2(text, i):                         # index of the bracket matching the one at i
    d = 0
    for k in range(i, len(text)):
        if text[k] == "(": d += 1
        elif text[k] == ")":
            d -= 1
            if d == 0: return k
    return -1

review, strings, gl_all = [], {}, {}
def convert(entry, t):
    gl = {}
    for m in re.finditer(r"^// GLOBAL (\S+?)(?:/\*[^*]*\*/)? @([0-9a-f]+) (\d+) (\S+)", t, re.M):
        gl[int(m.group(2), 16)] = (m.group(1), int(m.group(3)), m.group(4))
    body = t[t.index("\n", t.rindex("// ", 0, t.index("\n\n"))):] if "\n\n" in t else t
    body = "\n".join(l for l in body.split("\n") if not l.startswith("//") and not l.startswith("/* WARNING"))
    if re.search(r"in_stack_|unaff_|extraout_|in_E[A-D]X|in_ST", body): review.append(entry)
    # `(int)(&DAT_x + E)` counts in the variable's own size, as C does with a pointer: for one
    # the listing gives four bytes, E is in fours (0x2aeb9: the sparks' records, fifteen
    # words each, were being stepped fifteen BYTES)
    def scaled(m):
        size = gl.get(int(m.group(2), 16), ("", 1, ""))[1]
        return m.group(0) if size <= 1 else "(int)(&%sDAT_%s + (%s) * %d)" % (m.group(1), m.group(2), m.group(3), size)
    body = re.sub(r"\(int\)\(&(_?)DAT_([0-9a-f]{8}) \+ ([^()\n]*)\)", scaled, body)
    # EVERY STORE OF A FLOAT IS A ROUNDING.  The game's code keeps a sum on the processor's
    # stack at full width and rounds it when it is put into a float (fstp dword) -- often into
    # a temporary that the very next instruction reads back.  The stock decompiler prints such
    # a pair as one expression and the rounding is gone (0x4a831: the path's direction, a
    # difference of two nearly equal sums, was off in the fifth figure).  The language variant
    # of tools/ghidra-ext shows each store as f32(E) / f64(E); here that becomes a cast, which
    # in C (x87, -fexcess-precision=standard) rounds at that place and nowhere else.
    #   what is rounded is a float whatever the decompiler calls it: four untyped bytes
    #   read there (a float copied through the processor's stack) are read as a float
    def asf(n, ty):      # (a local is on the emulated stack: its address is a number for MP())
        if re.match(r"local_|[a-z]+Stack_", n): return "*(%s *)(&%s)" % (ty, n)
        return "S2_AS%s(%s)" % ("F" if ty == "float" else "D", n)
    u4 = set(re.findall(r"\bundefined4 ((?:local|param|[a-z]+Var|[a-z]+Stack)_?[0-9a-f_]*)\s*[;,)]", body))
    u8 = set(re.findall(r"\bundefined8 ((?:local|param|[a-z]+Var|[a-z]+Stack)_?[0-9a-f_]*)\s*[;,)]", body))
    #   a temporary without a type that only such stores fill IS a float; where it is copied
    #   on, its bytes go, whatever the decompiler calls the place
    for n in sorted(u4):
        if not re.search(r"\b%s = f32\(" % n, body): continue
        if len(re.findall(r"\b%s = (?!=)" % n, body)) != len(re.findall(r"\b%s = f32\(" % n, body)): continue
        u4.discard(n)
        body = re.sub(r"\bundefined4 %s;" % n, "float %s;" % n, body)
        body = re.sub(r"\b%s = f32\(" % n, "%s = (float)f32(" % n, body)
        body = re.sub(r"^([ \t]*)([^=;\n{}]+?) = %s;" % n, r"\1S2_SETF(\2, %s);" % n, body, flags=re.M)
        for l in body.split("\n"):
            if re.search(r"\b%s\b" % n, l) and not re.search(r"S2_SETF\(.*, %s\);|\b%s = \(float\)f32\(|f32\(%s\)|float %s;" % (n, n, n, n), l):
                print("%05x: a float temporary used otherwise: %s" % (entry, l.strip()[:110]), file=sys.stderr)
    #   and the same for what the decompiler itself calls a float: `place = name;` puts the
    #   float's bytes in the place.  (Where a whole number is meant the decompiler writes a
    #   conversion.)  0x2aeb9: a spark's place and speed went into a table without a type as
    #   whole numbers, and came out not numbers at all.
    fl = set(re.findall(r"^\s+float ((?:local|[a-z]+Var|[a-z]+Stack)_?[0-9a-f_]*);", body, re.M))
    db = set(re.findall(r"^\s+double ((?:local|[a-z]+Var|[a-z]+Stack)_?[0-9a-f_]*);", body, re.M))
    def putf(m):
        lv = m.group(2).strip()
        if m.group(3) not in fl or lv in fl or lv in db or lv.startswith("dVar") or lv.startswith("*(float *)") or lv.startswith("*(double *)"): return m.group(0)
        return "%sS2_SETF(%s, %s);" % (m.group(1), m.group(2), m.group(3))
    body = re.sub(r"^([ \t]*)([^=;\n{}]+?) = (\w+);$", putf, body, flags=re.M)
    for m in reversed(list(re.finditer(r"\bf(32|64)\(", body))):
        e = close2(body, m.end() - 1)
        inner = body[m.end():e]
        if m.group(1) == "32":
            inner = inner.replace("*(undefined4 *)", "*(float *)")
            if inner.strip() in u4: inner = asf(inner.strip(), "float")
            #   (one of the program's own variables, whatever type the listing gives it)
            #   (`(&local)[i]`: the float i places after a local of another type)
            g = re.fullmatch(r"\s*\(&(local_[0-9a-f]+)\)\[([^\]]*)\]\s*", inner)
            if g: inner = "*(float *)(&%s + (%s) * 4)" % (g.group(1), g.group(2))
            g = re.fullmatch(r"\s*_?(DAT_[0-9a-f]{8})\s*", inner)
            if g: inner = "*(float *)(&%s)" % g.group(1)
            g = re.fullmatch(r"\s*\(&_?(DAT_[0-9a-f]{8})\)\[([^\]]*)\]\s*", inner)
            if g: inner = "*(float *)(&%s + (%s) * 4)" % (g.group(1), g.group(2))
        else:
            inner = inner.replace("*(undefined8 *)", "*(double *)")
            if inner.strip() in u8: inner = asf(inner.strip(), "double")
        body = body[:m.end()] + inner + body[e:]
    body = re.sub(r"\(float\)(\w+)\b(?!\s*[\[(])", lambda m: asf(m.group(1), "float") if m.group(1) in u4 else m.group(0), body)
    body = body.replace("(float)f32(", "(float)(").replace("(double)f64(", "(double)(")
    #   `f32(E);` alone: a store nothing reads (the decompiler lost its reader -- the float
    #   arguments of pow and the like, tools/s2c_fix.txt)
    while True:
        m = re.search(r"(?<=[;{}:]\n)[ \t]*f(?:32|64)\(", body)
        if not m: break
        e = close2(body, m.end() - 1)
        body = body[:m.start()] + body[body.index("\n", e) + 1:]
    #   `X = f32(E);` into something that is not typed a float: the float's bytes go there
    def setf(m):
        return "%sS2_SETF%s(%s, " % (m.group(1), "" if m.group(3) == "32" else "D", m.group(2))
    body = re.sub(r"^([ \t]*)([^=;\n{}]+?) = f(32|64)\(", setf, body, flags=re.M)
    body = body.replace("f32(", "s2_f32bits(").replace("f64(", "s2_f64bits(")
    # strings and pointers to code/data by name
    def s_sym(m):                         # (`s_x_ADDR + n`: the string n bytes on -- 0x3c9ac's "attract" is "@attract" + 1)
        a = int(m.group(2), 16) + (int(m.group(3)) if m.group(3) else 0)
        e = IMG.index(b"\0", a)
        strings[a] = IMG[a:e]
        return f"0x{a:x}u/*\"{IMG[a:e].decode('latin1')[:24]}\"*/"
    body = re.sub(r"\b(s_\w*?_)([0-9a-f]{8})\b(?: \+ (\d+))?", s_sym, body)
    def g_arr(m):                         # (&DAT_x)[i]
        a = int(m.group(1), 16)
        ty = gl.get(a, ("", 4, "undefined4"))[2]
        return f"((%s *)MP(0x{a:x}u))[" % CT.get(ty.rstrip("_*"), "uint32_t")
    body = re.sub(r"\(&_?(?:DAT|INT)_([0-9a-f]{8})(?:/\*[^*]*\*/)?\)\[", g_arr, body)
    body = re.sub(r"&_?(?:DAT|INT)_([0-9a-f]{8})(?:/\*[^*]*\*/)?", lambda m: f"0x{int(m.group(1),16):x}u", body)
    def g_val(m):
        under, a = m.group(1), int(m.group(2), 16)
        if a in consts and not (0x119000 <= a):
            return lit(*consts[a])
        if m.group(3):                    # ._1_1_: one part of it -- from that byte, that many bytes
            off, n = (int(x) for x in m.group(3).strip("._").split("_"))
            gl_all.setdefault(a, "undefined4")
            return f"(*(%s *)MP(0x{a + off:x}u))" % {1: "uint8_t", 2: "uint16_t", 4: "uint32_t"}[n]
        name, size, ty = gl.get(a, ("", 4, "undefined4"))
        if ty.endswith("*"): ty = "pointer"
        if under or ty in ("undefined",):
            ty = {"float": "float", "double": "double"}.get(access.get(a), "undefined4")
        gl_all[a] = ty
        return f"(*(%s *)MP(0x{a:x}u))" % CT.get(ty, "uint32_t")
    body = re.sub(r"\b(_?)(?:DAT|INT)_([0-9a-f]{8})(?:/\*[^*]*\*/)?(\._\d+_\d+_)?", g_val, body)
    body = re.sub(r"\bPTR_\w*?_([0-9a-f]{8})\b", lambda m: f"(*(uint32_t *)MP(0x{int(m.group(1),16):x}u))", body)
    # a whole number written into one of the program's floats is its BITS (0x40800000 = 4.0:
    # the decompiler shows the store as it is in the code, a move of four bytes)
    body = re.sub(r"\(\*\(float \*\)MP\((0x[0-9a-f]+u)\)\) = (0x[0-9a-f]+|[1-9]\d*);", r"(*(uint32_t *)MP(\1)) = \2;", body)
    # an int kept for a moment in half of a double's place on the stack: nothing reads it
    body = re.sub(r"^\s*(\w+) = \(double\)CONCAT44\(\1\._4_4_,[^;]*\);\n", "", body, flags=re.M)
    # variables the engine keeps in its CODE object (the ground query's results)
    body = re.sub(r"\b_FUN_(000[0-9a-f]{5})\b(?!\()", lambda m: f"(*(int32_t *)MP(0x{int(m.group(1),16):x}u))", body)
    # a float compared with a hexadecimal number is compared as its BITS (cmp dword, imm)
    body = re.sub(r"\(int\)([A-Za-z_]\w*) (<=|>=|==|!=|<|>) (0x[0-9a-f]{8})\b", r"BITS(\1) \2 \3", body)
    body = re.sub(r"\b(0x[0-9a-f]{8}) (<=|>=|==|!=|<|>) \(int\)([A-Za-z_]\w*)", r"\1 \2 BITS(\3)", body)
    # ... an element of an array of them too (0x1c4fb: `(int)local_4c[i] < 0x3f800000`, a wheel's
    # share of the weight under 1.0 -- taken as a whole number it was always true, and the load
    # went the wrong way on the wheels that carry more)
    body = re.sub(r"\(int\)([A-Za-z_]\w*\[[^\]\n]+\]) (<=|>=|==|!=|<|>) (0x[0-9a-f]{8})\b", r"BITS(\1) \2 \3", body)
    body = re.sub(r"\b(0x[0-9a-f]{8}) (<=|>=|==|!=|<|>) \(int\)([A-Za-z_]\w*\[[^\]\n]+\])", r"\1 \2 BITS(\3)", body)
    # *(T **)(E): the address kept at E, itself to be read
    def close(text, i):                      # index of the bracket matching the one at i
        d = 0
        for k in range(i, len(text)):
            if text[k] == "(": d += 1
            elif text[k] == ")":
                d -= 1
                if d == 0: return k
        return -1
    while True:
        m = re.search(r"(\*?)\*\((\w+) \*\*\)\s*\(", body)
        if not m: break
        e = close(body, m.end() - 1)
        inner = "(*(uint32_t *)MP%s)" % body[m.end() - 1:e + 1]
        body = body[:m.start()] + ("*(%s *)MP%s" % (m.group(2), inner) if m.group(1) else inner) + body[e + 1:]
    # `(float)*(undefined4 *)(E)`: four bytes of no known type taken as a float are COPIED,
    # not converted (0x245a3 puts each corner's upward speed back after a hit: converted,
    # a car hit in the air got a speed of thousands of millions)
    body = re.sub(r"\(float\)\*\(undefined4 \*\)", "*(float *)", body)
    # a cast to a pointer followed by a bracket is an address being read
    body = re.sub(r"\((float|int|uint|undefined4|byte|char|short|ushort|undefined1|undefined2|double|undefined8|undefined|longlong|bool) \*\)\s*\(",
                  r"(\1 *)MP(", body)
    body = re.sub(r"\((float|int|uint|undefined4|byte|char|short|ushort|undefined1|undefined2|double|undefined8|undefined|longlong|bool) \*\)(?!MP\()([A-Za-z_]\w*)",
                  r"(\1 *)MP(\2)", body)
    # THE ROUTINE'S STACK FRAME is in the block too, every local at the offset its name
    # carries (local_9c is 0x9c below the return address): a structure the compiler laid
    # out on the stack is seen by the decompiler as several locals, and a routine that
    # fills it through the address of the first must find the others where they were.
    # So `&local` is an address like any other.  The routine becomes a body and a wrapper
    # that gives the stack back.
    stacked, frame, stack_ty = [], 0, {}
    def decl(m):
        nonlocal_frame[0] = max(nonlocal_frame[0], int(m.group(4), 16))
        ind, ty, name, arr = m.group(1), m.group(2), m.group(3), m.group(5)
        # eight bytes of no known type on the stack are a double (fstp qword): as a whole
        # number, 0x11253 -- the game's square root -- lost every fraction (S2-5za)
        if ty == "undefined8": ty = "double"
        # four bytes of no known type on the stack are a whole number WITH a sign: compared
        # with -1 as unsigned, the menu's carousel (0x336eb) never turned its loop
        if ty == "undefined4": ty = "int32_t"
        stacked.append((name, bool(arr)))
        stack_ty[name] = ty
        at = "s2_fp - 0x%s" % m.group(4)
        acc = "((%s *)MP(%s))" % (ty, at) if arr else "(*(%s *)MP(%s))" % (ty, at)
        return "#define %s %s\n#define %s_a (%s)" % (name, acc, name, at)
    nonlocal_frame = [0]
    # (an array of byte pointers on the stack is an array of addresses)
    body = re.sub(r"^(\s+)(?:undefined|undefined1|char|byte) \*((?:local|[a-z]+Stack)_[0-9a-f]+)( \[\d+\])?;$", r"\1uint32_t \2\3;", body, flags=re.M)
    body = re.sub(r"^(\s+)(?!return\b|goto\b)(\w+) ((?:local|[a-z]+Stack)_([0-9a-f]+))( \[\d+\])?;$", decl, body, flags=re.M)
    frame = (nonlocal_frame[0] + 15) & ~15
    for name, is_arr in stacked:
        # `(&local)[i]`: the local is the first element of what lies after it on the stack
        if not is_arr: body = re.sub(r"\(&%s\)\s*\[" % name, "((%s *)MP(%s_a))[" % (stack_ty[name], name), body)
        body = re.sub(r"\(int\)&%s\b|&%s\b" % (name, name), name + "_a", body)
        if is_arr:
            # `(int)(array + n)` counts in the array's elements: as the C pointer, then back
            while True:
                m = re.search(r"\(int\)\(%s\b" % name, body)
                if not m: break
                e = close2(body, m.start() + 5)
                body = body[:m.start()] + "PM" + body[m.start() + 5:e + 1] + body[e + 1:]
            body = re.sub(r"(?<!define )(?<!PM\()\b%s\b(?!_a|\s*\[| \(\()" % name, name + "_a", body)
    if stacked:
        m = re.search(r"^(\w[\w \*]*?)\s*\n?(FUN_[0-9a-f]{8})\(([^)]*)\)\s*\n\{", body, re.M)
        ret, fn, params = m.group(1).strip(), m.group(2), m.group(3)
        names = ",".join(q.split()[-1].lstrip("*") for q in params.split(",") if q.strip() and q.strip() != "void")
        sep = "," if names else ""
        body = body[:m.start()] + "static %s %s_body(%s%suint32_t s2_fp)\n{" % (ret, fn, params if names else "", sep) + body[m.end():]
        body += "\n" + "".join("#undef %s\n#undef %s_a\n" % (n, n) for n, _ in stacked)
        call = "%s_body(%s%ss2_push(0x%x) + 0x%x)" % (fn, names, sep, frame, frame)
        if ret == "void":
            body += "%s %s(%s)\n{\n  uint32_t sp = s2_sp;\n  %s;\n  s2_sp = sp;\n}\n" % (ret, fn, params, call)
        else:
            body += "%s %s(%s)\n{\n  uint32_t sp = s2_sp;\n  %s r = %s;\n  s2_sp = sp;\n  return r;\n}\n" % (ret, fn, params, ret, call)
    # a local that is a C pointer, handed on as an address: back to the number
    for name in set(re.findall(r"^\s+\w+ \*(\w+);", body, re.M)):
        body = re.sub(r"\(int\)\((%s\b[^()]*)\)" % name, r"PM(\1)", body)
        body = re.sub(r"\(int\)%s\b(?!\[)" % name, "PM(%s)" % name, body)
    body = re.sub(r"^(?!static )(\w[\w ]*?\**)\s*\n?(FUN_[0-9a-f]{8}\()", r"static \1 \2", body.lstrip("\n"), flags=re.M)
    for pat, rep in fixes.get(entry, []):
        body, n = re.subn(pat, rep, body)
        if not n: print(f"fix did not apply @{entry:05x}: {pat}", file=sys.stderr)
    # ... and the same of one of the program's own floats (the race's clock `< 0x40c00000` is `< 6.0`;
    # read as a number cut to an int it was always true: the opponents' handicap stayed 0)
    body = re.sub(r"\(int\)\(\*\(float \*\)(MP\(0x[0-9a-f]+u\))\) (<=|>=|==|!=|<|>) (0x[0-9a-f]{8})\b", r"(*(int32_t *)\1) \2 \3", body)
    body = re.sub(r"\b(0x[0-9a-f]{8}) (<=|>=|==|!=|<|>) \(int\)\(\*\(float \*\)(MP\(0x[0-9a-f]+u\))\)", r"\1 \2 (*(int32_t *)\3)", body)
    # `(float)` before one of the program's variables that the listing gives no type: its four
    # bytes ARE a float (the decompiler writes a conversion only for a typed whole number)
    def untyped(m):
        if gl.get(int(m.group(2), 16), ("", 4, "int"))[2].startswith("undefined"): return "((float *)MP(%su))%s" % (m.group(2), m.group(3))
        return m.group(0)
    body = re.sub(r"\(float\)\(\((u?int32_t) \*\)MP\((0x[0-9a-f]+)u\)\)(\[)", untyped, body)
    body = re.sub(r"\(float\)\(\*\((u?int32_t) \*\)MP\((0x[0-9a-f]+)u\)\)()", lambda m: untyped(m).replace("((float *)MP(", "(*(float *)MP(") if untyped(m) != m.group(0) else m.group(0), body)
    return body

# how a global is accessed, for the `_DAT_` ones the decompiler leaves untyped
access = {}
import subprocess
dis = subprocess.run(["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel", "--start-address=0x10000",
                      "--stop-address=0x52000", "build/screamer2_flat.bin"], capture_output=True, text=True).stdout
for m in re.finditer(r"\t(f[a-z0-9]+) +(DWORD|QWORD) PTR ds:0x(1[1-3][0-9a-f]{4})", dis):
    op, sz, a = m.group(1), m.group(2), int(m.group(3), 16)
    if op.startswith("fi"): continue
    access[a] = "double" if sz == "QWORD" else "float"

texts = {e: convert(e, t) for e, t in sorted(funcs.items())}

# a call that passes fewer arguments than the routine is written with (the decompiler gives a
# routine register parameters it does not use, and leaves them out at some calls): noughts
nparams = {}
for e, t in texts.items():
    m = None
    for m in re.finditer(r"^static [\w \*]+? (FUN_[0-9a-f]{8})\(([^)]*)\)\s*\n\{", t, re.M): pass
    if m:
        ps = [q for q in m.group(2).split(",") if q.strip() and q.strip() != "void"]
        nparams[m.group(1)] = len(ps)
def pad(t):
    out, i = [], 0
    for m in re.finditer(r"\b(FUN_[0-9a-f]{8})(_body)?\(", t):
        name = m.group(1)
        if m.group(2) or name not in nparams or m.start() < i: continue
        ls = t.rfind("\n", 0, m.start()) + 1
        if t[ls:m.start()].startswith("static "): continue          # its own heading
        d, k, commas, empty = 0, m.end() - 1, 0, True
        while k < len(t):
            c = t[k]
            if c == "(": d += 1
            elif c == ")":
                d -= 1
                if d == 0: break
            elif c == "," and d == 1: commas += 1
            elif d >= 1 and not c.isspace(): empty = False
            k += 1
        have = 0 if empty else commas + 1
        if have < nparams[name]:
            out.append(t[i:k]); out.append(("" if empty else ",") + ",".join(["0"] * (nparams[name] - have))); i = k
    out.append(t[i:])
    return "".join(out)
texts = {e: pad(t) for e, t in texts.items()}
# A ROUTINE'S ADDRESS AS A VALUE (the menu keeps the page it is on as one, 0x132494, and calls
# it: `(*(code *)x)()`): the address is a number, a call through one goes to s2_dispatch(),
# a switch over the routines so named (--dispatch, and any whose address is taken)
def fnptr(t):
    def num(m):
        dispatch.add(int(m.group(1), 16))
        return "0x%xu" % int(m.group(1), 16)
    t = re.sub(r"\(\*\(code \*\)(\((?:[^()]|\([^()]*\))*\))\)\(\)", r"s2_dispatch((uint32_t)\1)", t)
    t = re.sub(r"\(code \*\)(\((?:[^()]|\([^()]*\))*\)) == FUN_([0-9a-f]{8})\b", lambda m: "(uint32_t)%s == 0x%xu" % (m.group(1), int(m.group(2), 16)), t)
    t = re.sub(r"\((?:int|uint|int32_t|uint32_t|undefined4|code \*)\)FUN_([0-9a-f]{8})\b(?!\s*\()", num, t)
    return t
texts = {e: fnptr(t) for e, t in texts.items()}
# a constant the decompiler did not name: a double when the instruction that reads it says so
# (x87: DC / DD read eight bytes, D8 / D9 four -- 0x40a67's 150.0 is a double, read as a float
# it was 0 and the car page took one key only)
def ram_const(a):
    ref, size = a.to_bytes(4, "little"), 4
    k = IMG.find(ref, 0x10000, 0x100000)
    while k >= 0:
        if k >= 2 and IMG[k - 1] & 0xc7 == 0x05 and IMG[k - 2] in (0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf):
            size = 8 if IMG[k - 2] in (0xdc, 0xdd) else 4
            break
        k = IMG.find(ref, k + 1, 0x100000)
    if size == 8: return "%s/*0x%x*/" % (lit("double", "%.17g" % struct.unpack_from("<d", IMG, a)[0]), a)
    return "%s/*0x%x*/" % (lit("float", "%.9g" % struct.unpack_from("<f", IMG, a)[0]), a)

# A CONSTANT WRITTEN `(float)0.3` IS OFTEN THE PROGRAM'S DOUBLE: the decompiler casts a constant
# to the type of the sum it is in, but the x87 multiplies by the eight bytes it reads (fmul QWORD)
# -- 0.3 as a double and as a float differ in the last bits, and the revs (0x1f756) came out one
# bit off the original's in DOSBox.  In each routine: a `(float)<v>` whose v is a double the
# routine reads with a QWORD operand, and no float of that value with a DWORD one, is that double.
fpu_ops = re.compile(r"^\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t(f(?!i)[a-z0-9]+) +(DWORD|QWORD) PTR ds:0x(1[1-3][0-9a-f]{4})", re.M)
fpu_at = [(int(m.group(1), 16), m.group(3), int(m.group(4), 16)) for m in fpu_ops.finditer(dis)]
# (and a float the routine writes straight into its frame, `mov DWORD PTR [ebp-n],0x3fb33333`, then
# multiplies by -- the decompiler prints it as the nearest double: 0x20a18's 1.4 for the float 1.4)
frame_imm = re.compile(r"^\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\tmov +DWORD PTR \[ebp-0x[0-9a-f]+\],0x([0-9a-f]{8})$", re.M)
imm_at = sorted((int(m.group(1), 16), int(m.group(2), 16)) for m in frame_imm.finditer(dis))
double_fixed = {}
def double_consts(e, t):
    try: size = int(re.search(r"size (\d+)", open(f"{DEC}/f_{e:05x}.c").readline()).group(1))
    except (OSError, AttributeError): return t
    import bisect
    lo = bisect.bisect_left(fpu_at, (e, "", 0))
    dbl, flt = set(), set()
    for at, sz, a in fpu_at[lo:]:
        if at >= e + size: break
        if sz == "QWORD": dbl.add(struct.unpack_from("<d", IMG, a)[0])
        else: flt.add(struct.unpack_from("<f", IMG, a)[0])
    def one(m):
        v = float(m.group(1))
        for d in dbl:
            if d != 0.0 and abs(d - v) <= 1e-9 * abs(d) and struct.unpack("<f", struct.pack("<f", d))[0] != d:
                if struct.unpack("<f", struct.pack("<f", d))[0] in flt: return m.group(0)   # (a float of it too: cannot tell)
                double_fixed[e] = double_fixed.get(e, 0) + 1
                return "%s/*double*/" % lit("double", repr(d))
        return m.group(0)
    return re.sub(r"\(float\)([0-9][0-9.]*(?:e[+-]?[0-9]+)?)\b", one, t)
texts = {e: double_consts(e, t) for e, t in texts.items()}

# a float's BITS tested with & (`(f & 0x7fffffff) == 0`: is it nought), a 64-bit local's
# halves (`x._4_4_`), a program string indexed (`0x113020u/*"MRTRK"*/[i]`)
def leftovers(t):
    t = re.sub(r"\(\*\(float \*\)MP\(([^()]*)\)\) & ", r"(*(uint32_t *)MP(\1)) & ", t)
    t = re.sub(r"\b(\w+)\._4_4_ = ([^;]+);", r"\1 = (uint64_t)(uint32_t)(\1) | (uint64_t)(uint32_t)(\2) << 32;", t)
    t = re.sub(r"\b(\w+)\._0_4_ = ([^;]+);", r"\1 = ((uint64_t)(\1) & 0xffffffff00000000ull) | (uint32_t)(\2);", t)
    t = re.sub(r"\b(\w+)\._4_4_\b", r"((uint32_t)((uint64_t)(\1) >> 32))", t)
    t = re.sub(r"\b(\w+)\._0_4_\b", r"((uint32_t)(\1))", t)
    t = re.sub(r"\b0x([0-9a-f]+)u/\*(\"[^\n]*?\")\*/\[", r"((uint8_t *)MP(0x\1u))/*\2*/[", t)
    # a table of pointers indexed, then the pointer followed (`*(T *)(&PTR_x)[i] = `)
    t = re.sub(r"MP\(&\(\*\(uint32_t \*\)MP\((0x[0-9a-f]+u)\)\)\)\[([^\]]+)\]", r"MP(((uint32_t *)MP(\1))[\2])", t)
    # a local float array read as whole numbers (`(int)afStack_4c[i]`: its bits)
    t = re.sub(r"BITS\((\w+_a)\)\[((?:[^\[\]]|\[[^\[\]]*\])*)\]", r"((int32_t *)MP(\1))[\2]", t)
    # a constant of the program's the decompiler did not name: its value from the image
    t = re.sub(r"\bram0x00([0-9a-f]{6})\b", lambda m: ram_const(int(m.group(1), 16)), t)
    # a variable the decompiler called by its type letter and address
    ty = {"u": "uint32_t", "i": "int32_t", "f": "float", "b": "uint8_t", "c": "int8_t", "s": "int16_t", "us": "uint16_t", "p": "uint32_t"}
    t = re.sub(r"\b(us|[uifbcsp])Ram00([0-9a-f]{6})\b", lambda m: "(*(%s *)MP(0x%su))" % (ty[m.group(1)], m.group(2)), t)
    # a local's low byte set
    t = re.sub(r"\b(\w+)\._0_1_ = ([^;]+);", r"\1 = ((\1) & ~0xffu) | (uint8_t)(\2);", t)
    return t
texts = {e: leftovers(t) for e, t in texts.items()}
# A WHOLE NUMBER TURNED BY THE STEP (the menu's badges, logos, cars: angle += step x K, the
# fraction cut off each frame): s2_turn_by() keeps what is cut off when the step is under the
# 1/50 s the game never went below -- else the turns slow down at the port's faster frames
def turn_by(t):
    return re.sub(r"(\w+) = \(float\)\(\(float\)\*\(int \*\)MP\(((?:[^()]|\((?:[^()]|\([^()]*\))*\))*)\)\);\n(\s*)\1 = \(float\)\(\(\*\(float \*\)MP\(0x133350u\)\) \* ([0-9.]+f)(?:/\*0x[0-9a-f]+\*/)? \+ \1\);\n(\s*)\*\(int \*\)MP\(\2\) = \(int\)ROUND\(ROUND\(\1\)\);",
                  lambda m: "%s = (float)((float)*(int *)MP(%s));\n%s*(int *)MP(%s) = s2_turn_by((uint32_t)(%s), %s);" % (m.group(1), m.group(2), m.group(3), m.group(2), m.group(2), m.group(4)), t)
texts = {e: turn_by(t) for e, t in texts.items()}
# `x = FUN()` of a routine that gives nothing back (the caller was read as taking whatever
# was left in the registers): the call alone
voids = set()
for e, t in texts.items():
    for m in re.finditer(r"^static void (FUN_[0-9a-f]{8})\(", t, re.M): voids.add(m.group(1))
#   (and the routines written by hand that give nothing back)
_hand = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port", "src", "s2r_car.h")
if os.path.exists(_hand):
    for m in re.finditer(r"^static void (FUN_[0-9a-f]{8})\(", open(_hand).read(), re.M): voids.add(m.group(1))
def unvoid(t):
    return re.sub(r"\b(\w+) = (?:\(\w+\))?(FUN_[0-9a-f]{8})\(", lambda m: ("%s = 0; %s(" % (m.group(1), m.group(2))) if m.group(2) in voids else m.group(0), t)
texts = {e: unvoid(t) for e, t in texts.items()}

# A DOUBLE IN A SUM IS NOT ROUNDED TO A FLOAT FIRST.  The decompiler writes `(float)dVar10 / fVar1`
# where the machine code divides the double it holds (0x19cac: fld [len]; fdivr QWORD [q]) -- in C
# the cast is a rounding the game does not do (the tyres' push, last bits).  Within a sum the cast
# goes; as the whole of what is put into a float it rounds there anyway.
def double_operands(t):
    return re.sub(r"\(float\)(dVar\d+)\b(?!\s*;)", r"\1", t)
texts = {e: double_operands(t) for e, t in texts.items()}

# A CONSTANT IS THE NUMBER THE GAME READS, NOT ITS DIGITS.  Built for the x87 with
# -fexcess-precision=standard, C reads `0.8` and `3.14159274f` at the processor's full width:
# 0.8 to 64 bits, not the eight bytes the game's fmul QWORD reads; 3.14159274 itself, not the
# float the game keeps (0x19810: a quarter turn added as 1.5707964 put the wheels' push four
# float steps off).  And the listing prints a double to nine figures (0x1b92b compares with
# 1.570796370506286 and shows "1.57079637").  So a constant is matched to one the routine reads
# out of the data (fld / fmul ... DWORD or QWORD PTR ds:), and taken from there; one that is not
# read so is its type's nearest value.  Where that is not the digits, the number is written in
# hexadecimal, which holds it exactly.
from fractions import Fraction
inexact, unsure = [0], []
def routine_consts(e):
    try: size = int(re.search(r"size (\d+)", open(f"{DEC}/f_{e:05x}.c").readline()).group(1))
    except (OSError, AttributeError): return set(), set()
    import bisect
    lo = bisect.bisect_left(fpu_at, (e, "", 0))
    dbl, flt = set(), set()
    for at, sz, a in fpu_at[lo:]:
        if at >= e + size: break
        if sz == "QWORD": dbl.add(struct.unpack_from("<d", IMG, a)[0])
        else: flt.add(struct.unpack_from("<f", IMG, a)[0])
    for at, v in imm_at[bisect.bisect_left(imm_at, (e, 0)):]:
        if at >= e + size: break
        f = struct.unpack("<f", struct.pack("<I", v))[0]
        if 1e-6 < abs(f) < 1e7: flt.add(f)
    return dbl, flt
def exact_const(e, t):
    dbl, flt = routine_consts(e)
    def one(m):
        digits, suffix = m.group(1), m.group(2)
        x = float(digits)
        near = lambda c: c != 0.0 and abs(c - x) <= 2e-8 * abs(c)
        cd = sorted(c for c in dbl if near(c)); cf = sorted(c for c in flt if near(c))
        if suffix: pick = cf or cd
        else: pick = cd or cf
        if pick:
            v = pick[0]
            if len(set(pick)) > 1: unsure.append("%05x: %s could be any of %s" % (e, digits, pick))
        else:
            v = struct.unpack("<f", struct.pack("<f", x))[0] if suffix else x
        if Fraction(digits) == Fraction(v): return m.group(0)
        if suffix and struct.unpack("<f", struct.pack("<f", v))[0] != v: suffix = ""    # (a double: no f)
        inexact[0] += 1
        return v.hex() + suffix + "/*" + digits + "*/"
    out = []
    for part in re.split(r'("(?:[^"\\\n]|\\.)*")', t):        # (not inside a string)
        out.append(part if part.startswith('"') else
                   re.sub(r"(?<![\w.])(\d+\.\d*(?:[eE][-+]?\d+)?|\d+[eE][-+]?\d+)(f?)(?![\w.])", one, part))
    return "".join(out)
texts = {e: exact_const(e, t) for e, t in texts.items()}
for u in unsure: print(u, file=sys.stderr)
print("constants written exactly: %d" % inexact[0], file=sys.stderr)

def proto(body):
    m = None
    for m in re.finditer(r"^static ([\w \*]+?) (FUN_[0-9a-f]{8})\(([^)]*)\)", body, re.M): pass
    return f"static {m.group(1)} {m.group(2)}({m.group(3)});" if m else None

# a bare address that is a name in the program's data (a block's name handed to a hook)
for t in texts.values():
    for m in re.finditer(r"(?<!MP\()\b0x(11[0-8][0-9a-f]{3})u?\b(?! \+| \*)", t):
        a = int(m.group(1), 16)
        e = IMG.index(b"\0", a)
        if 2 <= e - a <= 24 and all(32 < c < 127 for c in IMG[a:e]) and IMG[a - 1] == 0: strings[a] = IMG[a:e]

with open(out, "w") as f:
    f.write("/* MADE BY tools/s2c.py FROM THE GAME'S ROUTINES -- do not edit: change the tool, its\n"
            "   fix file (tools/s2c_fix.txt) or the hooks (s2_hooks.c).  See the tool's header. */\n"
            '#include "s2_mem.h"\n\n')
    for e in sorted(externs):
        f.write(f"#ifndef HOOK_{e:05x}\nextern uint32_t FUN_{e:08x}();\n#endif\n")
    f.write("\n")
    for e in sorted(texts):
        p = proto(texts[e])
        if p: f.write(p + "\n")
        else: print(f"no prototype for {e:05x}", file=sys.stderr)
    if dispatch: f.write("static void s2_dispatch(uint32_t at);\n")
    f.write("\n")
    for e in sorted(texts):
        f.write(f"/* ---- 0x{e:05x} ---- */\n{texts[e]}\n\n")
    if dispatch:
        f.write("/* a call through an address (the menu's pages): the routines so called */\nstatic void s2_dispatch(uint32_t at)\n{\n    switch (at) {\n")
        for e in sorted(dispatch):
            zeros = ",".join(["0"] * nparams.get(f"FUN_{e:08x}", 0))
            f.write(f"    case 0x{e:05x}u: FUN_{e:08x}({zeros}); break;\n")
        f.write("    default: s2_dispatch_other(at); break;\n    }\n}\n")
    f.write("/* the names the routines hand to hooks */\nvoid s2_gen_strings(void)\n{\n")
    for a in sorted(strings):
        f.write(f'    s2_str(0x{a:x}u, "{strings[a].decode("latin1")}");\n')
    f.write("}\n")
if tables:
    with open(out, "a") as f:
        f.write("\n/* tables of the program's that the routines read (tools/s2_tables.txt) */\nvoid s2_gen_tables(void)\n{\n")
        for line in open(tables):
            if not line.strip() or line.startswith("#"): continue
            w = line.split()
            a0, n = int(w[0], 16), int(w[1], 16)
            f.write("    /* 0x%x: %s */\n" % (a0, " ".join(w[2:])))
            for a in range(a0, a0 + n, 4):
                v = struct.unpack_from("<I", IMG, a)[0]
                if v: f.write("    *(uint32_t *)MP(0x%xu) = 0x%08xu;\n" % (a, v))
        f.write("}\n")
print(f"{len(texts)} routines, {sum(t.count(chr(10)) for t in texts.values())} lines -> {out}")
print("hooks wanted:", " ".join(f"{e:05x}" for e in sorted(externs)))
print("to look at (stack / register artefacts):", " ".join(f"{e:05x}" for e in sorted(review)))
print("globals:", len(gl_all))
print("constants that are the program's doubles, not floats:", sum(double_fixed.values()), "in", len(double_fixed), "routines")
print("statements put back in order:", " ".join(f"{e:05x}:{n}" for e, (n, st) in sorted(reordered.items())))
for e, (n, st) in sorted(reordered.items()):
    for x in st: print(f"   NOT put back in {e:05x}: {x}")
