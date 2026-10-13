#!/usr/bin/env python3
"""
Screamer 2: put back in order what the decompiler prints out of order.

The game's vector arithmetic is inlined by its compiler as copies through temporaries on the
stack that are reached by pointers kept in other stack slots:

    fstp [ebp-0x2e8]               ; the centre's z
    mov eax,[ebp-0x2e8] / mov [ebp-0x2dc],eax        ; copied to a second temporary
    mov eax,[ebp-0x20c] / fld [eax+8] / ...          ; read back THROUGH A POINTER, stored

Ghidra shows the read through the pointer as the variable (`local_d0[2] = local_2f8;`) but
does not order it after the store it depends on: the store (`local_2f8 = local_304;`) is
printed later in the block, sometimes in the header of a following `for`.  Taken as written,
the read gets what the variable held before.  (0x245a3: the cars' outlines were scaled about
a centre whose two last coordinates were stale, and cars "touched" two lengths apart.)

The rule here: a local that is ASSIGNED ONCE in the whole routine and is read, in the same
block, before that assignment -- such a read can only be meant to follow it -- has its
assignment moved up to just before the first statement that reads it, if everything the
assignment uses is already set there.  What cannot be moved is reported.

    reorder(text) -> (text, [what was moved], [what could not be])
    python3 tools/s2order.py build/decomp2/f_245a3.c ...     # a report
"""
import re, sys

NAME = r"(?:local_[0-9a-f]+|[a-z]{1,3}Var\d+(?:_\d+)?|[a-z]{2}Stack_[0-9a-f]+)"
ASSIGN = re.compile(r"^\s*(" + NAME + r")\s*=(?!=)\s*(.*);\s*$", re.S)
BARRIER = re.compile(r"^\s*(?:case\b|default\s*:|goto\b|break\s*;|continue\s*;|return\b|\w+:\s*$)")

def logical(lines):
    """statements: source lines joined until the brackets close and the line ends one"""
    out, cur, depth = [], [], 0
    for l in lines:
        cur.append(l)
        s = re.sub(r"/\*.*?\*/|\"(?:\\.|[^\"])*\"|'(?:\\.|[^'])*'", "", l)
        depth += s.count("(") - s.count(")")
        e = s.rstrip()
        if depth <= 0 and (e.endswith((";", "{", "}", ":")) or e == ""):
            out.append("\n".join(cur)); cur, depth = [], 0
    if cur: out.append("\n".join(cur))
    return out

def parse(stmts, i=0):
    """a block: a list of ['s', text] and ['c', header, block, closer]"""
    block = []
    while i < len(stmts):
        t = stmts[i]; s = t.strip()
        if s.startswith("}"): return block, i
        if s.endswith("{"):
            inner, j = parse(stmts, i + 1)
            block.append(["c", t, inner, stmts[j] if j < len(stmts) else "}"])
            i = j + 1
        else:
            block.append(["s", t]); i += 1
    return block, i

DECL = re.compile(r"^\s*(?:[A-Za-z_]\w*\s+)+\*?\s*\w+(?:\s*\[\w+\])*\s*;\s*$")

def text_of(item):
    if item[0] == "s": return "" if DECL.match(item[1]) else item[1]
    return item[1] + "\n" + "\n".join(text_of(x) for x in item[2]) + "\n" + item[3]

def emit(block, out):
    for it in block:
        if it[0] == "s": out.append(it[1])
        else:
            out.append(it[1]); emit(it[2], out); out.append(it[3])

def reads(name, text):
    """is `name` read in text (its address taken, or being assigned, is not a read)"""
    for m in re.finditer(r"(?<![\w.])" + re.escape(name) + r"\b", text):
        before = text[:m.start()].rstrip()
        after = text[m.end():].lstrip()
        if before.endswith("&") and not before.endswith("&&"): continue
        if re.match(r"=(?!=)", after): continue
        return True
    return False

def assigns(name, text):
    return len(re.findall(r"(?<![\w.&])" + re.escape(name) + r"\s*(?:=(?!=)|[-+*/|&^]=|\+\+|--)", text))

