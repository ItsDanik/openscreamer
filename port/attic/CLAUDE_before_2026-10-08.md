# OpenScreamer — porting *Screamer* (1995, MS-DOS) to Linux x86/ARM

Reverse-engineering + reimplementation project. Goal: a native SDL2/3 port for Linux
x86 and ARM, then a MiSTer hybrid core (ARM-side game logic, FPGA for 15 kHz video /
audio / input) following the `MiSTer-dethrace` / `MiSTer-ecwolf` pattern.

---

## Working agreement — read this first

**Danik does not write code or decompile.** He is the domain expert on the MiSTer and on
these ports, and he does verification and game-feel judgement. Do not hand him code to
review, patches to apply, or compiler errors to triage.

- **Claude:** all code, tooling, decompilation, builds, debugging. This machine has
  Ghidra 12.1.2 (`/opt/ghidra`), python3, wine, objdump. The `Read` tool can open PNGs,
  so *look at rendered output yourself* instead of asking him to describe it.
- **Danik:** running builds on real hardware, judging whether the game looks/feels like
  the original, and scope/licence/target decisions.

Prefer "here is a build and a screenshot" over "please check this function". Keep
everything buildable with one command. Explain findings in plain language, not jargon.

---

## What we have

`~/openscreamer` holds the full retail game (copied 2026-10-07):

```
STARTL.EXE  STARTH.EXE  SETUP.EXE  DOS4GW.EXE  SCREAMER.BAT/CFG/PIF
HMIDRV.386  HMIMDRV.386  HMIDET.386            <- HMI Sound Operating System
DATA/   GEN LOCAL MAP1-6 (+I variants) MUSIC SOUND .DAT
GFX/    GAME GRAF MENU MAP1-6{L,H} .DAT
CONFIG/ TABS/ REPLAYS/
```

`SCREAMER.BAT` launches `STARTH` (the SVGA build, which is reported to crash even on real
hardware). **`STARTL.EXE` is the usable target** and the one this project decompiles.
STARTH.EXE is a *separate link*, not a patched STARTL (entry point and ~405 K bytes differ).

Sound goes through the HMI middleware — that whole layer is discarded and replaced with
SDL audio, so it does not need reverse-engineering.

---

## The binary

`STARTL.EXE`, md5 `a83834098e965ebc4bde48d0162542b1`, 1,034,919 bytes.

Toolchain: **Watcom C/C++32** (embedded banner says 1988-1994). Format: MZ stub =
DOS/4GW real-mode loader (10,832 bytes, `e_lfanew` = 0x2A50) followed by an **LE**
("Linear Executable") image. Verified layout:

| obj | base | vsize | stored pages | role |
|---|---|---|---|---|
| 1 | `0x010000` | 898,979 | 220 | code |
| 2 | `0x0F0000` | 130 | 0 | BSS |
| 3 | `0x100000` | 291,520 | 7 | data + stack (ESP = 0x1472C0 = top of obj3) |

227 pages x `0x1000`, data pages at file offset `0x1A800`. Self-check: 226 x 4096 + 679
(last page) + 0x1A800 = 1,034,919 = exact file size.

**Entry VA `0x2FEB8`** (obj1 + 0x1FEB8) = Watcom CRT `_cstart_`; opens with `EB 76`
(`jmp short`) over its own copyright banner, then does DPMI setup (`swi(0x31)`), the
`NO87=` environment check, and a BSS zero-fill.

**Only one symbol survived linking** (the name `start`) — every function name must be
recovered by analysis.

---

## ADDRESS RELOCATION — SOLVED (this was the project's main blocker)

**References in the file are object-relative, not absolute.** The image must be relocated
by adding the *target object's* base:

- data-object reference: `target = stored_value + 0x100000`
- code-object reference: `target = stored_value + 0x010000`

Settled empirically, not by reading a spec. `tools/TestReloc.java` walks the **real**
disassembled instructions (51,032 of them, 26,316 scalar operands) and counts how often
each candidate transform lands exactly on one of the 365 string starts in the data object:

| transform | hits |
|---|---|
| `v` (absolute) | 0 |
| `v + 0x10000` | 0 (against strings; see below for code targets) |
| **`v + 0x100000`** | **755 — of which 639 are 32-bit operands, 113 are 8-bit noise** |
| `insn_addr + v` (self-relative) | 0 |
| `field_addr + v` (self-relative) | 0 |

The 8-bit hits are the noise floor (~1.25 % of 8-bit constants land in the string pool by
chance); 639 against an expected ~21 by chance is ~30x signal. Confirmed again by
`tools/RelocateRefs.java`, which created 639 data references and 718 code references
(validated against known function entry points).

**Do not trust byte-pattern scanning for this.** An earlier session concluded "addresses
are already absolute, no relocation needed" by scanning raw bytes for `B8`-`BF`/`68`
opcodes. That is not disassembly — it produces false positives and misses instruction
boundaries, and the conclusion was simply wrong. Always enumerate real operands via
Ghidra.

**Precision caveat:** the transform alone cannot distinguish a real pointer from a
coincidental constant whose value lands in the object. `0xFF + 0x100000` = the
"File %s is corrupted." string, so every 32-bit immediate `0xFF` produces a false
reference. Where precision matters, require the target to be a known string start or
function entry (as `RelocateRefs.java` does), rather than trusting the arithmetic.

### Relocation tables (for reference)

- fixup **page table** at LE+`0x4A2` — 227 dwords, monotonic from 0, ends ~`0x82E`
- fixup **record table** at LE+`0x832`, ~`0xA031` (41,009) bytes; records look like 7-byte
  units beginning `07 00 ...` with a <`0x1000` field that decreases within a page
- Exact record format is **not needed** — the empirical transform above is validated and
  sufficient. Do not burn time re-deriving the LX record layout.

---

## Toolchain — commands that actually work

```bash
# 0. one-time: install the Watcom calling convention (user-level, reversible)
tools/ghidra-ext/install.sh

# 1. extract a loadable flat image (undoes the LE page packing)
python3 tools/unle.py STARTL.EXE -o build/screamer_flat.bin --map build/screamer_layout.json

# 2. import into Ghidra as a raw 32-bit x86 image, using the Watcom cspec
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer \
    -import build/screamer_flat.bin -processor x86:LE:32:watcom \
    -loader BinaryLoader -loader-baseAddr 0x0 -noanalysis \
    -scriptPath tools -postScript CreateEntry.java

# 3. run auto-analysis now that the entry point is a defined function
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer \
    -process screamer_flat.bin -analysisTimeoutPerFile 1800

# 4. create relocated references, then decompile anything by address
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer \
    -process screamer_flat.bin -noanalysis -scriptPath tools -postScript RelocateRefs.java
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer \
    -process screamer_flat.bin -noanalysis -scriptPath tools \
    -postScript DecompFunc.java 0x10bfc
```

### The Watcom calling convention

`tools/ghidra-ext/ScreamerWatcom/` holds a compiler spec defining `__watcall`:
**integer/pointer arguments in EAX, EDX, EBX, ECX in that order**, further arguments on
the stack, return in EAX (EDX:EAX for 64-bit, ST0 for float), caller cleans the stack
(so `extrapop` is unknown), and EAX/EBX/ECX/EDX are all volatile while ESI/EDI/EBP are
preserved.

It is installed as a *separate x86 variant* (`x86:LE:32:watcom`) rather than by editing
the stock `x86.ldefs`, because `/opt/ghidra` is root-owned. The install script copies the
cspec/ldefs into `~/.config/ghidra/ghidra_<ver>_DEV/Extensions/ScreamerWatcom/` and
symlinks the stock sleigh sources alongside — deliberately **not** symlinking
`x86.ldefs` or the stock `*.cspec`, which would register the same language id twice and
clash with the installation. Deleting that directory undoes the whole change.

Verify with `grep "Using Language" ` on a headless run: expect
`x86:LE:32:watcom:watcom`.

Scripts in `tools/`: `unle.py` (LE extractor), `dat_container.py` + `lzw.py` (asset
extraction), `CreateEntry.java`, `DecompEntry.java`, `DecompFunc.java`, `DumpAsm.java`,
`ListStats.java`, `TestReloc.java`, `RelocateRefs.java`, `RefsByText.java`, `RefsToVA.java`.

Current state: **787 functions / 51,032 instructions** from the entry seed alone (~20 % of
the code object) — the game logic is object/message-driven and not reachable from the CRT
by direct calls. Expect to seed more entry points.

### Pitfalls that cost time — do not rediscover these

- **Ghidra has no LE/LX loader.** Its MZ loader reads only the 10,832-byte stub and
  reports the remaining ~1 MB as "extra bytes"; analysis then finishes in 1 second and
  decompiles nothing. Always import the *flat image*, never the original EXE.
- **PyGhidra is unavailable in headless.** Python `-postScript` fails with "Ghidra was
  not started with PyGhidra". Write headless scripts as **Java**.
- **Java postScripts need `-scriptPath <dir>`**, or Ghidra refuses even an absolute path.
- **`img[a:a+n] = chunk` shrinks a bytearray** if `chunk` is short. This silently
  truncated the flat image by 3,417 bytes (the short final page). `unle.py` now asserts
  on it — keep that assert.
- **Ghidra ships no Watcom cspec** — only gcc/win/borland/delphi — so by default the
  decompiler emits `in_EAX` / `unaff_EBX` noise instead of parameters for every function.
  **Solved:** `tools/ghidra-ext/install.sh` installs an `x86:LE:32:watcom` language
  variant into the per-user Ghidra settings dir (see below). `~/.ghidra`/`/opt/ghidra` is
  root-owned, so the stock language definition cannot be extended in place.
- **Never pipe headless output through `tail`** — summaries are at the top and get lost.
  Write logs to a file.

---

## Asset files — CONTAINER FORMAT SOLVED

Every `.DAT` is a container. Layout, verified against all 35 data files:

```
[ asset data ]
[ directory: N records of 32 bytes ]
[ uint32 directory_size ]        <- last 4 bytes of the file
```

Each record is **16-byte name + 4 uint32**. The name is **XOR-0x3A encoded** and
NUL-terminated; a real NUL is *stored* as `0x3A`, so unused name padding decodes as `':'`
— that is an artifact, never data. The four fields are `offset`, `stored_size`,
`uncompressed_size`, `method`, where **method 0 = stored verbatim** and non-zero =
compressed (methods 1, 2 and 3 all occur). The directory is terminated by a record with
an empty name, so it can be located from the file tail alone.

Tool: `tools/dat_container.py list|extract <file.dat> <outdir> [name...]`.

| container | contents | method |
|---|---|---|
| `GFX/GRAF.DAT` (46) | `.PCX` screens — **extract and view directly** | 0 |
| `GFX/GAME.DAT` (38) | `.PKC` `.SPR` `.FNT` | 2 |
| `GFX/MENU.DAT` (17) | `MENU.COL` `MENU.MLB` + menu `.3DX` objects | mixed |
| `GFX/MAPn{L,H}.DAT` (23-30) | `.MMP .UNR .COL .MLB` + track art | mixed |
| `DATA/GEN.DAT` (5) | `CARS.DTR` `SPFXFULL.DTR` `MENU.DTR` `SFXSBASE.DTR` | 3 |
| `DATA/LOCAL.DAT` (7) | `.TXT` text data (ATTR, CARDAT, CHAMP, GAME, MENU, SCORE, TRACKDAT) | 3 |
| `DATA/MAPn.DAT` (15) | `MAPnA-D.DTR`, `MAPn.MRK`, `MAPn{ANM,CAM,CNG,OPP,OST,RPL,SLA}.SPL` | 0 |
| `DATA/MUSIC.DAT` (9) | `DRUM.BNK` `MELODIC.BNK` `MUSICn.HMP` (HMI music) | 3 |
| `DATA/SOUND.DAT` (92) | raw 8-bit unsigned PCM — `CRY.RAW`, `HELI.RAW`, ... | 0 |
| `CONFIG/SETUP.DAT` (14) | setup program assets | mixed |
| `TABS/*.TAB` | **not containers** — raw perspective / visibility lookup tables | - |

**The `.3dx` objects are not files on disk** — they live inside `GFX/MENU.DAT` and the
`GFX/MAPn*.DAT` containers, which is why the game looks for `%s.3DX` that is never on the
filesystem.

Confirmed end to end: extracted the 46 entries of `GFX/GRAF.DAT`, converted 38 PCX to PNG
with Pillow (`build/png/`), and **visually inspected them** — `LOAD1L.png` is the Virgin
Interactive loading screen, `CHAMP0L.png` a race car on a track. Real game art.

### The decompressor — SOLVED (LZW)

Methods 1/2/3 are all **LZW**, read out of the binary rather than guessed:

| role | function | detail |
|---|---|---|
| init | `FUN_0004d120` | widths 9..12; masks `0x1FF/0x3FF/0x7FF/0xFFF`; `next_free = 0x102`; `threshold = 0x200` |
| driver | `FUN_0004d1bc` | loops `FUN_0004d284` until it returns 0 |
| per-code step | `FUN_0004d284` | `CLEAR = 0x100`, `END = 0x101`; bump when `next_free >= threshold && width != 12` |
| bit reader | `FUN_0004d604` | `value = (u24_le(bitpos>>3) >> (bitpos & 7)) & mask[width]` |
| byte fetch | `FUN_000c5dcb` | also the routine that reports "File %s is corrupted." |

i.e. **classic LZW, LSB-first bit packing, 9-12 bit codes**, clear/end 256/257, first
assignable code 258, width bumped when `next_free` reaches 512/1024/2048. The stream
starts at the entry's offset — **no header byte** (`skip = 0`).

Implementation: `tools/lzw.py`, wired into `tools/dat_container.py extract`, which now
decompresses automatically.

**Validated: every compressed stream in all 35 containers decodes to exactly the size
declared in its directory record — zero mismatches.** Spot-checked content is real: track
names (`PALM TOWN`, `LAKE VALLEY`), car data (`SHADOW|0|340|2|1|0`), and Italian debug
strings (`Il samoiedo più grande del mondo` — the developers were Italian, matching
Graffiti/Milestone).

The track art containers also hold the scene's **`.3DX` objects** (`AJET.3DX`,
`AWBUS.3DX`, `AZ1PORTA.3DX`, `AUTO00-06.3DX`, ...), and `MAPn.BCK` decompresses to a
**PCX backdrop** — converted and viewed: sky, clouds and a city skyline.

The data object (VA `0x100000`) holds **338 strings**, starting `01 01 01 00 0A` then a
newline-delimited pool — the cheapest index to the engine's subsystems (`file/IO` 54,
`game` 71, `3D engine` 25, `sound` 25, `video` 11).

---

## First findings from the decompiler

**`FUN_00010bfc` is the `.3dx` object loader** — it is the only function referencing all
three of `%s.3DX`, `3D_HEADER` and `3D_BLKOBJ`. Reading it gives the engine's object
table layout:

- a global object table with **28-byte (`0x1c`) entries**; iteration bound is in EBX
- entry `+0x00` object name, `+0x0C` pointer to the allocated runtime object,
  `+0x10` = `+0x0C` + 0x40, `+0x14` = state (`1` = needs loading, later replaced by the
  loaded object pointer)
- allocation size is `vertex_count * 0x20 + 0x60` (`local_38 = *(obj+0x14)`,
  `local_34 = local_38 * 0x20 + 0x60`)
- a global free-slot table at `DAT_0002153c`, terminated by `0xFFFFFFFF`, with the next
  free index in `DAT_00007f78`; filename buffer at `DAT_00007fe4`
- error returns: `2` = open failed, `3` = alloc failed, `5` = alloc failed

Other located anchors: `screamer.cfg` -> `FUN_0001bf28`. `DATA/MAP1.DAT` contains IEEE
floats (1.0f, 0.6f near offset 0x410) and repeating 12-byte fixed-point coordinate
records around 0x1001 — not yet confirmed.

### The loader chain

- `FUN_00010bfc` -> `FUN_0004be4d` -> `FUN_0004bee8`, plus `FUN_0004d0e4` (a
  `sprintf`-alike: formats, then NUL-terminates at `dest + returned_length`).
- `FUN_0004be4d` is the open/parse pair: opens via `FUN_000c5d35`, sets an error flag
  (`ctx[0x2b] = 1`) and returns `-1` on failure, otherwise calls the parser.
- `FUN_0004bee8` is the **header parser**. It XORs bytes with `0x3A` in place while
  scanning a NUL-terminated string at `ctx+0x58` — this is where the container's
  XOR-0x3A name encoding came from (see Asset files). Failure paths set `ctx[0x2b] = 2`
  ("File %s is corrupted.") and `FUN_000c5dcb` is the routine that reports it. It then
  derives flags from `ctx[0x1d]` bits 0 and 1 and offsets from `ctx[0x1a]`/`ctx[0x1c]`.
- `FUN_000c5d35` is trivial — it just calls `FUN_000c5d1a`.

**`3D_HEADER` and `3D_BLKOBJ` are allocation tags, not file magic.** With the Watcom
cspec installed they appear directly in the decompiled loader:

```c
local_14 = FUN_0004c5a0(stream);                              // header size
local_1c = FUN_0002ec88(pool, 0x3b6 /* "3D_HEADER" */, local_14);
FUN_0004c2a5(stream, local_1c, local_14);                     // read header into it
local_38 = *(int *)(local_1c + 0x14);                         // object count
local_34 = local_38 * 0x20 + 0x60;                            // 32-byte blocks + 96-byte preamble
iVar3    = FUN_0002ec88(pool, 0x3c0 /* "3D_BLKOBJ" */, local_34);
FUN_0004c9c8(blocks, handleIndex);                            // then load the blocks
```

Earlier they were invisible because without the calling convention the decompiler could
not attribute the register-passed arguments. This is the single clearest demonstration of
why the cspec mattered.

The full loader, `FUN_00010bfc(int *objtab, uint first, int count)`:

- scans a global handle table at `DAT_0002153c` (terminated by `0xFFFFFFFF`) for a free slot
- walks object-table entries **28 bytes (`0x1c`) apart**: `+0x00` name, `+0x0C` pointer to
  the loaded object, `+0x10` = that pointer + `0x40`, `+0x14` state
- an entry with **state == 1 needs loading**; on success the state is replaced by the
  pointer to the `3D_HEADER` allocation
- builds `"%s.3DX"` into the filename buffer at `DAT_00007fe4`, then calls
  `FUN_0004be4d(stream, dir, name, 0x3b3)` — note `0x3b3` is the substring `".3DX"`
  (inside `"%s.3DX"` at `0x3AC`), i.e. the container entry suffix
- error returns: `2` = open failed, `3` = header alloc failed, `5` = block alloc failed

### `.3DX` object format — SOLVED

Confirmed by rendering: `AWBUS.3DX` renders as a solid box with a cab step, `AAUTO00.3DX`
as a recognisable low wedge-shaped supercar. `tools/render3dx.py` does it.

```
file header
  +0x10  0xFFFFFFFF
  +0x14  N              object count
  +0x18  N x uint32     offsets to the object blocks (object 0 always at 0x330)

object block
  +0x00  id
  +0x0C  offset used by the runtime loader to reach a sub-structure
  +0x10  a              vertex reference count   (AWBUS: 28)
  +0x14  b              face count               (AWBUS: 7)
  +0x18  flags
  +0x1C  FILE OFFSET OF THE VERTEX ARRAY       <- the key field
  +0x20  0xFFFFFFFF
  +0x24  offset list
        name, NUL-terminated
        ... per-octant face draw-order table at the end of the block
```

