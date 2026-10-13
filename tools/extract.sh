#!/bin/sh
# Unpack the game's containers for the Python tools.
#   tools/extract.sh [GAME_DIR] [OUT_DIR]      default: screamer  build/dec
#   tools/extract.sh screamer2                 Screamer 2, into build/dec2
# GAME_DIR is a copy of the original game holding DATA/*.DAT and GFX/*.DAT
# (Screamer 2 also ANIMS/*.DAT and CONFIG/SETUP.DAT). Each container becomes
# OUT_DIR/<CONTAINER>/.
set -e
here=$(dirname "$0")
game=${1:-screamer}
case "$game" in *screamer2*) def=build/dec2 ;; *) def=build/dec ;; esac
out=${2:-$def}
[ -d "$game/DATA" ] && [ -d "$game/GFX" ] || { echo "extract.sh: no DATA and GFX under \"$game\"" >&2; exit 1; }
mkdir -p "$out"
for f in "$game"/DATA/*.DAT "$game"/GFX/*.DAT "$game"/ANIMS/*.DAT "$game"/CONFIG/SETUP.DAT; do
    [ -f "$f" ] || continue
    python3 "$here/dat_container.py" extract "$f" "$out" > /dev/null
    echo "unpacked $f"
done
echo "done: $(find "$out" -type f | wc -l) files in $out"