def split_top(s, sep):
    parts, d, cur = [], 0, ""
    for c in s:
        if c in "([": d += 1
        elif c in ")]": d -= 1
        if c == sep and d == 0: parts.append(cur); cur = ""
        else: cur += c
    parts.append(cur)
    return parts

def hoist_for(block, whole, moved):
    """`for (a; X = e, Y = f, cond; step) {`: the assignments of once-assigned locals that the
    loop does not change come out in front of it"""
    k = 0
    while k < len(block):
        it = block[k]
        if it[0] == "c":
            hoist_for(it[2], whole, moved)
            m = re.match(r"^(\s*)for \((.*)\) \{\s*$", it[1], re.S)
            if m:
                parts = split_top(m.group(2), ";")
                if len(parts) == 3:
                    conds = split_top(parts[1], ",")
                    out, keep = [], []
                    body = "\n".join(text_of(x) for x in it[2]) + parts[2]
                    for c in conds[:-1]:
                        a = re.match(r"^\s*(" + NAME + r")\s*=(?!=)\s*(.*)$", c, re.S)
                        ok = False
                        if a and assigns(a.group(1), whole) == 1:
                            ops = set(re.findall(NAME, a.group(2)))
                            ok = not any(assigns(o, body) for o in ops) and a.group(1) not in ops
                        (out if ok else keep).append(c)
                    if out:
                        it[1] = "%sfor (%s;%s;%s) {" % (m.group(1), parts[0], ",".join(keep + [conds[-1]]), parts[2])
                        for c in out:
                            block.insert(k, ["s", m.group(1) + " ".join(c.split()) + ";"]); k += 1
                            moved.append("out of a for's header: " + " ".join(c.split())[:70])
        k += 1

def fix_block(block, whole, moved, stuck):
    for it in block:
        if it[0] == "c": fix_block(it[2], whole, moved, stuck)
    again = True
    while again:
        again = False
        for j, it in enumerate(block):
            if it[0] != "s": continue
            a = ASSIGN.match(it[1])
            if not a: continue
            name, expr = a.group(1), a.group(2)
            if reads(name, expr) or assigns(name, whole) != 1: continue
            first = next((i for i in range(j) if reads(name, text_of(block[i]))), None)
            if first is None: continue
            # not across a label, a case, a jump
            if any(block[i][0] == "s" and BARRIER.match(block[i][1]) for i in range(first, j)):
                stuck.append(f"{name}: a label or a jump between its use and its assignment"); continue
            ops = set(re.findall(NAME, expr))
            late = [o for o in ops if any(assigns(o, text_of(block[i])) for i in range(first, j))]
            if late:
                stuck.append(f"{name}: read before it is set, and {', '.join(sorted(late))} is set in between"); continue
            at = first
            while at > 0 and block[at][0] == "c" and block[at][1].lstrip().startswith("else"): at -= 1
            block.insert(at, block.pop(j))
            moved.append(" ".join(it[1].split())[:78])
            again = True
            break

def reorder(text):
    lines = text.split("\n")
    try:
        start = next(i for i, l in enumerate(lines) if l == "{")
        end = max(i for i, l in enumerate(lines) if l == "}")
    except (StopIteration, ValueError):
        return text, [], []
    body = lines[start + 1:end]
    block, _ = parse(logical(body))
    whole = "\n".join(body)
    moved, stuck = [], []
    hoist_for(block, whole, moved)
    out = []; emit(block, out)
    whole = "\n".join(out)
    fix_block(block, whole, moved, stuck)
    out = []; emit(block, out)
    return "\n".join(lines[:start + 1] + out + lines[end:]), moved, stuck

if __name__ == "__main__":
    for p in sys.argv[1:]:
        t, moved, stuck = reorder(open(p).read())
        if moved or stuck:
            print(f"{p}: {len(moved)} moved, {len(stuck)} not")
            for m in moved: print("   moved  ", m)
            for s in stuck: print("   STUCK  ", s)
