#!/bin/sh
# The version of this source: the number in VERSION, with its last number
# raised by one for every commit made since VERSION was last changed.
#   VERSION says 0.4.0b, three commits later:  0.4.3b
# So nobody has to touch a number when committing; to start 0.5, write
# 0.5.0b into VERSION.  Without git (a source archive) it is VERSION as it is.
dir=$(cd "$(dirname "$0")/.." && pwd)
base=$(cat "$dir/VERSION" 2>/dev/null || echo 0.0.0)
since=0
if git -C "$dir" rev-parse --git-dir >/dev/null 2>&1; then
    last=$(git -C "$dir" log -n 1 --format=%H -- VERSION 2>/dev/null)
    [ -n "$last" ] && since=$(git -C "$dir" rev-list --count "$last..HEAD" 2>/dev/null || echo 0)
fi
echo "$base" | awk -v n="$since" '{
    if (match($0, /^[0-9]+\.[0-9]+\.[0-9]+/)) {
        head = substr($0, 1, RLENGTH); tail = substr($0, RLENGTH + 1)
        split(head, p, "."); printf "%d.%d.%d%s\n", p[1], p[2], p[3] + n, tail
    } else print $0 }'