**Vertices** are `a` **int16 xyz triplets**, 6 bytes each, at the offset stored in
`+0x1C`.

**Faces** are `b` **36-byte records**. The table's position is given by the per-octant
draw-order table at the end of the block, whose entries are the records' byte offsets
from the block start (stride 36):

```
face record (36 bytes)
  +0x00  int16   constant within a file
  +0x02  int16   constant within a file
  +0x04  int16   n    corner count -- 4 = quad, 3 = triangle
  +0x06  (n+1) x int16   CLOSED LOOP of corner vertex BYTE OFFSETS
                         (divide by 6 for the vertex index; the n+1'th entry
                          repeats the first, closing the loop)
  +0x10  ...     normal / material / shading data
```

The mesh is therefore **indexed and shared-vertex**, not non-indexed. `a = 4b` happens
for some objects only because their vertices happen to be stored pre-ordered per face —
that was a coincidence, and an earlier version of this document wrongly generalised it.

**Do not reorder the face's vertices.** An earlier note here claimed the corners are
stored in "2x2 grid order" and must be drawn as `0,1,3,2`; that was an artefact of
reading a *non-indexed* vertex array directly. With the real face records the stored
loop is already the correct winding — use it as-is. Verified: the car now renders as a
clean solid body and the bus is unchanged.

At the end of each block is the **per-octant face draw-order table**: 8 rows of `b`
indices, each row terminated by `-1`, every row a permutation. That is the classic
convex-object trick of precomputing back-to-front face order for each of the 8 octants.

Because the mesh is indexed, the earlier "`a = 4 * b` for quad objects" rule is not a
property of the format — it holds only for objects whose vertex array happens to be in
face order. Use the face records, not that rule, to drive a renderer.

Block names are part names within a model: `fauto` front, `rauto` rear, `MV_mauto`
engine, `MV_map1d` track scenery.

**Normals** are float triplets (unit vectors) — e.g. 581 floats at `AAUTO00` offset
`0xFB4`. They are a separate array from the vertices; they are not used by the renderer
above but will be needed for shading.

**Still open:** the `.PKC`/`.SPR`/`.FNT` image formats, and `.MMP`/`.UNR`/`.MLB`.

---

### Track data — SOLVED

**The `.DTR` files are plain readable TEXT.** A large free win — the track's game logic
needs no reverse-engineering at all:

```
TRACK_DATA
*	color	camera_hi	bonus_time	carnage
	d24	d300	d50	d20
END_LIST

OPPONENTS
*	max_spd	accel	brakes	skill	sband
	f14000	f1100	f2000	f3500	f0.0003
END_LIST
```

Values carry a **type prefix**: `d` = decimal, `f` = fixed/float (a literal `.` also
occurs), `x` = hex, `s` = string. So `sazcono` is the string `azcono`, matching
`AZCONO.3DX`. `MAP1DAT.DTR` lists the track's scenery objects, animations and sound cues.
Its section heading is literally spelled `OSTACLES` — a typo in the shipped data; do not
"fix" it and then wonder why a lookup fails.

**`.SPL` files are linked-list splines** — the track's actual geometry:

```
header   8 bytes: [int32 count][int32 0]
entry   56 bytes:
  +0x00  int32  marker (1)
  +0x04  int32  X
  +0x08  int32  Y
  +0x0C  int32  Z          <- elevation: O(thousands) while X/Y are O(100000)
  +0x10  int32  NEXT INDEX <- linked list; the chain returns to node 0
  +0x14  int32  X          } repeated twice more --
  +0x18  int32  Y          } presumably the spline's in/out handles
  +0x1C  int32  0          }
  +0x20  int32  X
  +0x24  int32  Y
  +0x28  ...    0
```

**The nodes are NOT stored in path order — you must follow `NEXT`.** Verified: in
`MAP1CAM.SPL` the chain from node 0 visits all 14 nodes and returns to 0. Plotting in file
order instead yields a meaningless star of crossing lines, which cost a render iteration.

**Not all splines are simple paths — check before plotting.**

| spline | what it actually is |
|---|---|
| `SLA` | **the track's LEFT and RIGHT EDGES, interleaved.** Node 2k and 2k+1 are partners, ~2,617 units apart (range 2,202-2,919) — that separation *is* the road width. Plotting in node order produces a zigzag; split even/odd to get two parallel edge curves. |
| `CAM` | a single clean path (`next` scatters, chain closes on 0). Roughness sd/avg = 0.29. |
| `CNG` | still **unverified** — also looks interleaved (as-traversed sd/avg 2.06, dropping to 1.35 when split even/odd), but the split is still rougher than SLA's 0.36, so the pairing rule is not yet confirmed. Do not assume it is the centreline. |
| `OPP` / `OST` | opponents |
| `ANM` / `RPL` | animation / replay |

The diagnostic that settled it: measure the ratio of the standard deviation of segment
lengths to their mean, and compare against a known-good path (`CAM`, 0.29). `SLA`
as-traversed scored 0.98; split even/odd it scored 0.36, matching a clean curve. **Use
that test before plotting any spline** — a "zigzag" in the output means the interleaving
was not accounted for, not that the data is wrong.

`tools/render_track.py` plots the loops. The road surface is the ribbon enclosed by the
two `SLA` edges — `build/png/track1_road.png` fills it.

`MAP1.MMP` is **decoded** — see "Scenery" below; the 47 % zero / 34 % `0xFF` reading was
its sparse `-1` sentinels. Other track files, not yet decoded: `MAP1.UNR` (662 K — the 192
object meshes `V` indexes), `MAP1.MLB` (2.6 M), `MAP1.SMG` (16 K of sequential bytes — a
lookup table), `MAP1.COL` (768 B — a VGA palette, the same ramp seen in `MENU.DAT`),
`MAP1.BCK` (a PCX backdrop).

---

## Plan

Vertical slice, not a full static map first — a map cannot be verified and would be
rewritten anyway. Milestones ordered by how quickly Danik can see something:

1. ~~Crack the uncompressed formats~~ — **DONE**
2. ~~Get the decompressor + container format out of the code~~ — **DONE** (LZW)
3. ~~Render a model~~ — **DONE**. `.3DX` geometry solved; `tools/render3dx.py`
4. ~~Render a track~~ — **DONE**. `.SPL` splines solved; `tools/render_track.py`
5. ~~**A real binary**~~ — **DONE**. `port/` builds an SDL2 program that loads the real
   `.SPL` track data and drives a perspective camera along the road. See below.
6. Add what makes it look like Screamer: the `.3DX` scenery placed via `MAPnDAT.DTR`,
   the `MAPn.BCK` backdrop, `MAPn.COL` palette, textures
7. Then: driving, opponents, sound, menus -> MiSTer hybrid core

---

## The port (`port/`)

```
port/CMakeLists.txt       SDL2 via pkg-config
port/src/spl.h,  spl.c    .SPL loader (file order / chained / edge-pair split)
port/src/mesh.h, mesh.c   .3DX object loader; mesh_load_at() also parses a UNR object
port/src/unr.h,  unr.c    MAPn.UNR: blob header + the 192 object meshes
port/src/scene.h,scene.c  MAPn.MMP placement + MAPnDAT.DTR OSTACLES names
port/src/main.c           road tessellation, software rasteriser, camera, main loop
```

```bash
cmake -S port -B port/build && cmake --build port/build
port/build/openscreamer build/dec/MAP1 1              # window, track 1
port/build/openscreamer build/dec/MAP5 5 --shot out.ppm 90   # headless PPM frame
```

Arguments are positional: `datadir track [--shot file] [t] [camera-height] [layer]`.
`camera-height` is **absolute world units** above the road — 600 is about 1.8 m, which is
the default. It used to be a multiple of the road width, which made widening the road
raise the camera by the same factor and render the road pixel-identical; do not go back to
that. `layer` is **1** for the roadside scenery (the default), **0** for the on-road
class, **3** for both, and **2** to draw no scenery at all — that last one is the A/B
control that decides whether a visual artefact comes from the scenery or from the road.

Environment switches, all read at startup, for A/B testing without a rebuild:

| var | default | what it does |
|---|---|---|
| `OS_ROADZ=0` | 1 | take the road's elevation from `MAPnCAM.SPL` instead of the tiles |
| `OS_OBJZ=0`  | 1 | treat the record height as an *offset to the road* (the old, wrong reading) |
| `OS_ONROAD=0`| 1 | keep the scenery whose footprint crosses the road |
| `OS_ROLL=0`  | 1 | draw the lane-marking ribbons standing up, as authored |
| `OS_ROADDUMP=<n>` | off | print the road's height profile every n samples |
| `OS_OBJDUMP=<r>` | off | print every placed object within r units of the camera |
| `OS_ROADW`, `OS_ROADMAT`, `OS_GROUNDMAT`, `OS_ROADUCYC`, `OS_TEXSCALE`… | see the code | the tuning knobs from the texture sessions |

The last group is the pile of knobs that accumulated while the texture mapping was being
guessed at. The height and rotation fields are now **read out of the engine**, so they
should not need knobs; treat any new knob as a sign that something is still being guessed.

`--shot <file> <node>` renders one frame with no display — that is how this gets
verified, and how it will be tested on the MiSTer. Danik can run the windowed mode and
drive with the arrow keys.

**The single most important thing to know:** the splines are **control points, not a
mesh**. Track 1 has only 14 edge pairs spanning ~127,000 units, so one section is
~30,000 long and 2,600 wide — drawn literally it collapses to a hairline at the horizon.
The road must be resampled. `road_build()` interpolates the centreline and the half-width
vector with Catmull-Rom, `SUBDIV` sub-quads per section, and the camera drives along the
resampled centreline. Before this was added the binary rendered an empty screen and cost
several debugging rounds; do not remove the tessellation.

