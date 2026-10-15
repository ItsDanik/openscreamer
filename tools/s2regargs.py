#!/usr/bin/env python3
"""Screamer 2: how many register arguments a routine really takes (__watcall: EAX, EDX, EBX,
ECX in that order) -- which of the four it reads before it writes them, on any path, counting
what the routines it calls read (a value handed straight on is still an argument).

    python3 tools/s2regargs.py <entry>...        (needs capstone)

The decompiler misses an argument a routine only hands on (0x4eba5 opens a file in a container:
EAX the handle, EDX the container, EBX the entry's name, ECX the mode; it showed two).  The
engine's routines are answered by the port, so their calls must carry every argument.
Watcom's convention: a routine keeps every register but EAX (and what it returns in); the
prologue's pushes are saves, not reads.
"""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_REG, X86_OP_MEM, X86_OP_IMM

IMG = open("build/screamer2_flat.bin", "rb").read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True
ARGS = ["eax", "edx", "ebx", "ecx"]
FAMILY = {"eax": "eax", "ax": "eax", "al": "eax", "ah": "eax", "edx": "edx", "dx": "edx", "dl": "edx", "dh": "edx",
          "ebx": "ebx", "bx": "ebx", "bl": "ebx", "bh": "ebx", "ecx": "ecx", "cx": "ecx", "cl": "ecx", "ch": "ecx"}
memo = {}
LIMIT = 1                                       # how deep into the routines called (deeper: their own uses)

def insn_at(a):
    return next(md.disasm(IMG[a:a + 16], a), None)

def reads(entry, depth=0):
    if entry in memo: return memo[entry]
    memo[entry] = set()                        # (a call back into itself reads nothing new)
    found, seen, work = set(), set(), [(entry, frozenset())]
    while work:
        a, written = work.pop()
        written = set(written)
        prologue = a == entry
        while True:
            if (a, frozenset(written)) in seen: break
            seen.add((a, frozenset(written)))
            i = insn_at(a)
            if i is None: break
            m = i.mnemonic
            if m == "mov" and i.op_str == "ebp, esp": prologue = False
            regs_read, regs_write = i.regs_access()
            rd = {FAMILY.get(i.reg_name(r)) for r in regs_read} - {None}
            wr = {FAMILY.get(i.reg_name(r)) for r in regs_write} - {None}
            if m == "push" and prologue: rd = set()
            if m == "xor" and len(i.operands) == 2 and i.operands[0].type == X86_OP_REG and i.operands[1].type == X86_OP_REG \
                    and i.operands[0].reg == i.operands[1].reg: rd = set()
            found |= {r for r in rd if r in ARGS and r not in written}
            if m == "call":
                if i.operands[0].type == X86_OP_IMM and depth < LIMIT:
                    found |= {r for r in reads(i.operands[0].imm, depth + 1) if r not in written}
                written.add("eax")
            else:
                written |= wr
            if m in ("ret", "retf", "iret") or m == "jmp" and i.operands[0].type != X86_OP_IMM: break
            if m == "jmp":
                a = i.operands[0].imm; continue
            if m.startswith("j") or m.startswith("loop"):
                if i.operands[0].type == X86_OP_IMM: work.append((i.operands[0].imm, frozenset(written)))
            a += i.size
    memo[entry] = found
    return found

def at_calls(target, lo=0x10000, hi=0x4a000):
    """What the callers in lo..hi load into the four argument registers in the few
    instructions before each call to `target`: the most of them any caller sets."""
    best, n = 0, 0
    code = IMG[lo:hi]
    for k in range(len(code) - 5):
        if code[k] != 0xe8: continue
        a = lo + k
        rel = int.from_bytes(code[k + 1:k + 5], "little", signed=True)
        if a + 5 + rel != target: continue
        i = insn_at(a)
        if i is None or i.mnemonic != "call": continue
        n += 1
        # walk back: instructions that end exactly at `a`, up to 8 of them
        set_regs, at = set(), a
        for _ in range(8):
            prev = None
            for back in range(1, 12):
                j = insn_at(at - back)
                if j is not None and j.address + j.size == at: prev = j; break
            if prev is None or prev.mnemonic in ("call", "ret", "jmp") or prev.mnemonic.startswith("j"): break
            _, w = prev.regs_access()
            set_regs |= {FAMILY.get(prev.reg_name(r)) for r in w} - {None}
            at = prev.address
        best = max([best] + [ARGS.index(r) + 1 for r in set_regs if r in ARGS])
    return best, n

if __name__ == "__main__":
    for x in sys.argv[1:]:
        e = int(x, 16)
        out = []
        for LIMIT in (0, 1):
            memo.clear()
            r = reads(e)
            out.append(max([ARGS.index(k) + 1 for k in r] or [0]))
        c, n = at_calls(e)
        print(f"0x{e:05x} {out[0]} by itself, {out[1]} with what it calls; {c} set by its {n} callers")
