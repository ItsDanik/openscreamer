#!/bin/sh
# Screamer 2: build the decompiler project from the game's program, and decompile the
# game-logic range into build/decomp2.  Takes a quarter of an hour.
#   tools/s2project.sh
set -e
cd "$(dirname "$0")/.."
G=/opt/ghidra/support/analyzeHeadless
python3 tools/unle.py screamer2/STARTL.EXE -o build/screamer2_flat.bin --map build/screamer2_layout.json --fixups > /dev/null
python3 tools/patch_fpu2.py build/screamer2_flat.bin build/screamer2_flat_fpu.bin
mkdir -p build/ghidra2 build/decomp2
python3 - <<'PY'
import json, struct
L = json.load(open('build/screamer2_layout.json')); img = open('build/screamer2_flat.bin', 'rb').read()
sites = L['fixup_sites']; S = set(sites); lo, hi = 0x10000, 0x10000 + 869383
seeds = set()
for s in sites:
    t = struct.unpack_from('<I', img, s)[0]
    if lo <= t < hi and not (lo <= s < hi and ((s + 4) in S or (s - 4) in S)): seeds.add(t)
open('build/ghidra2/seeds.txt', 'w').write('\n'.join(hex(x) for x in sorted(seeds)) + '\n')
PY
rm -rf build/ghidra2f && mkdir -p build/ghidra2f
$G build/ghidra2f screamer2 -import build/screamer2_flat_fpu.bin -processor x86:LE:32:watcom -loader BinaryLoader \
   -loader-baseAddr 0x0 -noanalysis -scriptPath tools -postScript CreateEntry.java 0xB3D10 > build/ghidra2f/import.log 2>&1
# The analysis FIRST, from the entry point alone, and the seeds after it: seeded before it,
# the case labels of the switches (their tables are in the code, and a table of one entry
# looks like any pointer) become functions and the switches are never put together --
# 37 routines came out in pieces that way.  SeedFuncs refuses what is inside known code.
$G build/ghidra2f screamer2 -process screamer2_flat_fpu.bin -analysisTimeoutPerFile 1800 > build/ghidra2f/analyze1.log 2>&1
$G build/ghidra2f screamer2 -process screamer2_flat_fpu.bin -scriptPath tools -preScript SeedFuncs.java build/ghidra2/seeds.txt \
   -analysisTimeoutPerFile 1800 > build/ghidra2f/analyze.log 2>&1
rm -f build/decomp2/f_*.c
$G build/ghidra2f screamer2 -process screamer2_flat_fpu.bin -noanalysis -scriptPath tools -postScript FixSigs.java tools/s2_sigs.txt \
   -postScript DecompRange.java build/decomp2 0x10000 0x52000 > build/ghidra2f/range.log 2>&1
grep -h -E 'SEEDS|SIGS|RANGE' build/ghidra2f/analyze.log build/ghidra2f/range.log | sed 's/^INFO *//; s/ (GhidraScript) *$//'
python3 tools/s2consts.py
