#!/usr/bin/env python3
"""
lzw.py - the LZW variant used by Screamer's .DAT containers (methods 1/2/3).

Reverse-engineered from the binary (2026-10-07), not guessed:

  init            FUN_0004d120  -> widths 9..12, masks 0x1FF/0x3FF/0x7FF/0xFFF,
                                   next_free = 0x102 (258), threshold = 0x200 (512)
  driver          FUN_0004d1bc
  per-code step   FUN_0004d284  -> CLEAR = 0x100 (256), END = 0x101 (257)
                                   bump when next_free >= threshold and width != 12
  bit reader      FUN_0004d604  -> value = (u24_le(bitpos >> 3) >> (bitpos & 7)) & mask
                                   i.e. LSB-first within each byte

So: classic LZW, LSB-first bit packing, 9-12 bit codes, clear/end codes 256/257,
first assignable code 258, width bumped when next_free reaches the current
threshold (512/1024/2048).
"""


def lzw_decode(src: bytes, expected: int | None = None,
               skip: int = 0, max_out: int | None = None) -> bytes:
    """Decode an LZW stream.  Stops at the END code, at `expected` bytes, or
    when the input runs out."""
    limit = len(src) * 8
    bitpos = skip * 8
    out = bytearray()

    # dictionary as (prefix_code, first_char); 0..255 are literals
    prefix = [0] * 4096
    suffix = [0] * 4096
    for i in range(256):
        prefix[i] = -1
        suffix[i] = i

    width = 9
    threshold = 0x200
    next_free = 0x102
    prev = -1

    def read_code():
        nonlocal bitpos
        if bitpos + width > limit:
            return -1
        v = 0
        for i in range(width):
            byte = src[bitpos >> 3]
            v |= ((byte >> (bitpos & 7)) & 1) << i
            bitpos += 1
        return v

    def emit(code, stack):
        """Walk the chain, pushing chars; returns the first char."""
        guard = 0
        while code > 0xFF:
            stack.append(suffix[code])
            code = prefix[code]
            guard += 1
            if guard > 4096 or code < 0:      # corrupt input: prefix cycle
                raise ValueError("LZW dictionary cycle")
        stack.append(suffix[code])
        return code

    while True:
        if expected is not None and len(out) >= expected:
            break
        code = read_code()
        if code < 0:
            break
        if code == 0x101:                       # END
            break
        if code == 0x100:                       # CLEAR
            width, threshold, next_free = 9, 0x200, 0x102
            prev = -1
            continue

        stack = []
        try:
            if code >= next_free:
                # KwKwK case: code == next_free
                if prev < 0:
                    break
                first = emit(prev, stack)
                stack.insert(0, first)          # first char of previous string
            else:
                first = emit(code, stack)
        except ValueError:
            break

        for ch in reversed(stack):
            out.append(ch)

        if prev >= 0 and next_free < 4096:
            prefix[next_free] = prev
            suffix[next_free] = first
            next_free += 1
            if next_free >= threshold and width != 12:
                width += 1
                threshold *= 2
        prev = code

        if max_out is not None and len(out) >= max_out:
            break

    return bytes(out)


if __name__ == "__main__":
    import sys
    sys.path.insert(0, __file__.rsplit("/", 1)[0])
    from dat_container import read_container
    path = sys.argv[1] if len(sys.argv) > 1 else "screamer/DATA/LOCAL.DAT"
    data, entries = read_container(path)
    print(f"{'name':<16}{'stored':>8}{'want':>8}{'got':>8}  ok   preview")
    print("-" * 90)
    good = 0
    for e in entries:
        if e["method"] == 0:
            continue
        blob = data[e["offset"]:e["offset"] + e["stored"]]
        best, bestskip = b"", 0
        for skip in range(0, 5):
            r = lzw_decode(blob, expected=e["uncompressed"], skip=skip)
            if len(r) > len(best):
                best, bestskip = r, skip
        ok = len(best) == e["uncompressed"]
        good += ok
        prev = best[:34].decode("latin-1").replace("\n", "|").replace("\r", "")
        print(f"{e['name']:<16}{e['stored']:>8}{e['uncompressed']:>8}{len(best):>8}"
              f"  {'OK ' if ok else 'BAD'} skip={bestskip}  {prev}")
    print(f"\n{good} stream(s) decoded to exactly the declared size")