Design choices to keep: 320x200 software framebuffer scaled to the window (matches the
original's low-res look and is what a MiSTer hybrid core wants), painter's algorithm
rather than a z-buffer, SDL2 not SDL3 (the MiSTer ARM toolchain is SDL2-based).

### Scenery — `MAPn.MMP` SOLVED

The executable's own strings name the track files, which is how this was cracked:

```
%s.UNR   opened with allocation tags  OBJ_LST (object list), UNR_FILE (blob)
%s.MMP   the placement grid          %s.MLB = MATERIALS   %s.COL = palette
```

**`MAPn.MMP` is the placement table** — a two-level 256x256 grid of offsets into
itself. `FUN_0004c904` is the giveaway: it walks the grid and adds the file base to
every entry that is not `-1`, i.e. it relocates offsets into pointers. The `-1`
sentinels are why an entropy scan saw the file as "34% 0xFF, 47% zero".

```
+0x0000   256 x int32    level-1 offsets, -1 = empty row
row y:    256 x int32    level-2 offsets, -1 = empty cell
```

#### A cell is a 40-byte header of FIVE (base, count) pairs

```
base[i]  = i32(cell + 0x00 + 4i)
count[i] = i32(cell + 0x14 + 4i)          i = 0..4
```

This pairing is read straight out of the game's own code, and it is why `FUN_0004c904`
relocates exactly the first five words and no more:

* `FUN_000120c0` — a **nearest-object query**: walks a 3x3 block of cells and measures
  squared distance. Uses `(+0x08, +0x1C)` with a **12-byte** element stride and
  `(+0x0C, +0x20)` with a **28-byte** stride. In both, **X = element+4, Y = element+8**.
* `FUN_00082e88` — a **frustum culling** pass. Uses `(+0x10, +0x24)` and reads
  `elem[0], [2], [3], [5]` as a bounding box.
* `FUN_00082bcc` is the same culling pass without the grid lookup.

#### The five arrays are laid out back to back, and the chain holds 182/182

```
cell + 0x28              n0 + n1 records, 64 bytes each
  + 64*(n0+n1)           layer 2, 12-byte elements
  + 12*n2                layer 3, 28-byte elements
  + 28*n3                layer 4
```

**Layers 0 and 1 are the SAME array split at `n0`.** `base[0]` is always `cell+0x28`
(182/182), and `base[1] == cell + 0x28 + 64*n0`.

#### The 64-byte record

646/646 of track 1's layer-1 records carry `-1` at `+0x20`, which is what pins the
stride.

```
+0x00 V   object index -- spans 0..191, and MAPn.UNR declares exactly 192 objects
+0x04 1
+0x08 X
+0x0C Y   THE HEIGHT.  See below -- this is not a rotation and not an offset.
+0x10 Z   the second ground axis (what the port calls world Y)
+0x14 0
+0x18 T   still undecoded; 0..2047, steps of 28.5 within a family
+0x1C 0
+0x20 -1
```

#### `+0x0C` IS THE OBJECT'S HEIGHT — read out of `FUN_00083164`

The earlier reading of this field ("E", "a rotation", "+ = a normal object") was wrong.
`FUN_00083164` walks the cell's records with a 64-byte stride and does this:

```c
iVar6 = *(int *)(rec + 8)  - *(int *)(cam + 0xc);
iVar7 = *(int *)(rec + 0xc) - *(int *)(cam + 0x10);
iVar8 = *(int *)(rec + 0x10) - *(int *)(cam + 0x14);
d2 = iVar7*iVar7 + iVar6*iVar6 + iVar8*iVar8;      /* one 3D distance */
if ((d2 >> 2) < 676000000) { push (d2, record); }  /* ~52000-unit draw radius */
```

Three coordinates subtracted from the camera's position and squared-summed: a plain 3D
distance. `+0x08` and `+0x10` are separately the two grid axes (the 809/809 self-test), so
they are the ground plane and **`+0x0C` is the height**.

Tallies with the data: along the lap the height is a **staircase in 512-unit steps** from
`-4096` to `+4096` — a 16-level height grid, clamped at the ends. `FUN_00083164` also
zeroes `+0x04` on every record it visits, so that field is a per-frame flag, not data.

`+0x14` and `+0x1C` are 0 in every record of track 1, so there is no roll or pitch to
read. `+0x18` is not a heading in any convention tried (2048/4096/8192/65536 per turn,
phase swept) — still open.

#### THE ROAD'S ELEVATION COMES FROM THE TILES, NOT FROM `MAPnCAM.SPL`

`MAPnCAM.SPL`'s third field (the port called it "elevation", range -3569..+4620) is a
smooth curve and it is **not the road**: a road tile that is visibly ON the road can sit
2000 units below it (measured: track 1, `V23` at 52% of the lap, 2235 units apart). The
CAM spline is the camera path.

The road's height is the tiles' `+0x0C`. Measured against the **SLA** centreline (the
road's actual plan — `SLA`'s own Z is a constant 2, i.e. the plan is flat and the
elevation lives in the MMP), the road tiles are all within the road, and their height
along the lap is a clean profile:

```
lap  0- 3%   0            lap 46-50%   -128 -> 0
lap  3-12%   descends     lap 50-66%   0 -> +1050   (the hill)
             0 -> -4096   lap 66-74%   +1050 -> 0
lap 12-35%   -4096        lap 74-100%  0
lap 35-46%   climbs back to 0
```

There is one gap: no road tile at all between 20% and 32% of the lap (fill with the
nearest, which is -4096 either side).

**THE PROFILE MUST BE INTERPOLATED, NOT SNAPPED — the steps are not real.** Assigning
each road sample its nearest tile's height gives the road a visible 512-unit staircase.
Danik, first thing he said when he saw it: *"the visible steps are wrong, it should be
smooth."* The right construction is one `(arc length, height)` key per road tile and the
piecewise-linear curve through them (then a light 3-tap smooth at the corners). The 512
breakdown is then a property of how the track was *authored*, not a shape in the road.

**The objects keep their quantised heights**, and that is correct: a tile placed on a
level sits at that level. But *paint* does not — see the roll below.

**The check that proves the model:** an object's base — its record height `+0x0C` plus
the mesh's local minimum Y — lands on that field to within **±11 units**, because the
meshes are centred on their origin and the record height is the mesh's *centre*. Hundreds
of records land within a rounding error. Nothing else gives that.

`tools/mmp_dump.py` and the port's `OS_ROADDUMP=<n>` print the profile.

**A "road tile" is any mesh that uses the road material (MLB record 0)** — `V0 V1 V4 V5
V9 V10 V23 V28-V32 V38-V40 V49 V50 V55-V59 V71 V75-V77`. Selecting them by "flat mesh"
instead misses the whole staircase, because most of them carry a 256-unit kerb.

#### ROAD PAINT LIES DOWN — the lane markings are ribbons, not strips

Danik: *"no road markings (lanes) are visible."* They were being drawn, and they were
impossible to see. The marking meshes are **thin ribbons authored standing up** —
`V44`/`V117`/`V118` are `4136 x 258 x 0`: 4136 along the road, 258 **tall**, nothing
thick. Drawn as authored they are a 0.8 m wall standing in the lane, and from a chase
camera that is edge-on: a hairline. Laid down they are what they obviously are, a 0.8 m
wide painted stripe, and their keyed texture supplies the dash gaps.

The rule, all three parts measured rather than guessed:

* local Z extent <= 64 (no thickness), and
* local Y extent in (0, 700] — under 0.7 m, i.e. ribbon-scale, not a wall, and
* placed inside the road (`distance < half_width`)

→ draw it rolled 90 degrees: local Y becomes the across-road direction and local Z
becomes up. `V44 V47 V54 V26 V27 V117 V118` and the edge stripes `V84-V87 V145-V148` all
qualify, at laterals of ±0.3 to ±1.0 half-widths — lane lines and edge lines.

**And a rolled piece takes the ROAD's height, not its record height.** The record height
is one of the 512-unit levels, and `V44`'s records say 306 for every one of their 13
placements while the road under them is wherever the ramp happens to be; keeping it
floats the stripe up to a metre off the tarmac. Paint is placed at the road height plus
an 8-unit lift to stay out of the road quads.

Hmm — one thing this does **not** explain: only the centre line shows clearly. If the
original divides the road into four lanes, there should be three lines; the data has
markings at ±0.9 half-widths too, so the edge stripes should be appearing. Worth
checking against the original.

#### THE PROOF THAT MATTERS: every record sits in its own grid cell

The grid is an *index by position*, so a record's coordinates must select the very cell
it lives in, by the game's own formula `idx = (coord + 0x100000) >> 13`:

```
X at +0x08 -> level-2 index,   Y at +0x10 -> level-1 index
809 of 809 records land in their own cell  (100.0%)
swapped pairing: 30 of 809  (3.7%)
```

A wrong stride or a wrong coordinate offset gives a near-random cell index. 100% cannot
happen by accident. **This is the check to re-run after touching the parse** —
`tools/mmp_dump.py` prints the per-layer lateral statistics.

#### The two classes, and why they must never be mixed up

| | records | inside the road | median lateral | max |
|---|---|---|---|---|
| layer 0 | 163 | **53%** | 0.91 half-widths | 3.71 |
| layer 1 | 646 | 8% | 3.80 half-widths | 22.65 |

**Layer 0 is the on-road class and layer 1 is the roadside scenery.**

An earlier version of `scene.c` read `i32(cell)` and then `+0x08/+0x10` — that is the
*first record of layer 0*, with the coordinates right and the count ignored. It put 182
on-road markers around the track, 53% of them mid-road, and looked exactly like "boxes in
the middle of the road". Do not go back to reading one record per cell.

**The on-road records are real, not a parse error.** ~50 of layer 1 land within the road;
they are a series propagated along the lap (small `V`, negative `E`, and `T` stepping by a
constant **57** between neighbours), i.e. road pieces and start-line furniture, which
belong on the centreline by construction. No field separates them — `E < 0` drops 182 and
still leaves 25; `V < 10` drops 48 and leaves 35. `main()` therefore drops by geometry and
says so; `want_layer` 3 keeps them for inspection.

Note the half-width must be taken **per road sample**: it varies 2202..2919 units around
track 1, so a global mean misjudges the narrow sections.

### `MAPn.UNR` — SOLVED (the meshes `V` indexes)

Read out of `FUN_0004c744`, the loader, which leaves nothing to guess:

```c
FUN_0004be4d(stream, "%s.UNR");            // open
FUN_0004c2a5(stream, &blob_size, 4);       // read 4 bytes
FUN_0004c2a5(stream, &count, 4);           // read 4 bytes
objs = FUN_0002ec88(pool, "OBJ_LST",  count*4 + 0x1e4);
blob = FUN_0002ec88(pool, "UNR_FILE", blob_size);
FUN_0004c2a5(stream, objs, count*4);
*(int *)(count*4 + objs) = 0xffffffff;                    // terminator
for (k = 0; k < count; k++) objs[k] += blob;              // relocate
FUN_0004c2a5(stream, blob, blob_size);
```

```
+0x00  int32            blob_size
+0x04  int32            object_count            (192 for track 1)
+0x08  count x int32    offsets, RELATIVE TO THE BLOB
blob                    starts at 8 + 4*count
```

The arithmetic closes exactly: `776 + 660976 == 661752 ==` the file size of
`MAP1.UNR`, and the offsets are strictly increasing. **An earlier read of this header was
off by one dword** (the table starts at `+8`, not `+12`), which is why every object
looked like nonsense for so long — object 0 was being read 7216 bytes too far in.

#### Each object is a `.3DX`-shaped file in miniature

```
+0x10  0xFFFFFFFF
+0x14  block count      (1 for every object in track 1)
+0x18  block offsets    ([0x330] -- the same block offset .3DX always uses)
block   +0x10  a = vertex count
        +0x14  b = face count
        +0x1C  vertex array offset, relative to the object
faces   36-byte records, exactly as in .3DX
```

**All 192 objects parse.** `tools/mmp_dump.py`'s sibling test: max 189 vertices, 144
faces. `port/src/unr.c` reads the file and `mesh_load_at()` parses each object with the
unchanged `.3DX` block parser — no new mesh code was needed.

#### What the objects actually are

86 of the 192 are **flat planes**, and that is the tell:

| | extent | reads as |
|---|---|---|
| 110 v, 52 f | X ±3079, **Y = 0**, Z ±2053 | horizontal **road-surface tile** |
| 110 v, 52 f | X ±2317, **Y = 0**, Z ±3235 | horizontal road tile |
| 30 v, 16 f | X ±2068, Y ±517, **Z = 0** | vertical **wall / barrier** |
| 8 v, 2 f | X ±1034, Y ±1034, **Z = 0** | small sign panel |

Flat in local Y is a road tile; flat in local Z or X is a vertical panel. **This is the
track's own structure, not decorative scenery** — which is why some `MAPn.MMP` records
land dead on the centreline: they are the road surface itself.

So the port **skips meshes with no vertical extent** (`mesh_is_horizontal()`) — it already
synthesises the road and ground from the `.SPL` splines, and drawing the tiles again just
z-fights grey slabs across the view. The skip is a property of the mesh, not a guess.

#### Orientation: the axis is measured off the mesh, not guessed

The record carries no usable heading — `+0x0C` is the height, `+0x14` and `+0x1C` are
always 0, and `+0x18` is not a heading in any convention swept (2048/4096/8192/65536 per
turn, phase swept: the best is 30% within 20° where random is 11%). So the angle is
derived from the track.

**Which local axis gets aligned to the tangent is not the same for every mesh.** That is
what put pieces across the road: the barrier family (`V2`/`V3`, `4136 x 1034 x 0`) is long
in **local X** and flat in Z, so "align local Z with the road" stood it *across* the road
— which is what swallowed the camera on the downhill at t=60 and t=110. The road tiles
and the other walls (`V133`, `0 x 516 x 4136`) are long in local Z.

The rule now: measure the mesh's own local X and Z extents and align whichever is longer
(`yaw = tangent` for long-X, `tangent − 90°` for long-Z). Note `+0x0C` was previously
believed to be a rotation and is actually a height — see the record section above.

**Status:** positions and heights are proven, meshes are real, **the heading is
approximated from the track.** Judge placement and shape, not final look.

#### The on-road test uses the piece's FOOTPRINT, not its origin

Testing only the origin is not enough: these meshes reach up to 9308 units from their
centre, so a piece whose centre is clear of the road can still lie right across it. The
test is `distance_to_centreline < half_width + mesh_reach`, with `half_width` taken per
road sample.

It stays on, and it has to: with it off, the grandstand-scale meshes (8000-12000 units)
box the camera in — at t=60 and t=110 the view is inside a building.

**Colour-keyed overlays are exempt** (`mesh_is_overlay`). A lane marking is a thin strip
whose job is to lie ON the road, so the test deleted every lane line on the track.
Material 25 is **59% palette index 0** and material 20 is **34%** — a transparent
background, i.e. the colour key. `OS_ONROAD=0` disables the test for an A/B.

#### AND IT MUST TEST HEIGHT, or the bridges and the tunnel vanish

Danik: *"There are bridges that the player goes under in the original, also there is a
tunnel. These do not render in our port."* — and the reason was this filter. A bridge deck
spans the road exactly the way a wall does, so "drop everything that crosses the road"
deleted every one of them.

The rule that separates them is **height**, and it only became available once `+0x0C` was
understood as the height:

```
drop  <=>  footprint reaches across the road
       AND the piece's vertical span intersects the driving band
       AND it is not paint
```

A bridge carries a big record height — the elevated structures on track 1 run
**2269..12486**, far past the ±4096 the terrain tiles are clamped to — so its base clears
the band and it is kept. The pieces that used to box the camera in sit AT road level
(`e` in the ±2048 range) and those are the ones dropped. The band here is
`road_z + 150 .. road_z + 3000`; the camera rides at +600.

**The exemption for paint is the measured one — thin ribbon, on the road — NOT "uses a
keyed material".** The keyed materials are also on the crowd stand (`V153`, 4 m tall) and
the plastic structures, so exempting everything keyed is exactly what left *"random props
and barriers in the middle of the track"* (Danik). Only paint is exempt now.

#### THE BRIDGES: a flat deck 1792 units ABOVE the road

*"Some decorations are now floating in the air ... but not something that resembles a
bridge."* Both halves of that were one mistake: the port skipped **every flat mesh** as
"road or ground, already synthesised". They are not all road.

Track 1's bridges are three pieces:

```
V135  x27   6204 x 0 x 4136   flat deck   at +1792 above the road
V136  x 9   6456 x 0 x 3950   flat deck   at +1792       (the curved end pieces)
V133  x27   0 x 516 x 4136    parapet     at +2048  = deck + 256
V134  x27   0 x 516 x 4136    parapet     at +2048
```

The decks sit exactly **between** their two parapets — the parapet pair is 6144 units
apart, the deck's centre is on the midpoint, its 6204-unit width spans them — and 6144 is
the road width, so the bridge is exactly as wide as the road and the player drives under
it. Verified on the third group: parapets at `y = -51725` and `y = -57869`, decks at
`y = -54797`, the midpoint, three times over.

So the rule is **flat AND at the road's own height** = a terrain tile to skip.  The window
is **±1200 units, measured not picked**: the ground tiles are quantised differently from
the road tiles and legitimately sit up to **931** units off the road's height — at ±700
one of those was classified as a deck and drawn, which put a *"grass ceiling above our
heads"* (Danik). The decks are at 1792, so anything between 931 and 1792 divides the two
cleanly. Flat and anywhere else is a deck, a platform or a ceiling, and gets drawn.
Dropping the decks is what left the parapets hanging in mid-air.

**The driving band is 150..1200 units above the road** (3.6 m; the camera rides at 600).
A ceiling above that is passed *under* and must not count as an obstruction — with the
band reaching to 3000 the decks were deleted again by the very test added to save the
bridges.

### Textures — PARTIAL. Read this before spending a session on it.

#### The face record is fully decoded

```
face (36 bytes)
  +0x00  int16   n        corner count
  +0x06  (n+1) x int16     closed loop of corner byte offsets (/6 = index)
  +0x10  10 x int16:
            #0  always 0
            #1  Nx   #2 sign x   } the face normal, LOCAL space, 16384 = 1.0
            #3  Ny   #4 sign y   |   (14-bit fixed point; a face pointing
            #5  Nz   #6 sign z   }    straight down reads -16383)
            #7  MATERIAL
            #8  always 0
            #9  always 0
```

The sign words are **0 when the component is 0 and -1 when it is not** — verified
on every sampled face, e.g. object 0 is `Nx=0 Ny=-16383 Nz=0` with signs
`0, -1, 0`. So the tail is a sign+magnitude encoding of the normal, and the
material is a separate field.

`mesh_load_at()` reads the normal into `mesh.normal[]` and the value at `+0x1e`
into `mesh.material[]`, and the port **shades with the stored normal** rather
than a cross-product of the winding. That is exact data, and unlike the
cross-product it cannot be thrown off by a collapsed or duplicated corner.

#### `FUN_0004ce50` is a PCX LOADER, not a texture loader

Worth writing down because the name invites the wrong conclusion. It reads a
128-byte header, takes the image box from the int16 pair at `+4/+6` and
`+8/+10`, decodes with the classic PCX run rule — `(b & 0xC0) == 0xC0` means
"next byte, repeated `b & 0x3f` times" — and then reads `0x300` bytes of VGA
palette. The caller passes the string built from `0x1016f2`, which is `"%s.BCK"`,
and `MAP1.BCK` begins `0A 05 01 08`, a PCX, whose header gives exactly 512x256.

So: **`MAPn.BCK` is a PCX, and that whole function is the backdrop loader.** It is
not the texture path.

#### What MAPn.MLB actually contains

```
+0x0000   16-byte records:  [size, record_offset, colourA, colourB]
                            size is a POWER OF TWO (16 or 64 seen)
                            colourA/colourB are cross-checked below
record_offset -> 12-byte record:  [1, N, data_offset]
data_offset -> RLE data that decodes to exactly size*size pixels
```

Every one of the 91 size-64 records RLE-decodes to exactly 4096 pixels, and the
run counts are sane (200..3500 bytes for 4096 pixels), so the chain is real.

`MAPn.COL` is a genuine 768-byte VGA palette (it opens `(0,0,0) (3,2,1) (7,5,3)
(12,8,5)`, a brown ramp). `MAPn.SMG` is 16384 bytes of the byte sequence
`0,1,2,3,...` — i.e. a 256x64 **identity shade table**, the classic shape for a
software renderer's lighting LUT, here with no shading baked in. `colourA` and
`colourB` are near-equal pairs in many records (`50,50`, `96,80`, `254,32`) and
look like light/shadow palette indices, but that is a guess and is not relied on.

#### The one missing link, and what has been RULED OUT

The face's `MATERIAL` value is constant per object, takes 95 distinct values on
track 1, and ranges 1046..16772 — always **even**, so it indexes a 2-byte-aligned
array. Nothing tried so far resolves it:

| hypothesis | why it fails |
|---|---|
| elevation | track 1 is dead flat (centreline Z = 2 everywhere) while this spans −3840..+3072 |
| a heading | swept 2048/4096/8192/65536 per turn with the phase free: all at or near chance |
| an offset into MAPn.MLB | tested at every shift −12..+12: no alignment gives a valid 16-byte material record or a sane image header |
| a 4/12/16/32-byte-stride table index | the values are not congruent mod anything above 2 |

Also tried and wrong: rendering the RLE data through `MAPn.COL`, and with a
palette read from just after the RLE stream. Both give horizontal streaks, i.e.
per-row constants — a ramp, not a picture. **Those RLE streams are probably not
the visible textures.**

#### FACE → MATERIAL → COLOUR: SOLVED, read out of the renderer

**The material field is the int16 at `face + 0x04`**, and it is an offset into
`MAPn.MLB`'s 16-byte material table. From `FUN_00083468`:

```asm
MOVSX EAX, word ptr [EBP + 0x4]      ; int16 at face+4
ADD EAX, dword ptr [0x00021540]      ; += the MATERIALS blob base
MOV EAX, dword ptr [EAX]
CALL dword ptr [EAX + 0x722e8]       ; dispatch on what it finds
```

Verified against the data: **3000 of 3009 faces in track 1 land on a valid material
record** (size a power of two, texture record in range). Every value is a multiple
of 16, which is exactly the table stride.

> The field at `face + 0x20` is **not** the material — it is constant per object
> but so is every other candidate, and it is what the first draft of this section
> wrongly picked. It survives the "is it a plane constant?" test (object 30 has 51
> distinct normals across 53 faces and still one value at +0x20) but it is not the
> thing the renderer indexes the materials with. `+0x04` is.

#### The face record, with offsets read from `FUN_0001bfc4`

```asm
if (*(short *)(face + 6) == 4)  quad;  else  triangle;
list = *(int *)(blk + 0x28) + blk;          // face list
b    = *(int *)(blk + 0x14);                // face count
face = *(uint16 *)(list + k*2) + blk;       // uint16 offsets from the BLOCK
```

```
+0x00  int32    0
+0x02  int16    ?
+0x04  int16    MATERIAL -- an offset into MAPn.MLB's 16-byte table
+0x06  int16    n        corner count (3 or 4)
+0x08  (n+1) uint16      corner vertex BYTE offsets, /6 for the index
+0x14  int32    Nx   } the face normal, LOCAL space, 14-bit fixed (16384 = 1.0)
+0x18  int32    Ny   |  FUN_0001bfc4 solves the plane with them:
+0x1c  int32    Nz   }  y = -((x-ox)*Nx + (z-oz)*Nz) / Ny + oy
+0x20  int16    ?         constant per object
```

**`mesh_load_at()` reads the face base two bytes late** — `find_faces()` scans for a
run of records and lands at true+2 — so its `n` is at `r+4` (= true+6) and the
material is at **`r+2`** (= true+4). Everything is self-consistent; just do not
"fix" one offset without the other.

#### The material record, and the palette is 6-bit

```
MLB + offset:   +0x00  int32   texture size (a power of two: 16 or 64)
                +0x04  int32   texture record offset -> [1, N, data_offset]
                +0x08  int32   colourA   } palette indices
                +0x0C  int32   colourB   }
```

`colourA` is the **lit** colour and `colourB` the **shaded** one: 303 of track 1's
342 records have `A != B`, and the pairs are exactly what they should be —
`(200,100)` is `(0,63,0)` green against a dark blue, `(78,70)` is grey-pink
against dark red, `(254,32)` grey against black.

**`MAPn.COL` holds 6-BIT VGA values (0..63), so scale by 4.** Without it everything
renders at a quarter brightness and the palette's "white" reads `(61,61,61)`.

**The port now does the whole chain**: face → material offset → MLB record →
`colourA`/`colourB` → `MAPn.COL`, interpolating the shaded colour towards the lit
one by the light level. Track 3 shows green verges and dark red brick, which is
the game's own data, not a guess.

#### The texture pixels — SOLVED. They are RAW, and `size` is the HEIGHT

```
material.texrec -> [1, N, data_offset]
data_offset     -> 4096 RAW 8-bit indices, indexed by MAPn.COL
dimensions      -> (4096 / size) wide  x  size tall
```

**Every texture is 4096 bytes. `size` is its HEIGHT, not its dimension.** So
`size = 16` is **256 x 16**, not a 16x16 thumbnail, and `size = 64` is 64 x 64.

**Reading a size-16 texture as 16x16 was the bug that made every texture look
wrong** — it took the first 256 bytes of a 256x16 strip and stretched them over
the surface, which produces smeared horizontal banding and nothing recognisable.
The moment they are read 256x16 the content is unmistakable: brick courses, road
markings, timber planking, a building facade with windows, a blue "B" on white.

**What identified it was the packing, not the pixels.** Sorted texture data
offsets are repeatedly `4096 + 768` and `2*4096 + 768` apart — always whole
4096-byte textures, never 256. That ruled out `size * size` before any guessing
about shapes. When a decoder "works" on every input, count the container sizes.

**No compression and no per-texture palette** — an earlier attempt applied the
PCX run rule that had just been read out of the backdrop loader. It RLE-decoded
to exactly `size*size` values every single time, which felt like confirmation and
was not: *asking a raw buffer for exactly N values always succeeds.*

**This cost a long detour and the reason is worth keeping.** The PCX run rule
(`(b & 0xC0) == 0xC0`) had just been read out of `FUN_0004ce50`, and applying it
produced output that RLE-decoded to exactly `size*size` values every single time
— which felt like confirmation. It was not: *asking a raw buffer for exactly
`size*size` values always succeeds.* The decode was consuming raw texture bytes
as run-length codes and rendering the result, which gave plausible-looking grey
banding that is pure artefact.

**What settled it was the spacing, not the decoder.** Sorted texture data offsets
are repeatedly `size*size + 768` apart, and `2*size*size + 768` — i.e. a whole
number of uncompressed textures per gap. Rendering them raw through `MAPn.COL`
gives the whole track: brick courses, road markings, blue-striped barriers,
grass. **When a decompressor "works" on every input, check the container sizes
instead.**

`N` in the texture record is still not understood — it is not a byte count (0 of
203 agree) and it is frequently 0 even for materials that have a texture.

#### Rendering textures

`tri_tex()` samples with the barycentric weights the coverage test already
computes — affine, not perspective-correct, which is what a 1995 software
renderer did anyway.

**MIND THE WEIGHT ORDER — this one is invisible until you texture something.**
In `tri()`, `w0 = E01(p)/area` is the barycentric weight of **vertex 2**, and
`w1` is that of **vertex 1**. The coverage test does not care, and the flat path
cannot show the difference because all three corners are one colour — so writing
`w2*u0 + w0*u1 + w1*u2` shipped happily and swapped u1/u2 across every triangle,
tearing the texture into what Danik correctly called "scrambled". The correct form
is `w2*u0 + w1*u1 + w0*u2`, where `w2 = 1 - w0 - w1` is vertex 0's weight.

**No amount of texture-scale tuning fixes a permutation error**, which is what
made it look like a scaling problem for three rounds. The texture is **modulated by light level as greyscale**;
multiplying it by the material's `colourA`/`colourB` double-darkens, because that
pair is already near-black for many materials.

**The face record carries no texture coordinates**, so UVs are derived from the
face's own local extent: the two widest local axes, divided by `TEXSCALE`.
**That scale is a choice, not decoded data.** It is 4106 units — the length of a
road tile, so one texture repeat per piece — which reads far better than the
1024 that was picked by eye first.

#### The material field is a genuine surface selector

Objects 2 and 6 in track 1's UNR are **the same shape** — same `voff` (1548),
same 16 faces, same 3960-byte size — and carry **different** material values
(4264 against 1046). So the field really does select the surface, which is why
two identical geometries are separate entries in the list at all. It is constant
across every face of one object.

It appears **only** in face records: searching `MAPn.UNR` and `MAPn.MLB` for the
values as int16 finds nothing but the face fields themselves — there is no table
of them anywhere. So it is not an offset into either file.

**The next concrete step is to find the code that READS the face's `+0x1e`
value.** A scan of the whole code object for a word-sized read at displacement
`0x1e` finds only five functions and none of them is a renderer — so the field is
consumed at load or through a different displacement, and that is where to look
next. `FUN_00083468` is the polygon renderer that uses `DAT_00021540` (the
MATERIALS blob), but its decompilation is mangled by pointer arithmetic that
Ghidra turns into negative structure offsets; recovering it needs the assembly.

`MAPn.MLB` layout, which the material table sits in:

```
+0x0000   355 x 16-byte records   [size, record_offset, colourA, colourB]
                                  size is a power of two; four clean runs of
                                  pow-2 sizes over entries 0..354, ending at
                                  5680 == 355*16 exactly
+0x16C0   355 x 12-byte records   [1, N, data_offset]
data_offset -> RLE that decodes to exactly size*size pixels
```

#### MAPn.COL is eight ramps of 32, and that is what it is for

Entries 0-31 black to grey, 32-63 grey, 64-95 red, 96-127 blue, 128-159 deep
blue, 160-191 brown, 192-223 orange, 224-255 grey. That is a **shade-table
palette**: a surface picks a ramp, the light level picks a step within it.

**The port now shades that way** — `render_frame()` reads MAPn.COL, takes the
ramp from the face's material field and the step from the stored normal, and
lights with ambient 0.50 (the ramp bottoms out at black, so a wall facing across
the light would otherwise shade to nothing). **The material-to-ramp mapping is a
placeholder guess** — `(material / 2) & 7` — but it is *stable*, so the same
object type always gets the same ramp, and every colour comes from the real
palette. Replace the guess, do not remove the mechanism, once the field resolves.

### `.3DX` LOCAL AXES ARE NOT WORLD AXES  (this cost a long time)

In `.3DX` **local** space: **X = width, Y = HEIGHT, Z = depth**. In the *track*'s world
space Z is elevation. They are different conventions and mixing them silently lays every
object on its back.

The measurements that settle it:

| mesh | X | Y | Z | reads as |
|---|---|---|---|---|
| `AAUTO00` (car) | 753 | 450 | 1476 | 4.5 m long x 2.3 wide x 1.4 tall |
| `AWBUS` | 1038 | 1009 | 4103 | 12 m long x 3.1 wide x 3.0 tall |
| `AZGIRL` | 502 | 1340 | 0 | 3.9 m tall, **flat in Z** |

So place as `world = (x + lx*c - lz*s, y + lx*s + lz*c, z + ly)`.

**Sprites are vertical billboards** — flat in local Z (several are exactly 0 thick). To
face the camera, rotate about the vertical by `atan2(dy, dx) - 90 degrees`: the sprite's
normal is its local Z, which after a rotation `t` points along `(-sin t, cos t)`, so
aiming it at the camera (direction `phi`) needs `t = phi - 90`. Rotating by `phi` alone
leaves every sprite edge-on and effectively invisible.

### Camera height

`cam_height = road_width * factor`, factor given as the 6th argument (default 0.30).
`1.15` is a drone shot; **0.12-0.16 is eye level** -- given the car is 4.5 m for 1476
units, 1 unit is about 3 mm, so the road is ~8 m wide and an eye is ~1.3 m up.

### The obstacles are SPRITES

Danik's observation, and it fits the data: the objects are **flat billboards** -- 2 to 13
faces, all coplanar (in the UNR sample every vertex has x = 0). That is what a sprite is
in this engine; it is not a lit 3D mesh.

They render as grey panels here because **they have no texture**. The flat shading makes a
coplanar quad read as a box, which is why they look like "buildings scaled too small".
Getting them to look like the original needs the material/texture data -- `MAPn.MLB`
(allocated as `MATERIALS`) or the `.PKC`/`.SPR` images in the track's GFX container, none
of which are decoded yet. **Texture the sprites before judging the placement visually.**

**Do not trust a loose numeric scan, and do not trust a plausible-looking median.**
Four attempts produced false positives here: a range filter wide enough to pass ordinary
integers "found" 1,213 track coordinates in the MMP; an offset-table dump matched as
coordinates; the MRK proximity test passed candidates that were half on the road; and
reading one record per cell was taken for the whole table. Each time the tell was a hit
rate too high to be real, or a statistic computed over a subset that silently excluded the
records that would have contradicted it — the "median 2.9 half-widths, mostly roadside"
claim was measured over a set that could not contain the bad cases.

Two checks are cheap and would have caught every one of them:

* `tools/mmp_dump.py` — prints the per-layer lateral statistics **and** the grid
  self-test, and exits non-zero if the self-test is not 100%.
* `--layer 2` on the port — renders the frame with no scenery at all, so a visual
  artefact can be attributed to the scenery or to the road instead of guessed at.

**Still crude and known so:** flat-shaded only, no textures, and the ground is a plain
skirt rather than real terrain.

### Two bugs worth never repeating

**The road twisted like a DNA helix.** Cause: `road_build()` interpolated the half-width
*vector* between control points with Catmull-Rom, so its direction could rotate and its
magnitude pass through zero, flipping the ribbon. Fix, and the rule to keep: **only the
scalar width is interpolated; the extrusion direction is derived from the centreline
tangent at every sample** (`side = perp(normalise(next - prev))`). The ribbon then cannot
twist regardless of how the control points are spaced.

**The backdrop was chunky and stretched.** Two causes: (a) the panorama was mapped over a
full 360 degrees, so a 70-degree view magnified it about 3x; (b) the window was 1.6:1 when
320x200 was always shown at 4:3. Fix: map the image so **one source pixel subtends the
same angle as one screen pixel** — `ang_h = image_width / focal_x`, `ang_v = image_height
/ focal_y` — and render at 4:3 with `focal_x = focal_y * ASPECT` where
`ASPECT = 1.2 = (320/200) / (4/3)`. The same corrected focal must be used for the sky and
for the projection, or the two disagree.

The `ASPECT` correction is why a circle in world space looks round in a 960x720 window
even though the framebuffer is 320x200. Changing the framebuffer size means revisiting it.

**Known remaining artefact:** where the road passes directly under the camera, quads with
a corner behind the near plane are dropped whole, leaving a notch at the bottom of the
frame. Correct fix is to clip the polygon against the near plane rather than discard it.

### What makes it look like Screamer

Three things, in order of impact, all now in:

1. **The `MAPn.BCK` backdrop.** It is a PCX panorama. A conversion step turns it into
   `build/sky/MAPn.sky` — a `SKY1` magic, `uint16 w`, `uint16 h`, then raw RGB — which C
   can read without a PCX decoder. `sky_paint()` renders it as a cylindrical backdrop:
   each screen column maps to a yaw, each row to a pitch, and the image is stretched over
   `HORIZON_UP` radians above the horizon. Regenerate with:

   ```python
   from PIL import Image; import struct
   im = Image.open(f"build/dec/MAP{i}L/MAP{i}.BCK").convert("RGB")   # note the L!
   ```
   The backdrop lives in the **`GFX/MAPnL.DAT`** container, not the `DATA/MAPn.DAT` one
   that holds the splines — they are in different files and the path is easy to get wrong.

2. **A ground skirt.** `SKIRT` extends the road mesh outwards 20x the half-width in dark
   grey either side, so the road stops reading as a floating ribbon.

3. **Distance fog.** `emit()` fades every quad toward `COL_FOG` with
   `1 - exp(-depth / fog_far)`, where `fog_far` is 55x the road width. This is what makes
   the ground melt into the backdrop at the horizon rather than ending in a hard line,
   and it is most of why the result reads as a 1990s racer.

Known-good renders: `build/png/port_frames.png` (track 1, two corners) and
`build/png/port_tracks.png` (tracks 5 and 3 — each has its own backdrop; 5 is a
Mediterranean village, 3 a hazy coastline).

**Road edge lines** are drawn as thin quads at ±0.86..0.98 of the half-width. Cheap and
they do a lot of work for the sense of speed.

4. **The lane markings** are the game's own overlay strips, and they are now drawn.
   `V44`, `V117` and `V118` are `4136 x 258 x 0` sheets placed ON the road (object 27 at
   −0.59, −1.45 and −1.61 half-widths), as are `V26`/`V27`. Their materials are the
   colour-keyed ones (25 → 59% index 0, 20 → 34%), so the sampler skips index 0 and the
   road shows through — which is what makes them read as paint rather than as grey slabs.
   Two things had to be true for them to appear, and both are in the code now: index 0 is
   transparent (`tri_tex`), and the on-road filter must not delete overlays.

**Renderer decision (recommended, not yet confirmed by Danik):** reimplement the game's
own software renderer rather than writing a GL one. Screamer's dark/neon look *is* its
renderer, and a low-res software framebuffer is the natural fit for the MiSTer hybrid
core, where the FPGA does the scan-out.

### What is still unknown

`MAPn.MMP` and `MAPn.UNR` are **decoded** (see Scenery, above) — the placement grid and
the 192 track meshes. What is left in them is the *record semantics*: `E` (+0x0C) and
`T` (+0x18) are read but not understood, and `E` is what should orient each piece.

Also open: `MAPn.MLB` (2.6 M, allocated as `MATERIALS` — the textures, and what the pieces
need to stop being grey), `MAPn.SMG` (16 K of sequential bytes — a lookup table),
`MAPn.COL` (a palette) and `MAPn.BCK` (a PCX backdrop), both understood. And the
`.PKC` / `.SPR` / `.FNT` image formats.

**Textures are the next real milestone.** Every piece currently renders flat-shaded grey;
the shapes and positions are right but the surface is not, and no amount of further work
on placement will make it read as Screamer until the materials land.

---

## Legal

The game is **still sold on GOG and Steam** — it is not abandonware. Follow the Dethrace
pattern: publish the reimplementation, never ship game assets or the original binaries.
Keep `STARTL.EXE` and all `DATA/`, `GFX/`, `CONFIG/`, `TABS/`, `REPLAYS/` out of any
public repository.


### Reporting positions back (Danik -> Claude)

The window prints the current road sample and prints it in the title bar too:

```
OpenScreamer  t=248 of 336   lap 73.8%
```

`t` is exactly the argument `--shot <file> <t>` takes, so "the tunnel is missing about
where it says t=248" is directly reproducible with one command. Do not ask for a
description of the screen when a number will do.


### Flying and looking from above (added for debugging)

Danik: *"Give me the ability to fly and look from above to see what's going on."*
The window has free flight and an overhead view now:

```
W A S D       fly horizontally          R / F   up and down (SHIFT = 4x)
mouse drag    look around
v             hover above the road looking down; R / F then change the height
h             back on the road, chasing it
m             toggle auto-drive / free flight
```

The same view is available headlessly, which is what makes a report reproducible:

```
OS_OVERHEAD=8000 port/build/openscreamer build/dec/MAP1 1 --shot /tmp/above.ppm 46
```

**A structure in the wrong place is obvious from above and invisible from the driving
seat** — use the overhead view before theorising about placement.


---

## TEXTURES: the 2026-10-07 session's findings

### Every texture is a 256 x 16 strip. Settled by LOOKING at the content.

`mlb_dump.py` renders any material; at 256x16 the content is unmistakable and at every
other size it is shredded:

| material | at 256 x 16 |
|---|---|
| 21 | a row of **four car sprites**, 64x16 each |
| 96 | a crowd stand: trees, a structure |
| 20 | a **guardrail** -- a rail with posts on a transparent background |
| 0 | flat asphalt |

So the earlier "`size` is the HEIGHT" reading is wrong in its reasoning (see below); the
256-wide conclusion is right and is now confirmed from the pixels rather than from a
statistic.

### `size` in the material record is NOT a dimension

It takes 16, 64, **128 and 224** as well, and 4096 bytes does not divide by 224. It is
also used as an index in the renderer's dispatch (`FUN_0008411d`):

```c
(**(code **)((key & 0xf) + *(int *)((key & 0xfffffff0) + DAT_00021540) + 0x722e0))(*puVar2);
```

i.e. the material record's first dword is a base added to 0x722e0 and the low nibble of
the key picks an entry -- a table of routines, built at runtime (that region is zero in
the shipped EXE). **Do not use it as a texture scale.** The world scale is still an open
question; `8192` units per U repeat is the best evidence there is, because Danik arrived
at it by eye across the whole track, and it is what the port uses.

`OS_MATSCALE=1` tries "size x 512 units per repeat" and is OFF: it is a hypothesis, kept
only because it is cheap to A/B.

### THE LANE MARKINGS ARE IN THE ROAD TILES, NOT IN THE ROAD TEXTURE

Two measurements, both decisive:

1. **The road surface texture (material 0) is flat grey.** All 16 rows have the same mean
   (232), the extremes are 228..244, and there is not one pixel below 60. There are no
   lane lines in it, and nothing that could produce Danik's four bright/dark cycles.
2. **The road TILES carry the markings as their own faces.** `V0` has 32 faces of
   material 0 (the asphalt) and 20 more of materials **147/148**, which are **flat and
   coplanar** with the asphalt -- X -3079..-2951, Y = 0 -- i.e. a 128-unit (38 cm) strip
   of paint along the outer edge of the road. The same material on `V23` sits at
   Y 64..128, a raised kerb.

So the paint is part of the tile, and skipping whole tiles threw it away. `OS_TILEPAINT=1`
(default) draws a tile's non-surface faces--everything whose material is not the road
material and not the ground material--lifted 40 units so the painter's sort cannot put
the synthesised road on top of the paint. `OS_TILELIFT` adjusts the lift, `OS_TILEPAINT=0`
disables.

### The thin ribbons are RAILINGS, not paint

`V44`, `V117`, `V118` (`4136 x 258 x 0`) carry materials 20 and 25, and those textures
are a guardrail and a picket fence. They are barriers standing along the road, and the
"dashed centre line" seen in earlier renders was one of them LYING FLAT (the `roll`
transform). `OS_ROLL` is now **0** by default -- they stand as authored. `OS_ROLL=1` lays
them down again for an A/B.


### WHERE THE LANE MARKINGS ACTUALLY ARE (found 2026-10-07)

They are **faces of the road tiles**, and every tile that carries them is in **layer 0**
-- which the default view excludes. That is the whole reason they were never on screen.

```
material 147:  12 faces per road tile    4 fully left, 4 fully right
               V0: X -3079..-2951 and +2951..+3079, Y = 0   <- 38 cm edge stripes
               V23: the same, at Y 64..128                 <- a raised kerb
material 148:   8 faces per road tile    4 left, 4 right, at X = +-1604
               <- a lane line down the middle of each carriageway
```

The road SURFACE texture (material 0) is flat grey -- measured: every row the same mean,
range 228..244, not one pixel below 60. There is no paint in it.

Every mesh using 147/148 is a road tile, and they are **all layer 0**: V0 x40, V23 x14,
V71 x18, V5, V9, V10, V28-32, V38-40, V49/50, V55-59, V75-77. The port's default `layer 1`
was therefore filtering the road's own paint out before anything could draw it. The layer
filter now lets a piece of road through either way; scenery still obeys it.

**Status: geometry and placement are right, textures are not.** With `OS_TILEPAINT=1` the
strips appear exactly where the tile puts them -- along the road and across it -- but the
textures come out as colour SPLOTCHES. So materials 147/148 point at **tile sheets**
(possibly 16x16 cells, which would also explain why the material record's first field is
16 or 64 and why the renderer dispatches on it) rather than at the 256x16 strips every
other texture in the file is. **Decoding that sheet, and how a face selects a cell, is the
next step** -- and it is the same question as the `N` field in the texture record, which
is still not understood.

The switch is off by default so the road is not covered in splotches meanwhile; the
positions can be inspected with `OS_TILEPAINT=1`.

### Object-local UVs for thickness-free pieces (Danik, 2026-10-07)

> "The railings/roof above the crowd stands renders correctly when I press u
> (object-local)"

His finding is now automatic: a mesh with **no thickness** (local Z extent <= 64) --
a billboard, a railing panel, a decal, a crowd stand -- takes OBJECT-LOCAL texture
coordinates, and everything that must tile across many pieces keeps world coordinates.
A one-shot image wants its own origin; a surface wants a world-wide pattern. `u` still
overrides the whole thing.


### Three fixes from Danik's 2026-10-07 report

1. **"I still have to press u to make the crowd-stand roof look ok."** The `thin` test
   only looked at the local **Z** extent, so the roof and railing panels -- which are
   `0 x 516 x 4136`, flat in **X** -- missed the object-local UV rule. `thin` is now
   "flat in EITHER horizontal direction".

2. **"The crowd texture itself needs 90d CW rotation."** Implemented as a property of
   WHICH WAY a piece faces, not of thinness: a piece flat in Z faces across the road
   (a billboard) and takes its texture crossed over 90 degrees CW; the railing panels,
   which are flat in X and look right as they are, are untouched. `OS_BILLROT=0` gives
   the straight mapping for an A/B.

3. **"At long/sharp turns the outer road marking still clips through the road surface."**
   The road's own edge lines were separated from the road by a 20-unit lift and a fixed
   depth bias, and a bias cannot survive a long quad -- the difference in average depth
   between a long road quad and a thin strip beside it grows past any sane constant.
   The sort now takes a **sequence** as a tiebreak inside a 400-unit depth window: road
   surface is sequence 0, its detail (edge lines, tile paint) is sequence 1 and is
   therefore drawn after it, at every distance. `cmp_depth` in `main.c` is the place to
   look; the `seq` field is on the quad.


### `i` -- WHAT AM I LOOKING AT? (the tool that ends the guessing)

```
i    describe the piece under the crosshair, to the console:
     mesh number, layer, position, height, and the flags
     (thin / billboard(Z) / roll / tile / roadpiece / flat),
     then each face's material and the texture size that record gives.
```

Headless equivalent, for reproducing a report exactly:

```bash
OS_INSPECT=1 port/build/openscreamer build/dec/MAP1 1 --shot /tmp/x.ppm 200
```

**Use this before theorising about a piece.** Two rounds were lost to guessing which
mesh "the roof above the crowd stands" meant; the geometry around a crowd stand (V153)
turned out to contain only V34/35/36, V168, V188, V191 and V44 -- nothing that is
obviously a roof, so the roof is either part of the stand's own two quads or something
further away. Ask for the number.

### Road edge lines: three strips, never an overlay

The edge lines were separate quads coplanar with the road surface, which put two
surfaces at the same depth and made the sort choose -- on long turns the line dipped
through the tarmac.  A z-lift, then a depth bias, then a depth-window sequence tiebreak
were all attempts to win that argument.  The road is now THREE NON-OVERLAPPING QUADS:
surface -0.86..+0.86 of the half-width, edge strips -0.98..-0.86 and +0.86..+0.98.
Nothing is coplanar with anything, so there is no argument left to have.


### The texture dial keys (Danik's workflow, 2026-10-07)

```
i     describe the piece under the crosshair (now: the piece IN FRONT,
      skipping the ground and the road -- the first version picked whatever was
      nearest the screen centre, so reports came back naming the wrong mesh)
n     rotate that piece's material 90 degrees
z x   scale that piece's material down / up; prints the world units per repeat
b     fit the texture ONCE across billboards and panels (OS_FACEFIT=1)
```

`n`, `z` and `x` write per-material tables (`g_matrot`, `g_matscale_mul`) so one
material can be fixed without disturbing the others, and every press prints the
material number and the value, which is what gets written down here.

**Danik's reports, which are the ground truth for all of this:**

* edge markings on the road: **fixed and consistent** (the three-strip road above)
* the crowd, `V164`, six faces, all material 78 (size field 16): *"the crowd can't
  look right only by using n. it needs scaling as well"*
* the roof, `V22`, two faces, materials 164 and 15, thin and billboard(Z): *"same
  for roof/stand"*

So the mapping needs a SCALE that is not the global one -- the candidate being
"one texture repeat across a thin piece's face", which is what `b` switches on.
**Unresolved: the rule that decides the scale.** The per-material dial is how the
right values get found; when enough of them are known, look for what they have in
common rather than hard-coding a table.
