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

echo "installed Watcom compiler spec -> $EXT"
echo
echo "verify with:"
echo "  \$GHIDRA/support/analyzeHeadless /tmp/probe p -import <raw.bin> \\"
echo "      -processor x86:LE:32:watcom -loader BinaryLoader \\"
echo "      -loader-baseAddr 0x0 -noanalysis"
echo "expect: 'Using Language/Compiler: x86:LE:32:watcom:watcom'"
