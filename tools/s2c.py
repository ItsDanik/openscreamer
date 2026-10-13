#!/usr/bin/env python3
"""Screamer 2: turn decompiled routines into C the port compiles.

    python3 tools/s2c.py <out.c> <root entry>... [--hook <entry>...] [--fix tools/s2c_fix.txt]

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
taken, except the hooks.  `--fix` names a file of corrections for what the decompiler gets
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
roots, hooks, fixfile = [], set(), None
mode = "root"
for a in args:
    if a == "--hook": mode = "hook"
    elif a == "--fix": mode = "fix"
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

def read(entry):
    p = f"{DEC}/f_{entry:05x}.c"
    return open(p).read() if os.path.exists(p) else None

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
        if c in hooks or not (LO <= c < HI): externs.add(c)
        elif c not in funcs: todo.append(c)
externs |= hooks

def lit(kind, v):
    if kind == "double": return v if ("." in v or "e" in v or "n" in v) else v + ".0"
    return (v if ("." in v or "e" in v) else v + ".0") + "f"

review, strings, gl_all = [], {}, {}
def convert(entry, t):
    gl = {}
    for m in re.finditer(r"^// GLOBAL (\S+?)(?:/\*[^*]*\*/)? @([0-9a-f]+) (\d+) (\S+)", t, re.M):
        gl[int(m.group(2), 16)] = (m.group(1), int(m.group(3)), m.group(4))
    body = t[t.index("\n", t.rindex("// ", 0, t.index("\n\n"))):] if "\n\n" in t else t
    body = "\n".join(l for l in body.split("\n") if not l.startswith("//") and not l.startswith("/* WARNING"))
    if re.search(r"in_stack_|unaff_|extraout_|in_E[A-D]X|in_ST", body): review.append(entry)
    # strings and pointers to code/data by name
    def s_sym(m):
        a = int(m.group(2), 16)
        e = IMG.index(b"\0", a)
        strings[a] = IMG[a:e]
        return f"0x{a:x}u/*\"{IMG[a:e].decode('latin1')[:24]}\"*/"
    body = re.sub(r"\b(s_\w*?_)([0-9a-f]{8})\b(?: \+ \d+)?", s_sym, body)
    def g_arr(m):                         # (&DAT_x)[i]
        a = int(m.group(1), 16)
        ty = gl.get(a, ("", 4, "undefined4"))[2]
        return f"((%s *)MP(0x{a:x}u))[" % CT.get(ty.rstrip("_*"), "uint32_t")
    body = re.sub(r"\(&_?DAT_([0-9a-f]{8})(?:/\*[^*]*\*/)?\)\[", g_arr, body)
    body = re.sub(r"&_?DAT_([0-9a-f]{8})(?:/\*[^*]*\*/)?", lambda m: f"0x{int(m.group(1),16):x}u", body)
    def g_val(m):
        under, a = m.group(1), int(m.group(2), 16)
        if a in consts and not (0x119000 <= a):
            return lit(*consts[a])
        name, size, ty = gl.get(a, ("", 4, "undefined4"))
        if ty.endswith("*"): ty = "pointer"
        if under or ty in ("undefined",):
            ty = {"float": "float", "double": "double"}.get(access.get(a), "undefined4")
        gl_all[a] = ty
        return f"(*(%s *)MP(0x{a:x}u))" % CT.get(ty, "uint32_t")
    body = re.sub(r"\b(_?)DAT_([0-9a-f]{8})(?:/\*[^*]*\*/)?(\._\d+_\d+_)?", g_val, body)
    body = re.sub(r"\bPTR_\w*?_([0-9a-f]{8})\b", lambda m: f"(*(uint32_t *)MP(0x{int(m.group(1),16):x}u))", body)
    # variables the engine keeps in its CODE object (the ground query's results)
    body = re.sub(r"\b_FUN_(0004[0-9a-f]{4})\b(?!\()", lambda m: f"(*(int32_t *)MP(0x{int(m.group(1),16):x}u))", body)
    # a float compared with a hexadecimal number is compared as its BITS (cmp dword, imm)
    body = re.sub(r"\(int\)([A-Za-z_]\w*) (<=|>=|==|!=|<|>) (0x[0-9a-f]{8})\b", r"BITS(\1) \2 \3", body)
    body = re.sub(r"\b(0x[0-9a-f]{8}) (<=|>=|==|!=|<|>) \(int\)([A-Za-z_]\w*)", r"\1 \2 BITS(\3)", body)
    # a cast to a pointer followed by a bracket is an address being read
    body = re.sub(r"\((float|int|uint|undefined4|byte|char|short|ushort|undefined1|undefined2|double|undefined8|undefined|longlong|bool) \*\)\(",
                  r"(\1 *)MP(", body)
    body = re.sub(r"\((float|int|uint|undefined4|byte|char|short|ushort|undefined1|undefined2|double|undefined8|undefined|longlong|bool) \*\)(?!MP\()([A-Za-z_]\w*)",
                  r"(\1 *)MP(\2)", body)
    # a local that is a C pointer, handed on as an address: back to the number
    for name in set(re.findall(r"^\s+\w+ \*(\w+);", body, re.M)):
        body = re.sub(r"\(int\)\((%s\b[^()]*)\)" % name, r"PM(\1)", body)
        body = re.sub(r"\(int\)%s\b(?!\[)" % name, "PM(%s)" % name, body)
    body = re.sub(r"^(\w[\w ]*?\**)\s*\n?(FUN_[0-9a-f]{8}\()", r"static \1 \2", body.lstrip("\n"), count=1, flags=re.M)
    for pat, rep in fixes.get(entry, []):
        body, n = re.subn(pat, rep, body)
        if not n: print(f"fix did not apply @{entry:05x}: {pat}", file=sys.stderr)
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

def proto(body):
    m = re.match(r"static ([\w \*]+?) (FUN_[0-9a-f]{8})\(([^)]*)\)", body)
    return f"static {m.group(1)} {m.group(2)}({m.group(3)});" if m else None

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
    f.write("\n")
    for e in sorted(texts):
        f.write(f"/* ---- 0x{e:05x} ---- */\n{texts[e]}\n\n")
    f.write("/* the names the routines hand to hooks */\nvoid s2_gen_strings(void)\n{\n")
    for a in sorted(strings):
        f.write(f'    s2_str(0x{a:x}u, "{strings[a].decode("latin1")}");\n')
    f.write("}\n")
print(f"{len(texts)} routines, {sum(t.count(chr(10)) for t in texts.values())} lines -> {out}")
print("hooks wanted:", " ".join(f"{e:05x}" for e in sorted(externs)))
print("to look at (stack / register artefacts):", " ".join(f"{e:05x}" for e in sorted(review)))
print("globals:", len(gl_all))
