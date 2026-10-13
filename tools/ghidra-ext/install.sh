#!/usr/bin/env bash
# Install the Watcom C/C++ compiler spec as a Ghidra user extension.
#
# Why: /opt/ghidra is root-owned, so the stock x86 language definition cannot be
# extended in place.  Ghidra also ships no Watcom calling convention (only
# gcc/win/borland/delphi), so the decompiler shows `in_EAX` / `unaff_EBX`
# instead of parameters for every Watcom-compiled function.
#
# This installs a *separate* x86 variant (x86:LE:32:watcom) into the writable
# per-user settings directory, reusing the stock sleigh language by symlink.
# Nothing under /opt/ghidra is touched, and deleting the extension dir undoes it.
#
# Usage:  tools/ghidra-ext/install.sh
set -euo pipefail

GHIDRA_INSTALL=${GHIDRA_INSTALL:-/opt/ghidra}
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/ScreamerWatcom"

# locate the per-user Ghidra settings dir (e.g. ~/.config/ghidra/ghidra_12.1.2_DEV)
SETTINGS=$(ls -d "${HOME}"/.config/ghidra/ghidra_* "${HOME}"/.ghidra/ghidra_* 2>/dev/null | head -1 || true)
if [ -z "$SETTINGS" ]; then
    echo "error: no Ghidra settings directory found; run Ghidra once first." >&2
    exit 1
fi
EXT="$SETTINGS/Extensions/ScreamerWatcom"
L="$GHIDRA_INSTALL/Ghidra/Processors/x86/data/languages"

if [ ! -f "$L/x86.slaspec" ]; then
    echo "error: $L/x86.slaspec not found; set GHIDRA_INSTALL." >&2
    exit 1
fi

mkdir -p "$EXT/data/languages"
cp -r "$SRC/." "$EXT/"

# symlink the stock sleigh sources so Ghidra can load x86.sla, but deliberately
# skip x86.ldefs and the stock *.cspec files -- copying those would register the
# same language id twice and clash with the installation.
for f in "$L"/*; do
    b=$(basename "$f")
    case "$b" in
        x86.ldefs|*.cspec) continue ;;
    esac
    ln -sfn "$f" "$EXT/data/languages/$b"
done

# the language itself: the stock one with the stores of floats kept visible
# (see x86watcom.ldefs).  ia.sinc is copied with four constructors changed and compiled.
rm -f "$EXT/data/languages/iaf.sinc" "$EXT/data/languages/x86f.slaspec"
python3 - "$L" "$EXT/data/languages" <<'PY'
import re, sys
L, E = sys.argv[1], sys.argv[2]
s = open(L + "/ia.sinc").read()
k = "define pcodeop segment;"
assert k in s
s = s.replace(k, "define pcodeop f32;        # a value rounded by a store to a float (kept visible)\ndefine pcodeop f64;\n" + k, 1)
n = 0
for w, op, size in (("m32fp", "f32", 4), ("m64fp", "f64", 8)):
    for ins in ("FST", "FSTP"):
        pat = re.compile(r"(:%s %s\s+is [^\n]*\n\{\s*\n\s*FPUInstructionPointer = inst_start;\n\s*)%s = float2float\(ST0\);" % (ins, w, w))
        s, c = pat.subn(lambda m: m.group(1) + "local rnd:%d = float2float(ST0); %s = %s(rnd);" % (size, w, op), s)
        n += c
assert n == 4, "ia.sinc is not as expected (%d of 4 stores found)" % n
open(E + "/iaf.sinc", "w").write(s)
open(E + "/x86f.slaspec", "w").write(open(L + "/x86.slaspec").read().replace('"ia.sinc"', '"iaf.sinc"'))
PY
( cd "$EXT/data/languages" && "$GHIDRA_INSTALL/support/sleigh" x86f.slaspec x86f.sla > x86f.log 2>&1 ) || { echo "error: sleigh failed, see $EXT/data/languages/x86f.log" >&2; exit 1; }

echo "installed Watcom compiler spec -> $EXT"
echo
echo "verify with:"
echo "  \$GHIDRA/support/analyzeHeadless /tmp/probe p -import <raw.bin> \\"
echo "      -processor x86:LE:32:watcom -loader BinaryLoader \\"
echo "      -loader-baseAddr 0x0 -noanalysis"
echo "expect: 'Using Language/Compiler: x86:LE:32:watcom:watcom'"
