#!/bin/sh
# Unpack the game's containers into the directory the port reads.
#   tools/extract.sh [GAME_DIR] [OUT_DIR]      default: screamer  build/dec
# GAME_DIR is a copy of the original game holding DATA/*.DAT and GFX/*.DAT.
set -e
here=$(dirname "$0")
game=${1:-screamer}
out=${2:-build/dec}
[ -d "$game/DATA" ] && [ -d "$game/GFX" ] || { echo "extract.sh: no DATA and GFX under \"$game\"" >&2; exit 1; }
mkdir -p "$out"
for f in "$game"/DATA/*.DAT "$game"/GFX/*.DAT; do
    python3 "$here/dat_container.py" extract "$f" "$out" > /dev/null
    echo "unpacked $f"
done
echo "done: $(find "$out" -type f | wc -l) files in $out"
