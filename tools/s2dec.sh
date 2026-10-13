#!/bin/sh
# Decompile Screamer 2 functions into build/decomp2/f_<entry>.c
#   tools/s2dec.sh 0x4b547 0x4f78e ...      (any address inside the function)
# Saves new functions into the project, so later runs know them.
cd "$(dirname "$0")/.."
/opt/ghidra/support/analyzeHeadless build/ghidra2 screamer2 -process screamer2_flat.bin -noanalysis \
    -scriptPath tools -postScript DecompTo.java build/decomp2 "$@" > build/ghidra2/dec.log 2>&1
grep 'DECOMP:' build/ghidra2/dec.log | sed 's/^INFO  DecompTo.java> //; s/ (GhidraScript) *$//'
grep -E 'ERROR' build/ghidra2/dec.log | head -3
