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

**`screamer/` holds the game's files the project uses** (since 2026-10-11; the retail
game was copied in on 2026-10-07 and everything else has been DELETED):

```
screamer/STARTL.EXE  STARTH.EXE         what is decompiled (STARTH: the hi-res program, unread)
screamer/DATA/   GEN LOCAL MAP1-6 (+I variants) MUSIC SOUND .DAT
screamer/GFX/    GAME GRAF MENU MAP1-6{L,H} .DAT
screamer/CONFIG/ CONTROLS.CFG DEFCTRL.CFG       the game's control set-up, for the controls work
```

(`screamer/HMIDRV.386`, `HMIDET.386`, `HMIMDRV.386` are BACK since 2026-10-11, restored by
Danik so the mixer could be read -- section 30; the port does not use them.)
Gone, and not needed by anything here: DOS4GW.EXE, SETUP.EXE
and its SETUP.DAT / SETUP.CFG, SCREAMER.BAT / .CFG / .PIF, the icons, TABS/ (PERSP01.TAB,
VIS.TAB: the port never read them), REPLAYS/ (only DUMMY.DAT shipped).  In the older
sections below, a path like `DATA/…` or `STARTL.EXE` now means `screamer/DATA/…`,
`screamer/STARTL.EXE`.

**The port reads the containers directly** (port.c `packed_open`, since 2026-10-11): its
code still asks for `build/dec/<CONTAINER>/<FILE>`, and when the data directory is a copy
of the game (it has DATA and GFX folders, any letter case) `os_fopen` answers that out of
`DATA/<CONTAINER>.DAT` or `GFX/<CONTAINER>.DAT` -- directory lookup, the game's LZW in C,
a read-only stream over the bytes (`fopencookie`).  Up to six containers are kept in
memory (in the block that outlives the parts of the program).  Data directory, first that
exists: `./screamer`, `<program>/screamer`, `<program>/../../screamer`, then the unpacked
kinds (`./build/dec`, `<program>/data`, …) which still work.  So from the project
directory the port now runs off `screamer/`.
Checked against the unpacked files: every one of the 112 files a race reads decodes the
same; 37 pictures (six tracks, both resolutions, a reversed track), a race's last picture
and its whole sound, and three menu screens are byte-identical; all twelve robot laps the
same; a lower-case game folder works.  Loading a track: 0.11 s against 0.09 s.
**Trap:** `fmemopen(NULL, n, "w+")` was tried first for the stream and gave slightly
different pictures though every byte was right -- do not go back to it.
`OS_PACKDUMP=<dir>` writes out everything read from the containers.

**The start-up check** (port.c `game_check`, once a run, before anything else): prints
`Checking game data in <dir> ... OK (31 containers, 712 files)`.  Needed: DATA's sixteen
containers and GFX's nine low-resolution ones; the six `MAPnH` are optional (without them:
"low resolution only").  Each container must be there, have a sound list of contents, and
every file in it must UNPACK to its listed size (about 0.08 s for the lot).  Anything
missing or damaged is listed by name and the program stops with exit code 1.

`build/dec/` stays: the Python tools and the notes in this file work on unpacked files.
`tools/extract.sh [game dir] [out dir]` (default `screamer` → `build/dec`) makes it;
its output is file-for-file identical to the `build/dec` in use.

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
python3 tools/unle.py screamer/STARTL.EXE -o build/screamer_flat.bin --map build/screamer_layout.json

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

**The block header also holds the texture coordinates** (`+0x24`, one `(u, v)` per
vertex) — see "How the track is built". The `.PKC`/`.SPR`/`.FNT` image formats are
solved too (sections 10 and 12).

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
  +0x14  int32  X          } the Bezier handle on the way IN to this point
  +0x18  int32  Y          }
  +0x1C  int32  0
  +0x20  int32  X          } the Bezier handle on the way OUT
  +0x24  int32  Y          }
  +0x28  ...    0

These are CUBIC BEZIER curves, proven against the road -- see "The .SPL files are cubic
BEZIER curves" in the port section.  Joining the nodes with straight lines or Catmull-Rom
cuts the corners and leaves the road.
```

**The nodes are NOT stored in path order — you must follow `NEXT`.** Verified: in
`MAP1CAM.SPL` the chain from node 0 visits all 14 nodes and returns to 0. Plotting in file
order instead yields a meaningless star of crossing lines, which cost a render iteration.

**Not all splines are simple paths — check before plotting.**

| spline | what it actually is |
|---|---|
| `SLA` | **the track's LEFT and RIGHT EDGES, interleaved.** Node 2k and 2k+1 are partners, ~2,617 units apart (range 2,202-2,919) — that separation *is* the road width. Plotting in node order produces a zigzag; split even/odd to get two parallel edge curves. |
| `CAM` | a single clean path (`next` scatters, chain closes on 0). Roughness sd/avg = 0.29. |
| `CNG` / `SLA` / `OST` | **not paths: PROP LAYOUTS**, one prop per node (section 14). The game loads ONE of them by game mode (`0x14fb4`): mode `0x20` → `CNG`, mode `0x40` → `SLA` (slalom — which is why its nodes come in pairs a road-width apart: they are gates), otherwise `OST`. |
| `OPP` | ten splines: nine opponents + the player (section 11) |
| `ANM` | one spline per moving-scenery object (section 14) |
| `RPL` | replay cameras |

The diagnostic that settled it: measure the ratio of the standard deviation of segment
lengths to their mean, and compare against a known-good path (`CAM`, 0.29). `SLA`
as-traversed scored 0.98; split even/odd it scored 0.36, matching a clean curve. **Use
that test before plotting any spline** — a "zigzag" in the output means the interleaving
was not accounted for, not that the data is wrong.

`tools/render_track.py` plots the loops. The road surface is the ribbon enclosed by the
two `SLA` edges — `build/png/track1_road.png` fills it.

`MAP1.MMP` is **decoded** — see "How the track is built" below; the 47 % zero / 34 % `0xFF` reading was
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
6. ~~Make it look like Screamer~~ — **DONE 2026-10-08**. All six tracks draw from the
   game's own pieces, fully textured. See "The port" below.
7. Then: cars, driving, opponents, sound, menus -> MiSTer hybrid core

---

## The port (`port/`) — DRAWS THE REAL TRACKS (rewritten 2026-10-08)

**State: all six tracks render from the game's own data, fully textured** — road with
lane lines, kerbs, ground, walls, tunnels, bridges, buildings, billboards with readable
text, crowd stands, the start grid. Reference renders: `build/png/port_track1_lap.png`
(twelve frames round track 1) and `build/png/port_tracks2-6.png`.

Nothing on screen is synthesised or tuned by eye any more. The 2026-10-07 port built the
road from splines, guessed every texture coordinate, and filtered scenery by a pile of
measured-but-invented rules; that is all gone. It is kept in `port/attic/` (with the
CLAUDE.md of that day) purely as a record. **If you find yourself adding a knob, a
per-material table or a "which pieces to skip" rule, stop: the data already says it.**

```
port/CMakeLists.txt       SDL2 via pkg-config
port/src/mesh.h, mesh.c   .3DX / UNR object: vertices, faces, normals, material, UVs
port/src/unr.h,  unr.c    MAPn.UNR: blob header + the object meshes
port/src/scene.h,scene.c  MAPn.MMP: where each piece goes, and its heading
port/src/main.c           materials, depth-buffered software rasteriser, camera, loop
port/src/drive.c, sound.c, music.c, port.c   driving model, mixer, FM music, one-program glue
port/src/fmchip.c, nuked/                    the sound card's FM chip in software (section 32)
(the first-day .SPL loader is in port/attic/spl_first_day.*; nothing used it)
```

```bash
cmake -S port -B port/build && cmake --build port/build
port/build/openscreamer build/dec/MAP1 1                      # window, track 1
port/build/openscreamer build/dec/MAP5 5 --shot out.ppm 300   # headless PPM frame
```

Arguments are positional: `datadir track [--shot file] [t] [camera-height] [layer]`.
`t` is a sample along the camera path, one sample = 400 world units (track 1 has 1072).
`camera-height` is world units above the road surface (600 = about 1.8 m, the default).
`layer` is 3 = everything (default), 0 or 1 = only that record run, 2 = nothing.

Environment, all optional: `OS_OVERHEAD=<h>` (with `--shot`: look down from h units up),
`OS_YAW=<deg>` / `OS_PITCH=<deg>` (turn the shot camera), `OS_INSPECT=1` (print the piece
at the centre of the frame), `OS_CULL=0` (draw back faces too), `OS_DRAWDIST=<units>`
(default 140000; the engine's own is 52000 and the `O` key switches between the two), `OS_FOG=<units>` (haze; off, the original has none),
`OS_NOCARS=1` (leave the parked cars out), `OS_POS=x,y,z` + `OS_HEADING=<deg>` (put the
shot camera at an exact spot — the `pos` the `I` key prints — instead of on the lap).

**Settled, 2026-10-08 — the striped wall `V81` (material 21) is right as drawn.** It is
one strip of slanted hazard stripes (a 32x254 texture holding a single diagonal band), not
an arrow with a missing half. Danik raised it, then confirmed it was fine. Nothing to
mirror; do not go looking for it.

**Controls (2026-10-08).** Three modes, `TAB` cycles them, `1`/`2`/`3` pick one; the
window title always shows the mode and `t=NNN` (= the `--shot` argument).

| mode | what it is | keys |
|---|---|---|
| DRIVE | you drive — see section 8 | `UP` `DOWN` `LEFT` `RIGHT`, `A`/`Z`, `N`, `BACKSPACE` |
| RIDE | the camera drives the lap by itself | `UP`/`DOWN` faster/slower, `P` pause |
| WALK | on foot, eyes 1.7 m up, held on the ground by gravity | `W A S D`, `SHIFT` run, `CTRL` creep, `SPACE` jump |
| FLY | no gravity | `W A S D` fly where you look, `E`/`Q` up/down, `SHIFT`/`CTRL` |

Everywhere: click = mouse look (`ESC` releases the mouse, `ESC` again quits);
`LEFT`/`RIGHT`/`PGUP`/`PGDN` turn and look; `[` `]` jump 25 samples back/forward along
the lap, `HOME` to the start; `V` view from above (`E`/`Q` height); `I` describe the piece
in the centre of the screen (exact — the renderer keeps an owner per pixel); `F12`
screenshot (`shot_NNN.ppm`), `F11` full screen; `K` culling, `L` record run 0/1/both,
`G` wall segments, `O` drawing distance.

Scale used for all of it: **1 m = 328 units** (a car is 1476 units for 4.5 m). Walking is
6 m/s (a real 1.4 m/s walk is ten minutes a lap), run x4. Gravity: the ground is the
highest upward-facing polygon not above knee height (`surface_pick(..., below=1)`), so
kerbs are climbed and bridges overhead ignored; off the edge of the modelled world you
keep your height instead of falling for ever. **There is no wall collision yet** — you
can walk through walls and cars. That belongs with the driving physics.

`SDL_VIDEODRIVER=dummy OS_DEMO=400 port/build/openscreamer build/dec/MAP1 1` is the
headless self-test of WALK: it walks forward 400 frames, jumps once, and prints feet /
ground / eye height. Use it after touching the movement code, since I cannot press keys.

**I verify with `--shot` and the `Read` tool, every change.** A frame sheet takes
seconds; do not hand Danik a build that has not been looked at.

Design choices: 320x200 software framebuffer (the original's look, and what the MiSTer
hybrid core wants); SDL2 not SDL3 (MiSTer toolchain); **a depth buffer, not a painter's
sort** — the track is big tiles with paint, kerbs and walls on and beside them, and every
"the marking clips through the road / switches sides in the bend" report of the old port
was a sort error. Polygons are **clipped** against the near plane, not dropped, because
the tile under the car always has corners behind the camera.

---

## HOW THE TRACK IS BUILT — all of it read out of the game

### 1. `MAPn.MMP` places the pieces (and gives their heading)

A two-level 256x256 grid of offsets into itself (`FUN_0004c904` relocates them; `-1` =
empty). A cell is a 40-byte header of five `(base, count)` pairs — `base[i] = i32(cell +
4i)`, `count[i] = i32(cell + 0x14 + 4i)` — followed by the arrays back to back. Runs 0
and 1 are one array of **64-byte records** split at `n0`, starting at `cell + 0x28`:

```
+0x00  V        object index into MAPn.UNR
+0x04  1        (a per-frame flag the engine clears)
+0x08  X        ground axis        -> port world x
+0x0C  HEIGHT   vertical           -> port world z
+0x10  Y        ground axis        -> port world y
+0x14  0  }
+0x18  T  }  (presumably three rotation angles) only T is ever non-zero.
+0x1C  0  }  T IS THE HEADING: 2048 units = one full turn.
+0x20  -1
```

A mesh corner `(lx, ly, lz)` (local Y is UP) lands at

```
world = ( X + lx*cos(a) - lz*sin(a),   Y + lx*sin(a) + lz*cos(a),   HEIGHT + ly )
a = T * 2*pi / 2048
```

**Proofs.** (a) The grid self-test: every record's X/Y select the cell it is stored in via
`(coord + 0x100000) >> 13`, 809 of 809 — `tools/mmp_dump.py`. (b) The heading: drawn from
above with the rule above, track 1's flat tiles abut edge to edge into one continuous
road right round the lap, bends included (bend pieces step T by 57 = 10 degrees). With
the sign flipped the bends shatter; 1024 or 4096 per turn line nothing up. And billboard
text ("Virgin", "5up", "John's", "START") reads the right way round, so nothing is
mirrored.

*The 2026-10-07 notes said T "is not a heading in any convention tried". It was tested
against the direction of the road, but a piece is turned to its own grid, not to the
road — and several meshes are long in X while others are long in Z, so the comparison
was 90 degrees out for half of them.*

Run 0 vs run 1: run 0 is mostly road pieces, run 1 mostly everything else. The port
draws both and needs no rule about either.

**The track IS the pieces.** Road surface, lane lines, kerbs, grass, walls, tunnel roof,
bridges: all are UNR meshes placed by MMP records. The slopes are in the meshes
themselves. There is no separate road to build and nothing to filter out.

### 2. `MAPn.UNR` holds the meshes

```
+0x00  int32            blob_size
+0x04  int32            object_count        (192 on track 1, up to 360)
+0x08  count x int32    offsets, relative to the blob
blob                    starts at 8 + 4*count
```

Each object is a `.3DX`-shaped file in miniature (block at `0x330`). **The block header,
offsets relative to the object's own start:**

```
+0x10  a      vertex count
+0x14  b      face count
+0x18  flags  (bit 0x2000 = this block is REFLECTIVE: 0x834fc calls 0xc1fc0, the
              reflection-coordinate pass of section 15 -- see there)
+0x1C  offset of the VERTEX array      a x (int16 x, y, z)
+0x20  offset of a second per-vertex array, or -1
+0x24  offset of the TEXTURE-COORDINATE array   a x (int16 u, v, 0)   <- the key
+0x28  offset of the face list (uint16 offsets from the block)
```

(`0x8a665..0x8a698` in the flat image stores these three array pointers to
`0x741bc / 0x741c0 / 0x741c4`.)

**Face record, 36 bytes** (true base; `mesh_load_at()`'s `r` is this + 2, self-consistently):

```
+0x04  int16   MATERIAL: byte offset of a 16-byte record in MAPn.MLB
+0x06  int16   n, corner count (3 or 4)
+0x08  (n+1) x uint16  corner vertex BYTE offsets (/6 = index), loop closed
+0x14  int32 Nx, +0x18 Ny, +0x1C Nz   face normal, 16384 = 1.0
```

The normal points AWAY from the visible side (a road tile's reads straight down): the
engine draws a face when `(corner - eye) . normal >= 0` (`FUN_00083468`), and so does
the port.

### 3. TEXTURE COORDINATES ARE PER VERTEX

This was the wall. The old notes said "the face record carries no texture coordinates",
which is true and beside the point — they sit in the array at block `+0x24`, parallel to
the vertex array and indexed by the same byte offset. The textured-polygon set-up at
`0xd5350`:

```asm
mov   edx, [0x741c4]            ; texture-coordinate array
mov   bx,  [esi+8]              ; face corner -> vertex byte offset
movsx eax, word [ebx+edx]       ; raw u (0..255)
imul  eax, [ecx+8]              ; x material U extent
sar   eax, 5
movsx eax, word [ebx+edx+2]     ; raw v
imul  eax, [ecx+0xc]            ; x material V extent
sar   eax, 5
```

### 4. `MAPn.MLB` — materials and pixels

**Material record, 16 bytes:**

```
+0x00  ROUTINE SELECTOR, not a size.  The renderer calls
       [record[0] + 0x722e0 + n]  -- a table of polygon fillers at 0x822e0 in the flat
       image (the address is object-relative, +0x10000; reading it un-relocated is why
       it once "looked zero").
          16  = solid texture
          64  = texture with palette index 0 transparent (`test al,al / jz`)
          224 = see-through: the texel is a STRENGTH 0..63 and the pixel becomes
                `SMG[strength*256 + pixel]` -- see "MAPn.SMG" just below
          128 = NO TEXTURE, ONE FLAT COLOUR (0xd8844): the palette index is in the
                FACE record at +0x02.  Not a separator -- the cars' tyres and
                undersides are made of it, and the port drew none of it until
                Danik saw wheels with no width (build/png/port_tyres.png)
          160 = one material on track 1, not yet looked at (drawn as solid)
+0x04  offset of the texture record
+0x08  U EXTENT  } size in texels of the material's rectangle; a vertex's raw u, v are
+0x0C  V EXTENT  } 0..255 fractions of it:  texel = raw * extent / 256
```

**Texture record:** `+0x00` number of FRAMES, `+0x04` looks like an average colour,
`+0x08` data offset of frame 0, the further frames' offsets after it. They are animation
frames and paint jobs, NOT detail levels — see section 15.

**Pixels:** raw 8-bit indices into `MAPn.COL` (6-bit VGA, x4), in a page that is **always
256 bytes per row** whatever the rectangle's width: `pixel = data[v*256 + u]`
(`mov bh,dh / mov bl,.. / mov al,[ebx+data]`). So the road surface (extent 254x254) is a
whole 256x256 picture, a guardrail is 254x32, the crowd 120x45, a lane line 12x200, the
"Graffiti" sign 122x44. `tools/mlb_dump.py <track>` draws them all.

**Dead readings — do not resurrect:** "every texture is a 256x16 strip" (it saw the
first 16 rows of each page); "`+0x08/+0x0C` are a lit/shaded colour pair"; "size x 512
units per repeat"; world-space or face-extent UVs; any per-material rotation or scale.
The lesson from the detour: a statistic (row correlation) and a plausible picture agreed
on the wrong answer for two sessions, and the game's own drawing routine settled it in
one read. **When a format question stalls, find the code that consumes the field.**

There is no lighting in the solid routine — it writes the texel as it comes — so the
port draws textures unshaded.

**`MAPn.SMG` is the blend table for routine 224** (solved 2026-10-08): 64 rows of 256
palette indices. Row 0 is the identity (which is why a glance at the file's first
bytes read "0, 1, 2, 3…" and it was written off as an identity table — it is not);
rows 1..~20 brighten progressively, others darken or tint (row 41 is a shadow, row 63
an orange glow). The routine (`0xcd9dd`) does `mov ah,[texture]; mov al,[screen];
mov al,fs:[eax]`. So a see-through material's texture is a map of strengths, not
colours. Uses: track 4's **headlight beams** — a fan 1600 units to each side and 4000
ahead, part of each car's nose block (`build/png/port_headlights.png`) — window glass,
shadows. The port keeps a palette index per pixel (`pxi`) so the lookup is exact; sky
pixels (an RGB picture) get the nearest palette colour on demand.

### 5. The `.SPL` files are cubic BEZIER curves

Node (56 bytes): `+0x04` point x, y, z; `+0x10` next index; `+0x14` x, y = handle IN;
`+0x20` x, y = handle OUT. The curve from node A to B = A.next is the Bezier
`(A.point, A.out, B.in, B.point)`. Proof: sampled so, **200 of 200** points of track 1's
`MAP1OPP.SPL` lie on a road tile; handles swapped 140; straight lines 157.

`OPP` is the line the computer opponents drive and is what the camera follows (resampled
every 400 units, height taken from the track surface underneath — `path_drape()`).
`SLA` = road edges (left/right interleaved), `CAM`/`RPL` = camera paths, with heights.

---

### 6. Cars — `AUTOnn.3DX` (drawn since 2026-10-08, `build/png/port_cars.png`)

Twelve blocks per car: body, nose, tail, four tyres, four hub discs, and one untextured
block. Same block format as the track pieces, UVs at `+0x24`, and the materials are
records in the track's own `MAPn.MLB` (track 1: records 207-227), so each track
container carries the cars matched to its own material table.

**Each block has its own position.** Block header `+0x0C` is the offset of a 36-byte
record `[1, 0, 0, 0, x, y, z, 0, 0]` (int32s): the block's offset within the object.
Zero for track pieces; for `AUTO00` the tyres and hubs are modelled at the origin and
carry `(+-226, 109, 490)` (front axle) and `(+-226, 94, -445)` (rear). `mesh.c` applies
it, which is all it takes to assemble a car. The y of that offset is the wheel radius,
so a car stands on y = 0. **Its nose is towards local −Z**: the big wheels (radius 109, at
z = +490) are the rear ones. An earlier note here said +Z and the car was drawn backwards. Block header `+0x00` is a
bounding radius and `+0x04` three int16 = the bounding centre.

The wheels are separate blocks precisely so the engine can turn and steer them —
that is for the driving work. Block flags seen: `0x6a00` body, `0xa00/0xa01` nose/tail,
`0x800` hub disc, `0x1` tyre.

The port parks the seven models two abreast ahead of the start line as a display
(`build/png/port_cars_from_above.png`, all six tracks). Where the game itself lines the
cars up is not decoded.

**(The port's own road-edge measurement, `road_span()` / `path_edges()`, is gone** — it
was a stand-in for walls and went through four buggy versions. The game's real walls
are in the track file; see section 9.)

### 7. The game's numbers are plain text (`.DTR`) — ready for the driving work

`build/dec/GEN/CARS.DTR`, `CARS_DATA`: per car `filename`, `animation` (`x0` or `x8000`
= the second paint job), then `maxspd maxrpm grip accel shiftmode n_shifts` and six gear
ratios. 13 cars = 7 models x 2 liveries less one (the comments are in Italian and name
them: Lamborghini, F40, Bugatti, Mitsubishi, Porsche, Corvette, "Raz car").
`COCKnn{H,L}` sections lay out each dashboard (needle pivots, gear lamp positions).

`MAPnA.DTR` (and B..E — presumably difficulty levels): `TRACK_DATA` = `color camera_hi
bonus_time carnage` (track 1: `camera_hi` **300** — the chase camera height; the port's
ride camera uses 600 for now), and `OPPONENTS` = nine rows of `max_spd accel brakes
skill sband`.

`MAPnDAT.DTR`: `OSTACLES` (the knock-over props), `ANIMATIONS` = `name speed spline
sound` — on track 1 a truck, two buses and a jet, each moving along a spline at a
speed — and `SOUND_FX`.

---

### 8. DRIVING — the player's car (ported 2026-10-08, `port/src/drive.c`)

`build/png/port_driving.png` shows it. The port starts in DRIVE mode.

**Where it is in the binary.** The race frame is `0x2002f`; the player record is a
global at **`0xf918`** (opponents: `0x10ab4`, 0x590 bytes each). The frame calls, on the
player: `0x16f24` (integrate), `0x15a70`, `0x15b9c`, `0x15aa8`, `0x15af8`, `0x15c0c`, each
a list of small routines — the full list with what each does is the header comment of
`drive.c`. `0x136c8` sets a car up from its `CARS_DATA` row (copied to record `+0x470`).
The decompiled text of all of it is kept in **`build/decomp/player_update.c`**.

**How to decompile this code (it is FPU-heavy C, unoptimised — reads well once this is
done):** `python3 tools/patch_fpu.py build/screamer_flat.bin build/screamer_flat_fpu.bin`
inlines Watcom's helper calls (`0x4d7b4` float-to-int, `0x4d826` cos, `0x4d832` sin) as
`frndint`/`fcos`/`fsin`. **`0x4d7b4` CUTS THE DECIMALS OFF, it does not round** (it sets
the FPU control word's high byte to `0x1f` first), so every `ROUND(...)` Ghidra prints
for it is a `(int)` cast in C, not `lroundf`; import that copy as its own Ghidra project (`/tmp/ghix2`,
`-noanalysis`) and use `tools/DecompAt.java <entry>...`, which creates the function at
each address. Without the patch Ghidra marks the rounding helper "does not return" and
every physics routine comes out truncated or with its arithmetic missing. Ghidra still
drops stack-passed float arguments to `0x22b60`/`0x22bcc`/`0x220fc`: read those from
`objdump -D -b binary -m i386 -M intel` at the call site.

**Player record fields** (offsets from `0xf918`): `+0x4b0/4b4/4b8` position x, height, z
(ints); `+0x4c8` pitch, `+0x4cc` heading, `+0x4d0` roll; `+0x50c` speed; `+0x7a8` rpm;
`+0x7ac` gear; `+0x144` throttle, `+0x114` brake, `+0x108` steer angle, `+0x10c` its
limit; `+0x2f4` heading change per frame, `+0x2f8` turning radius, `+0x2ec` wheelbase;
`+0x110` wheels locked, `+0x4f0/4f4/4fc` skid, `+0x128` body slip angle, `+0x170` spin;
`+0x148` crash state; `+0x174..0x190` constants derived from the car row; `+0x670`
grip of the surface underneath; `+0x194 + 0x54*n` the four wheels.

**The model, as read:**
- frame time `0x18e00`, clamped to 0.02..0.2 s; smoothing is
  `v = (v - target) * damp[i] + target` with `damp[fps][i] = pow(0.5 + i*0.005, 50/(fps+1))`
  (table built at `0x1571c`, pointer at `0xcf04`). The port steps at a fixed 50 Hz.
- **engine** (`0x18da0`): engine locked to the wheels, `rpm = ratio[gear] * speed *
  (maxrpm/maxspd * 0.02223)`; throttle adds `accel * pull[gear] * curve` rpm/s, pull =
  5, 3.5, 2, 0.95, 0.78, 0.72 and curve a half-sine peaking mid-range of the gear;
  engine braking `sqrt(rpm/max) * maxspd*0.03`; automatic box shifts up past 8500 rpm
  and down below 4000; pulling away is wheelspin — a flat 1000 units/s² until 2000
  units/s. `shiftmode` 1 = automatic, 0 = manual (so each model's two rows are an
  automatic and a manual car).
- **brake** (`0x16140`): `maxspd * 0.3` units/s²; locks the wheels above `grip * 11500`.
- **steering** (`0x1628c`): lock limit `(1 - sqrt(speed/maxspd)) * 17° + 1°`; the car
  turns about a point `wheelbase / tan(lock)` to its side.
- **skid** (`0x17d04`): cornering force `speed² * turn * 0.0001 / dt * surface`; above
  `grip * 16000` the car runs wide, above `grip * 26000` (sooner when braking) it spins.
- **wall** (`0x16c90`): speed drops by 90% times how squarely it was hit, and the nose
  is turned along the wall.
- speedometer: `km/h = speed * 340 / 21000`.

**Ground contact is the game's (2026-10-08; `player_contact()`):** `0x11a30` puts each
wheel (front-left, rear-left, front-right, rear-right) at its model position × |cos
roll| / |cos pitch|, turned by heading + body slip, and takes the height of the DRIVABLE
surface there (run-0 piece; a wheel over nothing keeps its height). `0x16698`: car
height = plain average of the four, pitch = `atan((FR − RR)/wheelbase)`, roll =
`−atan((FL − FR)/track)`, no smoothing; shadow height = the average without the grass
jolts (+0x4ec).
**GRASS.** A piece's "surface code" (`0x8020`, block `+0x6b`) is just the FOURTH LETTER
OF ITS NAME; `e` (erba) = rough. A wheel on grass has its height jolted by `±((x + z) &
15)`; a FRONT wheel on grass sets player `+0x2f0`, which (a) makes braking past the
lock limit SPIN the car instead of locking (`0x16259`) and (b) tugs the steering by
`sqrt(1 − speed/maxspd) × ((x + z) & 63) × 0.0008` either way (`0x16458`).
`OS_GROUNDDUMP=1` prints the wheels; `OS_OLDGROUND=1` is the old stand-in.
**The surface search is the game's own since 2026-10-11** (`track_under()`, section 30):
first answer wins, in the game's cell, piece and face order. Not ported at all: suspension bob (`0x168a4`), crash/roll-over states
(`0x18518`), smoke, turning wheels, opponents (`0x1cd34`..). The old doubt about the
sideways slide's sign is settled -- the game is lop-sided there, see section 30.

**Testing without a keyboard:** `SDL_VIDEODRIVER=dummy OS_DRIVE_DEMO=3000
port/build/openscreamer build/dec/MAP1 1` — a crude robot driver laps the track and the
console prints speed, gear, rpm, height and wall hits each second (a lap of track 1
takes it about 53 s). Run it after touching `drive.c` or the car/track glue in `main.c`.

DRIVE keys: `UP` accelerate, `DOWN` brake, `LEFT`/`RIGHT` steer, `A`/`Z` gears on manual
cars, `N` next car, `BACKSPACE` back onto the road, `C` camera view.
There is no reverse (none seen in the code read so far).

---

### 9. WALLS, THE DRIVABLE AREA, MARKERS — all in the track's own files (2026-10-08)

Decompiled text: `build/decomp/collision.c`. Picture: `build/png/port_walls_overlay.png`
(`G` in the window, or `OS_SHOWEDGES=1`, draws the wall segments as yellow dots).

**Record run 0 of `MAPn.MMP` is the DRIVABLE AREA.** That is the meaning of the two
record runs: run 0 = pieces you can drive on, run 1 = everything else. The test
(`0x11c00` → `0x11ccc`) walks the run-0 records of the 3×3 cells round a point, rotates
the point into each piece's frame (`T * 2π/2048`), and asks whether it is over one of
the piece's faces (`0x1c09c`; for a wheel, `0x1bfc4` also solves the face's plane for the
height, and byte `+0x6b` of the piece's block is a surface code). It tries the point
and two more nudged by 10 units, so seams do not count.

**The cell's other arrays** (base at cell `+0x08` / `+0x0C`, counts at `+0x1C` / `+0x20`):

```
array 2, 12 bytes   float angle, int x, int y
    a point on the road's CENTRE LINE and the road's heading there
    (forward = (sin a, -cos a), the same convention as the car's heading).
array 3, 28 bytes   float angle, int x, int y, int x1, y1, x2, y2
    ONE WALL SEGMENT: middle, two ends, angle; normal = (cos a, -sin a).
    Chained end to end: two closed loops, one each side of the road.
```

Track 1 has 486 wall segments and 321 centre points; drawn from above the walls lie
exactly on the road's edges on every track. `0x120c0(mode)` returns the nearest of
either kind (mode 0 = centre point, used for the start position/heading and to know
which way the road goes; mode 1 = wall).

**The collision** (`0x11e74`, `0x11f98`, `0x16b88`, `0x16c90`; `player_walls()` in main.c):
the body has four corners — each wheel's position pushed 250 units ahead (front) or 150
behind (rear) — in the order front-left, rear-left, front-right, rear-right. Sideways
they are AT THE WHEELS (226 out), not at the bodywork. Never take the car's size off
its model: on track 4 the model includes the headlight beam, and doing so made the beam
solid ("invisible walls"). A corner
that is off the drivable area is put back relative to the nearest wall; the car moves
by the largest correction in each axis; then the wall response: speed × (1 −
0.9·|sin(heading + wall angle)|), nose swung by `(front-right out − front-left out) ×
that × 5`, and a shove along the push of strength **`|sin|⁴ × speed × 2 + 2000`, at
most 5000** units/s (`0x16df4`: `pow(|sin|, 4.0)` — Ghidra drops the argument), decaying
in the integrator (`0x171f0`) and cut to zero under 250. The fourth power is the point:
a glancing scrape gets barely more than the flat 2000, only a square hit the full
shove. (The port had `speed × 2 × |sin|` until Danik said scraping pushed back too hard.)

The point-on-track test is exact and uses EVERY face of the piece (`0x1c09c`; quads
`0x1c12c` = bounding box then four edge tests, triangles `0x1c37a`) — not only level
"floor" faces: the sloping kerb at the road's edge is track.

**One deliberate departure from the game, in where a corner is put back.** The game
(`0x11f98`) replaces the corner's across-the-wall coordinate with 30 units on the
*other side* of the wall's line from where it is. The port puts it 30 units *inside*
the line, never moves a corner outwards, and leaves alone a corner already inside by
30. Reason: the wall segments are straight chords, so on the inside of a bend the
track's surface ends up to ~130 units short of them (`OS_WALLCHECK=1` counts these:
2 of 486 walls on track 1, 67 of 619 on track 4). A corner in that sliver is off the
track but inside the line, and "the other side" throws it out through the wall and
back every frame. With the first port of the rule (a 30-unit nudge per frame, which is
also wrong — the correction is the whole way at once) that was Danik's "invisible wall"
and "car gets stuck inside the wall" on track 4. Which side of a wall is inside is
measured per wall (`walls_prepare()`); the files have walls authored both ways round.

**Tools for this area** (all headless, all print to the console):
`OS_WALLCHECK=1` walls vs track edge; `OS_HOLES=1` holes in the drivable area along the
whole lap (none on tracks 1 and 4); `OS_TRACKMASK=x,y,half` a character map of the
drivable area round a point; `OS_WHATAT=x,y` which pieces lie over a point;
`OS_HITDUMP=1` every wall hit with corners and push; and with `OS_DRIVE_DEMO`,
`OS_RAM=start,frames,steer` makes the robot drive into a wall on purpose and reports
how far inside the nearest wall the car stays.

**`MAPn.MRK` — the lap's markers.** int32 count, then 32-byte records: `+0x00` radius,
`+0x04` kind (1 in track 1's tunnel — engine sound), `+0x08` float, **`+0x0C` float
SLIP**, `+0x10` flags (2 = start/finish line or a checkpoint, see section 12), `+0x14` x, `+0x1C` y. The game (`0x1820c`)
tracks the marker last reached, looking only at it and its two neighbours; that marker's
SLIP is the factor in the cornering-force sum (player `+0x670`) until the next one. On
track 1 it is **0 on every straight** and 0.98 / 1.0 / 0.8 / 0.6 at the four bends: the
car can only skid where the authors allowed it. A lap counts (`0x19490`) at the finish
marker once the others have been passed; the port shows lap times in the title bar.
The `+0x08` float is the opponents' PACE for that stretch — see section 11.

---

### 10. CAMERAS, THE LENS, THE COCKPIT (2026-10-08; `build/decomp/camera.c`, `build/png/port_cameras.png`)

**The lens.** `screen = centre + coordinate * 128 / depth`, in 320x200 pixels, the same
128 for both axes (`0xd7493`: `shl eax,7` then a divide; the near plane constant at
`0x729ac` is −128) — about **103° across**. And the centre is not mid-screen: views are
set up with `0x4c610(320, height, height * 0.65)`, so the eye line is **65% of the way
down** (row 130 of 200). The port used a 50° lens centred on the screen until Danik said
every camera looked too close; `GAME_FOCAL`, `g_cy`, `g_vh` in main.c now.

**The camera record** is a global at `0x10a1c` (position at `+0`, pitch `+0x30`, heading
`+0x34`, lean `+0x38`, routine number `+0x88`); `0x19aa0` updates it once a frame.

**The table of views** at `0x1da0` has seven rows, but **the player gets four of them
while driving: rows 0, 1, 2 and 6** (Danik, from the original). Row 5, the low chase
staring at the bumper, is NOT a driving view — the port offered it for a day; do not
put it back. Rows 3–5 are presumably for replays. Rows are
`[key, camera routine, height of the 3D view, overlay, request flag]`:

| row | routine | 3D view | overlay | what it is |
|---|---|---|---|---|
| 0 | 0 | 200 | 2 | bumper cam |
| 1 | 0 | **132** | **1** | **cockpit**: same camera, short view, dashboard picture over the rest |
| 2 | 1 | 200 | 2 | near chase |
| 3 | 3 | 200 | 0 | trackside (nearest `MAPnCAM.SPL` node, turned to the car) — NOT ported |
| 4 | 8 | 200 | 0 | not read |
| 5 | 6 | 200 | 2 | low chase |
| 6 | 9 | 200 | 2 | far chase |

**The camera routines** (`0x19e68` dispatches): 9 / 1 / 6 are `0x1a3a8(.., 0 / 1 / 2)` —
1700 behind, 300 above, looking down 7.2° / 1300, 300, 10.8° / 1400, 100, 7.2°. The
camera's heading, **lean** and pitch each ease towards the car's (`0x21ff8`, stiffness
`0x51`); position is that far back along the heading and `car height + above −
behind·sin(pitch)` up. So the picture leans with the car on a banked road. Routine 0
(`0x1a2ac`): `camera_hi` (TRACK_DATA in `MAPnA.DTR`, 300 on track 1) above the car's
origin, turning rigidly with it, and the car is taken off the draw list. The game's
eased pitch/lean sources are the car's `+0x4d4` / `+0x4dc` (not ported; the port uses
the car's own pitch and roll).

**Changing view is a fly-across, not a cut** (`0x19ad8`, `0x19ef0`, `0x1a134`): the
difference between the old camera and the new (position and three angles) is kept and
added to the new camera while it dies away — position × `damp[90]` a frame, angles
eased to zero with stiffness `0x19` — until every angle's cosine is over 0.99995 and
every distance under 200 × frame time. Only then does the 3D view take its new height
and the overlay appear. (It looks like a curve; it is two exponential decays.)

**The old camera must be taken FRESH at the moment of the change** — `0x19ad8` runs the
old view's routine once more, at the car's present position, before saving it. The
port first used the camera the last frame was drawn from; that leaves one frame of the
car's travel in the difference (a couple of hundred units), which takes about two
seconds to die away — so going from the bumper cam to the cockpit, which are the same
camera, the dashboard appeared seconds late. Done right the difference is zero and the
change is immediate.

**The dashboard: `COCKnn{A,B}{L,H}.PKC` in `GAME.DAT`** (nn = car model, A/B = the two
liveries, L = 320x200). **`.PKC` format, solved:** a run of int32s — a negative number
skips that many pixels (see-through), a positive one is followed by that many palette
indices to copy. Track palette.

**Needles and gear lamp** (`0x236dc`; needle `0x2390c`; `dash_*` in main.c,
`build/png/port_dashboard.png`). Positions are the `COCKnn{H,L}` sections of `CARS.DTR`:
needle length, first lamp sprite; speedometer pivot, rev-counter pivot; lamp position for
N, 1..6. Speed needle angle = `speed / maxspd × 4.084` rad, rev needle = `rpm / maxrpm ×
3.613` rad (clamped), both eased (stiffness `0x5a`); drawn at that angle **minus 2 rad**,
clockwise from straight up, as three lines fanning from the pivot to one tip, colour 255.
(Checked against the art: zero lands on the dial's "0" and full scale on "350".) The lamp
is a sprite from `GAMEL.SPR`: the dashboard's first lamp sprite + the gear (section 12).

Port: `C` cycles bumper cam / cockpit / near chase / far chase in DRIVE (it starts in
the far chase), `F1`–`F4` pick one directly (`F5` is the FPS counter);
`OS_VIEW=n` and `OS_VIEWCHANGE=frame,view` drive it in `OS_DRIVE_DEMO`. The camera has
`roll`; the backdrop leans with it. (`K` is culling now.)

---

### 11. OPPONENTS AND THE STARTING GRID (2026-10-08; `build/decomp/opponent.c`, `build/png/port_race.png`)

**A `.SPL` file holds SEVERAL splines**, one after another: `int32 count, int32
start_node, count × 56 bytes`, repeated to the end of the file (loader `0x1b00c`; the
second header word was read as "0" for two days because it is 0 in the first spline of
track 1). `MAPnOPP.SPL` has **ten**: opponent k drives spline k, the tenth is the
player's. `MAPnANM.SPL` has one per moving-scenery object.

**The grid is the splines' start nodes**: node `start_node` of spline k, its own point
(`+0x04/+0x08`), is car k's place — two columns, about 3500–4100 apart, the player last.

**The curve the game actually evaluates** (`0x1d54c` + `0x1e7a4`) is a **uniform
quadratic B-spline over the nodes' HANDLES**, two pieces per node:

```
piece 0:  A = node.in (+0x14)   B = node.out (+0x20)   C = next.in
piece 1:  A = node.out          B = next.in            C = next.out
point(u) = A·(1−u)²/2 + C·u²/2 + B·(1 − those two)        u in 0..1
```

The node's own point is NOT on the curve (it is roughly the midpoint of its two
handles, which is why the cubic-Bezier reading of section 5 also landed on the road).
The camera path in the port still uses that older reading; opponents use the real one.

**An opponent** (records at `0x10ab4`, 0x590 bytes, count at `0xf8e8`; the five numbers
of its `OPPONENTS` row in `MAPnA.DTR` are loaded straight into the record at `+0x2e4`):
- speed (`0x1ced0`): `target = max_spd × PACE`, where PACE is the float at `+0x08` of
  the lap marker it last reached (`MAPn.MRK` — the field that was unexplained), plus
  `skill` on top of `max_spd` when the player is ahead of it (the catch-up); it gains
  `accel` units/s² below target and loses `brakes` above;
- it moves that far along its curve each frame (`0x1d2fc`); no steering, no skidding;
- position = curve point + a sideways offset that collisions give it and that drains
  away after three seconds (`0x1d6ac`, `damp[0x60]`);
- it is only fully simulated within 35000 units of the camera (`0x1e2fc`);
- its car: the next row of `CARS_DATA`, skipping the player's (`0x142e8`).
`A`..`E` variants of `MAPn?.DTR` differ in these rows — presumably the difficulty levels.

**Port:** nine opponents, grid start, held until the green light (section 12). (`ENTER` no longer
restarts a race — that was a testing key and Danik hit it at the end of races; it only
moves the end screen on to the replay / the question.) The running order is the game's (`0x20418`, section 30).

**Car against car — from the game** (`cars_collide()`; `build/decomp/hud_and_carcollision.c`).
Every car is **two circles** on its centre line, built from the BODY's corners
(`0x136c8`, `0x142e8`): the nose is 250 ahead of the front wheels, the tail 150 behind
the rear ones; with L that whole length each circle sits L/4 in from its end and has
radius L/4 (334 on AUTO00, whose half-width is 376). *The port first used a quarter of
the wheelbase from each axle — a third too small and 250 short of the nose — and cars
visibly passed through each other (Danik, 2026-10-08).* What the game's own circles
still leave uncovered: 25–130 units at the nose and tail, ~40 at the flanks. Pairs are only tested within 1500 units
(`0x12378`, `0x126e4`).
- *Player touches opponent* (`0x173f8` → `0x17818`): the OPPONENT is moved out by the
  overlap — unless that puts it off the track (`0x11c00`), then the player is moved.
  Opponent in front: it is shoved on at half the player's speed and knocked round by
  speed × 0.0001 rad (max 90°; it straightens by itself); the player loses 5% of its
  speed per frame of contact (`damp[90]`), is shoved back at 0.21 × speed, nose pushed
  away at speed × 0.0002 rad/s. Opponent behind: its speed × 0.7.
- *Opponent against opponent* (`0x1cffc` → `0x1d1f0`): the faster one drops to their
  average speed, the higher-numbered one is moved over.
- *Overtaking* (`0x1e404`): an opponent looks three radii ahead of itself; if the player
  is within `2·r + r_player` of that point and going slower, it shifts its line by just
  enough to clear. That is all the steering an opponent has.
- Not ported from this group: `0x124c0`, the player hitting the knock-over props (cones
  and such: they are sent flying at a multiple of the car's speed).
`OS_NOOPP=1` removes them; `OS_OPPDUMP=1` with `OS_DRIVE_DEMO` prints their laps and speeds.

---

### 12. THE RACE SCREEN — fonts, sprites, clocks, radar, start lights (2026-10-08)

Pictures: `build/png/port_hud.png`, `build/png/port_hud_tracks.png` (all six tracks). Decompiled text: `build/decomp/hud_text_sprites.c`,
`build/decomp/timers.c`. Port: `hud_*` in main.c. `OS_NOHUD=1` hides it, `OS_NOTIMER=1`
keeps the engine pulling when the countdown is out.

**`GAMEL.FNT`** (loader `0x255f8`, drawing `0x24fc0(x, y, string, font, colour)`,
measuring `0x25448`): one byte = number of fonts − 1 (7 fonts); per font 12 bytes
`height, first character, count`; per font per character 8 bytes `width, offset`; an
int32 size; the pixels. A glyph is `height` rows of `(width+1)/2` bytes — **two pixels
per byte, high half first**, each a shade 1..15 (0 = see-through) drawn as palette index
`colour*16 + shade`, so a text colour is one of the palette's 16-colour ramps. Advance =
width + 2. In a string, byte 1 = fixed pitch, 2 = proportional, 3/4 = the extra column
off/on. Fonts: 0 = full alphabet 30 high; 1 = big digits (its `:` is drawn as `/`);
2 = LCD digits 10 high; 3 = tiny clock digits with `:`; 4 = medium digits; 5 = small
alphabet; 6 = big white digits.

**`GAMEL.SPR`** (loader `0x25750`, drawing `0x25ab0`): `int32 count, int32 pixel bytes`,
count × 20 bytes `[x offset, y offset, w, h, offset]`, then raw palette indices, 0 =
see-through. Sprites 0–3 start lights, 4 radar arrow, 5 radar dot, 7–54 gear lamps in
each dashboard's size, 55+ the CD player's buttons. The engine keeps a list of sprites
to draw at `0x1cb10` (40 bytes each: flag, x, y, sprite number).

**Pictures** (`.PKC`, loaded by `0x1b22c`): `IMPLET_L` = the labels LAP, POSITION, TOTAL,
LAP TIME, BEST LAP (drawn every frame); `IMPRES_L` = the outside views' rev counter with
km/h and gear windows; `CDFRONTL` = the CD player.

**The race screen (`0x23400`):** lap `"%d:"` at (8, 11) font 1 colour 5 and the number of
laps after it at y 20 in font 4; position at (276, 11) font 1 colour 5; the countdown,
whole seconds, centred at y 8 in font 6 colour 2; total / lap / best-lap time as
`mm:ss:hh` at (8, 56), (8, 72), (8, 88) in font 3 colour 2. **Instruments (`0x2367c`):**
view overlay 1 = dashboard + needles + lamp sprite; overlay 2 = `IMPRES_L`, rev needle
pivoted at (271, 168), 30 long, colour `0x75`, angle `rpm/maxrpm × π − 2`; speed as
`"\x01%03d"` of `speed × 0.017` at (271, 187) font 2 colour 7; gear at (304, 187) colour 1.

**The radar (`0x23a68`, lines `0x1057c`):** a 72×72 window at (4, 124), darkened to 75%
through a palette lookup (`0x25870` builds it); the track's WALL SEGMENTS (MMP cell array
3) as lines in colour 255, turned nose-up round (40, 160); opponents = sprite 5, player =
sprite 4 at (38, 158). One pixel = `32768 / zoom` units, zoom 24 / 48 / 72 from the
option at `0x1d2a0` (0 = off). The port uses 48 — **the game's default is not known.**
(The engine's sine table at `0x20190`, 2048 × 8 bytes, has amplitude 32768, not 16384.)

**Start lights (`0x20d45`):** sprite `whole seconds / 2` at (120, 54): out, one red, two
red, then at **6 s** the green one and the race is on (flag 1 at `0x18e18`); gone at 8 s.

**The clocks (`0x18a04`, `0x19490`, `0x195fc`, `0x19668`):** total (+0x78c) and lap
(+0x794) time start at the FIRST crossing of the finish line (+0x784) — the grid is
behind it. The countdown (+0x790) starts at **15 s**, runs from the green light, and at
the line **and at every checkpoint** becomes **what is left / 5 + `bonus_time`**
(TRACK_DATA; 50 on track 1). A checkpoint is any `MAPn.MRK` marker with flag 2: marker 0
is the line, track 1 has no other, tracks 2–6 have one to three more. (Flag 4 on some
markers is not read yet.) At zero the engine stops pulling (flag `0x40` at `0x18e19`) and the
race ends when the car has stopped (flag `0x10` at `0x18e14`). Best lap is +0x798.

**The end of the race (`0x20eb8`, `0x20f88`, `0x21048`; `build/decomp/race_end.c`;
`build/png/port_race_end.png`).** Flag 8 at `0x18e14` (finished) or `0x10` (out of time
and stopped) starts it: the engine sound stops, a voice plays (`0x2479c` 0xb won / 0xc
not / 0x19 time over), the game asks for view-table row 3 (the trackside camera), and
flag 4 runs the end screen for 5 s (and until the voice is done), then the race exits.
What is drawn, all centred in font 0 colour 2: finished — `GREAT!` at y 62 if first,
`"%s %s"` of the ordinal and `POSITION` at y 90, the total time at y 120; time over —
`TIME OVER` at y 90. The texts are the lines of `LOCAL.DAT`'s `GAME.TXT`, pointed to
from `0x18f90` (0 POSITION, 1–10 1ST..10TH, 11 GREAT!, 13 TRY AGAIN, 14 YOU MADE IT!,
22 TIME OVER); modes 4, 0x10, 0x20, 0x40 have their own variants (`0x24174`…).
**Reading Ghidra's labels:** `_LAB_00018fe4_4` means address `0x18fe4 + 4`.

**Port stand-ins:** three laps (the game's option, `0x1d2ac`, is not read; `OS_LAPS=n`
overrides, `OS_TIMER=s` sets the countdown's start); the end screen stays up until
ENTER instead of leaving after 5 s; no voices.

**The trackside camera (view-table row 3, `0x1a620`; `trackside_camera()`).** Its
positions are the nodes of **`MAPnRPL.SPL`** (the loader `0x14fb4` puts `CAM` at
`0xf908` and `RPL` at `0xf90c`, and this routine reads `0xf90c`). It takes the node
nearest the car, looks from there straight at the car, and if the car is more than 1500
units away moves along its line of sight until it is 1500 from it; no lean; the car and
its shadow are put back on the draw list. Used at the end of the race here; the game
also has it in the view table for replays.

### 13. THE BACKDROP (2026-10-08; `sky_paint()` in main.c)

Danik saw the backdrop jump and look mirrored when turning. Both were true: the port's
first backdrop wrapped `MAPn.BCK` round a cylinder at the lens's scale (1.9 times round a
turn, so it jumped where the heading wraps) and sampled it right-to-left.

The game (`0x83a68`; `0x83b44` is the leaning version) just **copies** the 512×256
picture to the screen: the column under the left edge is `((2047 − heading) / 2) mod 512`
— one pixel per two heading units, so the picture goes round **exactly twice per turn**;
its top row is at `eye line + 22 − 256 + 128·sin(pitch)`; everything below it is filled
with one colour: the palette index in TRACK_DATA's `color` (`0x1337d` copies `0xcf40`
into the view record at +0x10 = `0x7a50c`; TRACK_DATA lands at `0xcf40`: color,
camera_hi, bonus_time...). Game heading = port yaw + a quarter turn.

### 14. PROPS AND MOVING SCENERY (2026-10-08; `build/decomp/props_and_animations.c`)

Pictures: `build/png/port_props.png`, `build/png/port_moving_scenery.png` (bus and truck
on track 1's bridges, taken from the robot's lap), `port_moving_scenery_models.png`.

**Props** (`0x15178` sets out, `0x124c0` hit, `0x205b4` flight; records of 0x40 bytes at
`0xd6fc`, count `0xd6f4`). Every node of the mode's layout spline is one prop: point =
place, **the height field (+0x0C) = the KIND**, a row of `OSTACLES` in `MAPnDAT.DTR`.
Each row has THREE names (cone / bunny / skittle…) and option `0x1d270 & 3` picks the
column. The prop stands on the drivable surface under it, heading = the nearest road
centre point's angle (`0x120c0(0)`; + π when `0x1d270 & 8`, which looks like a
reversed-track option). On track 1 in the normal mode they are the roadside signs.
- hit: either of the car's two circles within `radius + 200`; thrown along the car's
  heading at `speed × 1.4`, upwards `speed × 0.02` per frame; `+0x7b8` counts props hit
  (and `0x18b50` gives +1 s of countdown per new one — in which mode is not read).
- flight: speed −2000/s; fall 500/s per second; tumbles `speed × dir × dt × 0.008 /
  0.0015`; ground bounce keeps 0.8 until under 50; leaving the drivable area it turns
  back with 0.3 of its speed; at rest it lies on its side (pitch + π/2) and can be hit
  again. The upward speed is added once per FRAME regardless of frame time; the port
  scales it to 30 frames/s.
- An object's three angles (+0x14, +0x18, +0x1c of its draw record) are pitch, heading,
  roll, each `radians × 1024/π`; a car's are its `+0x4c8/4cc/4d0` (`0x15cbb`).

**Moving scenery** (`0x15374`, `0x20944`, cursor `0x20b0c` / `0x22358`; records of 0xbc
bytes at `0xf17c`): `ANIMATIONS` rows are `name speed spline sound`; the object runs
round spline `spline` of `MAPnANM.SPL` for ever at `speed` units/s — same quadratic
B-spline over the handles as the opponents, height = the nodes' own +0x0C run linearly
between nodes, facing its direction of travel. **It only moves while within 52000 units
of the camera**, so each one waits at its spline's start until the player comes near —
that is the timing that puts the bus on the bridge as you arrive. `AKKNULLA` rows with
speed 0 are empty objects carrying only a sound (`0x21b80` is the sound, with distance
and pan). Not at all when the game's animations option (`0x1d2b4`) is off.

**Placement check (2026-10-08, after Danik reported props "on the grass" that he
remembers "in the right lane").** NOT RESOLVED. What is established:
- the game uses the node's x and z exactly as stored (`0x15178`: no sideways shift, no
  scaling; height = the drivable surface under it), and so does the port;
- on track 1 in the normal mode (`OST`) the 24 cones stand on or beside the road's edge
  line and the 21 trestle barriers (`AZSTAC`: a thin red-white A-frame, 580 wide, facing
  along the road) about 215 units outside the white line — none in a lane;
- **every prop model is centred on its origin**, so standing on the ground only its top
  half shows (a cone is 141 units = 43 cm, the "girl" 2 m). That gives sensible sizes,
  and nothing in the code lifts them, but it has not been checked against the original;
- the `CNG` and `SLA` layouts DO put their props on the road surface (track 1: 22 of 36
  and 21 of 28) — if Danik's memory is of cones in a lane, it may be of those modes;
- `MAP5OST.SPL` is byte-identical to `MAP4OST.SPL`: track 5's normal-mode props are
  track 4's coordinates and land nowhere near its road.
Tools: `OS_PROPDUMP=1` now also prints the piece and material each prop stands on;
`OS_PROPFILE=CNG|SLA` loads the other layouts.

**Port:** `props_*` and `anims_*` in main.c. `OS_NOPROPS=1`, `OS_NOANIMS=1`,
`OS_PROPS=1|2` (the other two columns), `OS_PROPDUMP=1` (places), `OS_PROPKICK=n,speed`
(throw prop n and print its flight). **Open question for Danik:** the signs' painted
face points the way the code says (the road's direction), which means you see their
grey backs as you drive up — right or wrong against the original?

### 15. ANIMATION — texture frames, moving parts, the clock (2026-10-08)

Pictures: `build/png/port_banner_frames.png`, `texture_frames_track1.png` (every frame of
track 1's animated textures and of a car's), `port_moving_parts.png` (the seven moving
objects of tracks 3–6 at three moments).

**The animation clock** (`0x1fcc4`): `0x18e08 += frame time × 0.3`, wrapping at 1;
`0x21538 = that × 65535`. One turn of the clock = **3.33 s**.

**Texture frames** (`0x8ab47`, `0xd5980`): `frame = frames × phase >> 14`. `phase` is the
drawn object's record `+0x20 >> 2`; when that field is −1 — every MMP record's is — it
is `clock & 0x3fff`, so a track texture plays through once every **0.83 s**. Per track
there are 4–9 such textures: the START banner in the wind (15 frames; tracks 1 and 4),
track 1's giant TV screen (10) and turning "Skulls" sign (20), banners and signs
elsewhere (up to 61 frames on track 5).
- **A car's textures have 8 frames, and its record `+0x20` picks one** (`0x173a9`):
  `paint job (0 / 0x8000) | 0x4000 when NOT braking | 0x2000 at or under 4000 units/s`,
  frame = that `>> 13`. So +4 = second paint job (CARS_DATA `animation`), +2 = brake
  lights off, +1 = slow (the wheel's still picture; the other is its blur).
- On every track: an 80×60, 6-frame see-through texture (routine 224) = two 3-frame
  puffs of smoke / fire, and a 254×254 texture with 2–3 "frames" on routine 160 that
  are sky / lights / red pictures — routine 160 looks like a reflection map.

**Moving parts** (`0x8abf8`, `0x8ad14`; `mesh.c`): an object whose header `+0x10` is not
−1 has that many frames; `frame = frames × clock >> 16` (one run per 3.33 s). After each
block's position record (block `+0x0C`), from its `+0x1C`, come `frames × 0x30` bytes:
12 int16 = 3×3 rotation (16383 = 1.0) + translation, then the same for the inverse.
Written with Z before Y: `x' = m0·x + m1·z + m2·y + t0`, `y' = m3·x + m4·z + m5·y + t2`,
`z' = m6·x + m7·z + m8·y + t1`. Objects: track 3 V335 ×2 (180 frames), track 4 V213
(400) and V235 (300, ten turning pillars), track 5 V240 ×2 (300, a crane) and V327
(100, a barrier arm), track 6 V211 (150) and V275 (100). None on tracks 1–2, none in
the `.3DX` files. `scene_load` prints where each stands.

`OS_ANIMCLOCK=0..1` fixes the clock for `--shot`.

**Wheels, shadow, smoke (2026-10-08;** `0x15c0c`, `0x19818`, `0x19970`;
`build/decomp/smoke.c`; pictures `port_wheels_brakelights.png`, `port_shadows.png`,
`port_smoke_ab.png`**).** At run time every block of a car has a 0x20-byte record:
three angles at +8 / +0xC / +0x10 (about the axle, the vertical, and lengthways) and a
height at +0x18. Each wheel record (`+0x194 + 0x54·n`) names its two blocks at +0x48
and +0x50.
- wheels turn `speed / 15` units per FRAME (= rolling without slip at the game's frame
  rate) — **only the hub disc (block flags 0x800; the block the wheel record names at
  +0x48) turns.** The tyre block (flags 0x1) is just the lower half of the tread plus
  the inner wall, what shows under the body; spin it and the wheel becomes a flat disc
  with nothing behind it (the port did, for a day); the two front wheels steer `steer / (limit × 1.5)` rad — always 38° at full lock;
- the four body blocks lean by `+0x4e0 × 2` and `+0x4e8 × 4` (the suspension, `0x168a4`)
  — NOT ported;
- the shadow is `SHADOW.3DX`, a separate object (handle `+0x468`): the car's place,
  heading and tilt, ground height − 2, nudged 20 units away from the camera (`0x18cbc`);
  its material is routine 224, i.e. it darkens through `MAPn.SMG` (rows 32–41);
- smoke: eight `SMOKE.3DX` puffs per car (slots of 0x28 bytes at `+0x308`); while
  skidding, one starts at each REAR wheel every 0.05 s, plays its 6 texture frames in
  2/3 s (`+0x20 += 1.5 × 65535 × dt`, `0x118e8`) and is carried with the car.
  A puff always faces the screen -- that is the game's own doing (object flag 0x8000,
  section 30) -- and only the player smokes (opponents are given no smoke objects).
- `0x20e63` (opponents before the start) sets their `+0x4f4` to a slow sine × 15000 —
  that is engine revs for the sound, not a visible rocking.

**Body lean (`0x168a4`; `suspension_step`).** Per wheel a deflection, ±18 units: kicked
by the change of speed × 0.00175 (front −, rear ×3 +) and by `turn/dt × speed² × 1.5e-9`
(left −, right +), each divided by the frame time; it swings as `cos(4π·t + π)` and dies
away (`damp[61]` sideways, `damp[71]` lengthways, eased in with `damp[0x3d]`). Body drop
= the average; roll = `−atan((FL − FR) / track) × 4`; pitch = `atan((FR − RR) /
wheelbase) × 2`. **The game's `track` (+0x2e4) is NEGATIVE** (`−(xFL − xFR)`, and the
model's +X is the car's left) — miss that and the car leans INTO the bend (it did, for
one build). Both kicks are divided by the frame time, so the lean depended on the PC's
speed; the port assumes 30 frames/s for it.

**Block flag `0x2000` is the reflection pass — settled 2026-10-09.** The face-drawing
routine keeps the block's flags (`0x8a66e` → `0x74280`) and tests only that one bit
(`0x834fc`): set, it calls `0xc1fc0`, which works out how far the object has moved since
last frame (that is the sliding offset below) and falls into `0xc2230`. Over every model
in the game (2530 blocks: all `.3DX` and all six `.UNR`) the 49 blocks with the flag are
exactly the 49 that have per-vertex normals, which is what the port keys on. Nothing
was missing.

**Reflections = routine 160 (`0xc2230` makes the coordinates, `0xdb310` draws;
`port_reflections.png`).** A block header's `+0x20` is an int32 count + that many float
unit vectors: PER-VERTEX NORMALS (a car's body, nose and tail have them). For each
corner, with e = unit vector eye → corner and n = its normal: `r = 2(e·n)n − e`, and the
texture is read at `127 + 127 × (r sideways, r up)` minus an offset that grows with the
distance travelled (× 0.002 a unit; × 0.03 in a tunnel, `0x1813c`). WHICH PICTURE:
`frames × (object +0x3c) >> 16`, and +0x3c is the car's `+0x7b0` = 0 / 0x8000 / 0xff00
for lap-marker kind 0 / 1 / 2 — sky in the open, the lights in a tunnel.

**The crash (`0x16c90` decides, `0x18518` states 2 → 1; `port_crash.png`).** A wall hit
is a crash when `speed × (angle off the road's heading / 90°) > maxspd × 0.3` (and the
surface kind is 0). Once: speed × 0.6, heading = the road's, thrown up at 3000 units/s,
turning 12 rad/s in pitch and in roll, and the game cuts to **view-table row 5** (so
that is what the low chase is for) remembering the view it left. Every frame: gravity
6000, both rates × `damp[97]`; on each landing a smoke puff, the fall × −0.4, the car
set level and given −1 / ±0.6 rad/s; the fourth landing ends it and the view returns.
Throttle, skid model and wall response are off meanwhile; the shadow stays flat.
State 5 is the wheelspin off the line, state 6 ends it. `OS_CRASHAT=<seconds>` crashes
on purpose; `OS_NOCRASH=1`, `OS_NOLEAN=1` switch these off.

**There are NO skid marks in the original** (Danik, 2026-10-08: he had misremembered).
That agrees with the code: nothing in the player's per-frame update draws or records
any. Closed — do not look for them, in STARTL or in the SVGA build.
`port_window_pillars.png`. `tools`-free check: orbit a car with `OS_POS`/`OS_HEADING`
shots from eight sides — a single side-on shot missed this twice.
**Prop placement is confirmed right** by Danik (2026-10-08) with the stored positions
used exactly. Nothing is open against the hi-res build.

**Opponents, from the game (2026-10-08; `build/png/port_opponent_crash.png`):**
- they have NO smoke objects (`0x142e8` creates only the car and `SHADOW`) and no
  suspension model;
- ground (`0x1cb9c`): ONE point, the drivable surface under the car's middle; tilt =
  that face's normal turned into the car's frame × π/46080 (1.117 × the slope); off the
  drivable area at over 3000 units/s the car is stopped;
  The object's pitch / heading / roll are assembled in `0x1cd34`: ground tilt + crash
  angle each, heading = curve tangent + crash turn + the offset at `+0x4c8`. **Sign
  trap:** the port's lean is + when the RIGHT side is higher; the first port of this had
  it reversed and the cars leaned against every banked bend at twice the banking
  (Danik's "two wheels at 45°"). `OS_TILTCHECK=1` compares it with a four-wheel
  measure over a run;
- in a bend (`0x1df24`): `v = (speed × sband)² × (heading turned this frame)`, held to
  ±2; the heading offset `+0x4c8` eases to `v × 0.5` (stiffness 0x4f) — the nose turns
  in, and a collision's knock dies away towards that — and the BODY blocks to `v ×
  0.02` of roll and pitch (stiffness 0x4c; `0x1c83c` writes them to the body blocks
  only). So opponents do lean, but by 2.3° at most. `sband` is the fifth number of the
  `OPPONENTS` row;
- crash (`0x1cffc`, states at `+0x2f8`, handlers `0x1d824`): when its speed is more than
  5000 below what it was at the start of the frame, state = 2 or 3 by `((x + z) >> 2) &
  1`. 2 → 1 is the player's tumble with the same numbers (up 3000, gravity 6000, 12
  rad/s pitch and 12 rad/s yaw, ×0.4 bounce, four landings); 3 → 4 is a spin on the
  road from 24 rad/s until it has turned 4π. `OS_OPPCRASH=car,seconds,kind` forces one;
- engine sound (`0x1dffc`): only cars within 35000 and only THREE voices, a nearer car
  taking a farther one's; pitch `speed × 2.4 + 13568 + 9 × (units nearer this frame)`;
  loudness `((35000 − d)/35000)² × 23552`; panned (port: mono). Car 12 uses the other
  sample of the table at `0x1e2c`.

### 16. STORED HEADINGS ARE SLIGHTLY SHORT — the port snaps them (2026-10-08)

Danik found lane lines stepping and slivers between road tiles in a bend of track 1
(`build/png/port_heading_snap_ab.png`, left as stored, right snapped). Cause: the pieces
were laid out at whole multiples of 5°, but the `T` stored in the MMP record is up to
2.5 units short of the exact value — 90° is 510 not 512, 180° is 1022, 270° is 1535,
350° is 1990 (exact 1991.1). Neighbouring road pieces only meet when turned by the
exact angle: at the join Danik pointed at the sideways miss is 25 units with 510 and
0.1 with 512; over all neighbouring road pieces of tracks 1–3 the mean corner gap drops
from ~18 to ~10 units.

**This is a deliberate departure from the game's code**, which uses `T` as stored,
straight into its sine table (`0x8b287`; the table is in the CODE object at flat
`0x30190`, 2048 × 8 bytes, a plain sine of amplitude 32767 — an earlier note looked for
it in the data object and found zeros). So by its code the original has the same
25-unit steps; whether they show there is for Danik to say. `scene.c` snaps a heading to
the nearest multiple of 5° when it is within 2.5 units of one; `OS_NOSNAP=1` turns that
off. The ~10 units that remain are mostly ALONG the road (tiles overlapping end to end).

### 17. SOUND (2026-10-08; `port/src/sound.c`, `sound_*` in main.c, `build/decomp/sound.c`)

HMI is replaced by a 32-voice STEREO SDL mixer. From the game:
- samples = the `.RAW` files of `SOUND.DAT`: 8-bit unsigned mono, **11025 Hz** (`0x1f1eb`
  sets the driver up with `0x2b11`);
- `SFXSBASE.DTR` (GEN.DAT) lists the effects and **a sample's number is its row**:
  0 skidfx, 1 crash, 2 oppmotor, 3 automoto, 4 wallh1, 5 hit_0, 6 click, 7 autogall,
  8 dentone, 9 door, 10 taz3, 11 taz4. Row = name, volume (low 16 bits, 0x7fff full),
  loop (−1 for ever, 0 once, n times), loop start, loop length, pitch, pan. Track sounds
  (`SOUND_FX` in `MAPnDAT.DTR`) and the commentary (`SPFXBASE/SPFXFULL.DTR`) are appended
  after them (bases at `0x18d84`, `0x1906c`);
- the calls: `0x1f708(id)` start → handle, `0x1f870(h)` stop, `0x1f7d4(h, pitch)` pitch in
  16.16, `0x1f754(h, id)` swap the sample, `0x1f79c(id, h)` start unless already playing;
- **engine**: one looping voice (player `+0x7cc`), started when the race is set up
  (`0x128dc`), pitch `rpm × 14 + 32768` every frame (`0x19408`); sample from the table
  at `0x1d78` = automoto, or autogall when the lap marker's kind is 1 (tunnel), or taz3 /
  taz4 for car 12; **skid**: sample 0 while the skid flag of `0x17d04` is up (handle
  `+0x7d0`); **wall** 4, **crash start** 8, **prop hit** 5,
  **car against car** 1 (`crash`): `0x17818` starts it afresh on EVERY frame of contact
  (the port: at most 30 times a second).
- **commentary** (`0x2479c` queue an event, `0x24820` play the queue, `0x24a24` pick the
  sample, `0x2425c` the per-frame triggers; ported as `comment_*`). 27 events × up to 3
  recordings, table at `0x1fb0` (sample numbers counted through `SPFXBASE.DTR` THEN
  `SPFXFULL.DTR`): 0–3 three two one go, 4 check, 5 lap, 7 newlap, 8 beat, 9 crash,
  10 skid, 11 win, 12 lose, 13 bad, 14 well, 15 hitcar, 16 back, 17 stop, 19 cones,
  20 bend, 21 lead, 22 hurry, 23 lapped, 24 loslea, 25 notime, 26 all. A queued event
  has a mode (0–2 that recording, 3 take turns) and a delay; with no voice speaking a
  clock runs, the shortest delay goes first, the clock restarts when it finishes.
  Triggers keep a timer per event (`0x19228`): hold-for-T-then-once (`0x24bec`),
  every-T-while-true (`0x24c68`), once-until-re-armed (`0x24cd0` / `0x24d1c`).
  From `0x2425c`: heading within 45° of the road reversed → back, every 5 s; speed
  under 1000 → stop, every 8 s; countdown ≤ 5 → hurry (re-armed at each bonus); first
  for 5 s → lead; then second or worse for 5 s → loslea; **lap-marker flag 4 → bend**
  (that is what flag 4 is); sliding for `0xfda0 × 2.9` s → skid; place worse than
  `max(2, +0x788)` for 8 s → bad. Elsewhere: checkpoint → check (recording by time
  left: < 3 s 2, ≤ 15 s 0, else 1); line → lap (2 into the last lap, 1 after a best
  lap, else 0) and newlap for a best lap; crash → crash; end → win / lose / notime.
  **hitcar** (`0x2454a`): player `+0x7c4` eases to 1 while touching a car and to 0 when
  not (`0x175aa`, `0x177f7`, stiffness `0x32`); spoken whenever it is over 0.8.
  **lapped** (`0x24464`, modes 1 and 2): held 2 s, when the player is a lap up on any
  opponent (more laps AND further round the lap, or two laps more).
  **cones** (19) and **all** (26) belong to the cone-carnage / slalom modes only
  (`0x1d25c & 0x20 / 0x40`: a cone hit; all cones or gates done) — they cannot be heard
  until those modes exist. **beat (8) and well (14) have NO trigger anywhere in
  STARTL.EXE**: every call of `0x2479c` and of the three helpers passes a constant, and
  none is 8 or 14 (the `mov eax,8` beside "crash" at `0x18540` is the DENTONE sample).
  The recordings are in the files and never played. Events 6 and 18 have no samples.
  Settled 2026-10-11 from the call sites: the bend hold time IS 0 (`push 0`), `0xfda0`
  IS the car's grip (player `+0x488`), and `+0x788` is NOT "best place so far": it is the
  number of opponents, written once at set-up (`0x12c34`) -- "bad" = last for 8 s.
- **sounds with a place** (`0x21b80`, the moving scenery; `sound_from()`): start within
  40000 units, stop beyond; loudness `((40000 − d)/40000)²`; pitch `65536 + 9 × (units
  nearer this frame)`; the sample is the `sound` column of ANIMATIONS, a row of the
  track's own `SOUND_FX`.
- **THE MIX (2026-10-09; Danik: engines quiet and muffled against the music).** The
  driver is opened at 11025 Hz, 8-bit (`0x1f1eb`), and the three master volumes — music,
  effects, speech (`0x20804`, `0x20808`, `0x2080c`) — all start at `0x6000` of `0x7fff`
  (`0x1bc2f`). `sound.c` now works an effects sample out 11025 times a second from the
  NEAREST stored sample and holds it for two output frames (it used to mix at 22050 Hz
  sliding between samples = a low-pass filter = the muffling), at 192 per 8-bit step
  (256 × 0.75; it was 80). Intro, engines only: rms 1688 → 3917, music about 1900.
  `OS_SFXVOL=x`, `OS_MUSICVOL=x`. How loud HMI's FM music was beside its digital
  channel is in the discarded driver: the music's 0.6 is still the port's guess.
- **DOPPLER, AND THE LISTENER IS THE CAMERA** (confirmed 2026-10-09 after Danik heard it
  in the original's intro; `0x1dffc`, distance at `0x1e34b`). A car's distance for
  sound is 2D, from the view's camera (x, z at `0x13cc8` / `0x13ccc`), not from the
  player's car. Pitch = `speed × 2.4 + 13568 + 9 × (distance last frame − distance now)`:
  that last term is the Doppler shift (the scenery's `0x21b80` has the same 9). When the
  car's own camera has CUT (`+0x578`, set by `0x1a920`) last = now, so a cut is silent.
  The term is per FRAME, so its strength went with the frame rate: the port takes 30
  frames/s (`g_doppler`; `OS_DOPPLER_FPS=n`) — measured in the intro: +9 % coming at a
  camera post at speed, −9 % going away. The port had the term but (a) measured from the
  player's car — in the intro that is nowhere, and `sound_from()` made the scenery silent
  there — and (b) let a camera cut through as a one-step squeal; now `g_ear` is the
  camera and a jump of over 1000 units in a step counts as a cut. `OS_DOPPLERLOG=1`.
  **On the grid the opponents REV** (`0x20e63`, ported 2026-10-09): until the green light
  the speed their note is made from (`+0x4f4`) is `sin(car number × 0.5 × grid clock) ×
  15000`, then 0 and the real speed. The swing takes the pitch below zero for part of
  it; what the driver did then is not known — the port holds the note at its lowest.
- **STEREO** (`0x21cfb`, `0x1dffc`; `sound_pan()`): the line from the source to the
  camera is turned into the camera's frame, and `pan = 0x8000 ± 32767 × sqrt(|sideways|
  / distance)` goes to HMI (`0xc6670`) — a square root, so a source a little to one
  side is already well over. Only the opponents' engines and the scenery's sounds are
  panned; the player's own sounds and the voice stay in the middle. How HMI turns a pan
  into two volumes is in the discarded driver: the port fades the far side linearly.
- the opponents' engines follow the game's own rule now (`0x1dffc`, section 15's
  opponent notes). `OS_NOOPPSOUND=1` silences them.
- **MUSIC** (`port/src/music.c`; `0x134f8` race, `0x29e47` menu, loader `0x2faac`). With
  the MUSIC option on (`0x1d2c8`) the game plays `music%d`: the number comes from the table at `0x1efc`,
  **which is filled at start-up (`0x127b4`) from `CD_TRACKS` in `MENU.DTR`** — eight
  numbers: [0] mode `0x100`, [1] menu, [2..7] the six tracks = **1, 2, 5, 3, 4, 6, 7, 1**.
  So menu = MUSIC2, tracks 1–6 = MUSIC5, 3, 4, 6, 7, 1. (The values sitting in the
  program image, 2, 3, 2, 4, 5, 2, 4, 5, are placeholders; the port used them for one
  build and Danik heard the wrong songs. A table in the data object may be overwritten
  at start-up — look for a `mov ecx, <table>` loader before trusting it.) The same
  number + 1 is the CD track (`0x2636c`); there is no CD audio here. `MUSICn.HMP` = "HMIMIDIP013195": `+0x30` tracks, `+0x38` ticks per
  second (120), tracks from `0x388`, each `[number, length incl. these 12 bytes,
  channel]` + events; a delay is 7 bits a byte, LOW part first, the LAST byte has the
  top bit set; then an ordinary MIDI message (no running status); `FF 2F` ends.
  `MELODIC.BNK` / `DRUM.BNK` are AdLib banks (28-byte header, 128 × 12 name records,
  128 × 30 instruments = two 13-byte operators + two wave shapes); a drum's name
  record byte 2 is the pitch it is played at. The connection byte is the chip's own
  bit (0 = FM: 112 of 128), not AdLib's inverted one.
  **Since 2026-10-11 the music is played as the game played it, by register writes to
  an OPL2 chip: section 32.**  (Until then the synthesiser was the port's own, 18 voices
  with stand-in loudness, stealing and panning: `port/attic/music_own_synth.c`.)
  `OS_NOMUSIC=1`, `OS_MUSIC=n` (pick a song), `OS_MUSICVOL=x` (default 0.6).

I cannot hear: `OS_SNDWAV=<file>` writes the mix of an `OS_DRIVE_DEMO` run to a WAV
instead of opening a device, and the check is numeric (no clipping over a lap; engine
fundamental 66 Hz idling, rising with rpm); `OS_SNDLOG=1` prints every line the
commentator speaks, with the time. `OS_NOSOUND=1` switches sound off.

### 18. MENUS — first piece: track and car selection (2026-10-08)

`port/build/openscreamer` **with no arguments** now opens track selection, then car
selection, then starts the race (it re-runs itself with the track on the command line
and the car in `OS_CAR`; leaving the race with ESC comes back the same way). With
arguments it behaves as before. Picture: `build/png/port_menu_selection.png`.
`OS_MENU_SHOT=screen,pick,file` writes one menu frame without a window (screen 0 =
tracks, 1 = cars).

**How the original's menu works** (`build/decomp/menu.c`, and `0x29ea0` / `0x2a458` /
`0x2df38` / `0x2e08c` / `0x2e384` in `hud_text_sprites.c`): it is ONE 3D SCENE.
`0x27eb4` is the loop, `0x27f9c` one frame (background copy, keys `0x28080`, camera
`0x29530`, 3D render, then the screen's 2D part by the number at `0x203b4`: 0 logos
`0x2df38`, 2 cars `0x2e08c`, 3 tracks `0x2e384`, 4 name entry `0x2e4f8`). The camera
(`0x29530`) eases between two sets of angles and positions with a 0..1 factor at
`0x20378` — that is the fly-through from screen to screen (`0x293e8(n)` starts one).
Menu pages are tables of 16-byte items `[?, id, value, max]` behind the pointers at
`0x2de4`; page number `0x1fb5c`, the selected line of each page at `0x1fb04`; item ids
`0x19`–`0x1f` pick the game mode (`0x1d25c`: 1 normal, 2 championship?, 4, 0x10 network,
0x20 cone carnage, 0x40 slalom), `0x4d2` cycles an option. `0x28ac4` is ENTER. The
menu's words are 3D objects (`0x2b33c`, 80 records of 100 bytes at `0x1db74`; letters
from `ALETTER.3DX`). NOT read yet: the camera's numbers per screen, `0x28080`'s key
handling in full, the item tables themselves.

**The menu pages, decoded** (pointers at `0x2de4`; item = line of `MENU.TXT` counted
without its `*` comment lines, id, value, highest value; an option shows line + value):
page 0 main = NORMAL GAME, CHAMPIONSHIP, SPECIAL MODES, NETWORK GAME, OPTIONS; page 1
options = detail (LOW/MID/HI, default HI), difficulty (ROOKIE/NORMAL/PRO, default
NORMAL), LAP NUMBER (3/5/10/25, **default 3**), SPEECH (default ON), MUSIC (default ON),
VIEW RECORDS; page 2 network; pages 3 and 5 NEW / RESUME; page 4 TIME ATTACK / CONE
CARNAGE / SLALOM; page 6 the six tracks (records). Option items have id 1234 (`0x4d2`).
The port shows pages 0 and 1 as a plain list (`menu_list`, picture
`port_menu_pages.png`); working: NORMAL GAME, OPTIONS → lap number (`OS_LAPS`) and
speech (`OS_NOSPEECH`). Everything else is listed dimmed. `OS_MENU_PAGE=0|1` with
`OS_MENU_SHOT` shoots a page.

**Assets:** `MENUL.FNT` = 4 fonts (11, 9, 19, 30 high); `MENUL.SPR` = 0–3 arrows (two
plain, two lit), 4–8 grip bar, 9–13 acceleration bar, 14–15 SCREAMER logos, 16–28 side
views of the 13 cars; `MENUnL.PCX` backgrounds carry the palette the screen uses;
`CARDAT.TXT` = per car name, gearbox (0 auto / 1 manual), km/h, grip step, accel step,
0; `TRACKDAT.TXT` = 4 lines a track (and the six names backwards for the mirrored
tracks); `MENU.TXT` = every menu word; `MENU.DTR` = menu sounds, CHEATS (TAZOR, VTELO,
MONTY, JOINT, CLOCK, ABURN, INVER, MIRRO, UPDOW), CD track order.

**Wheels the body covers:** AUTO06 (Bullet) has its rear tyre blocks flagged `0x201`
instead of `0x1` — bit `0x200` is otherwise only on body blocks — and its bodywork comes
down over them. The port gives those wheels (tyre and hub) no depth nudge.
(The stray purple pixels once seen round the wheels on the car screen are gone — Danik,
2026-10-09. Closed.)

**A car drawn anywhere needs a `car_pose`** (even an all-zero one): `draw_object` only
applies the wheel / wheel-arch depth nudges to objects that have one. The menu's car had
none at first and the body cut into its wheels again (Danik). `OS_MENU_TURN=<rad>` and
`OS_MENU_ZOOM=<factor>` with `OS_MENU_SHOT` look at a car from any side, close up; F12
in the menu saves `menu_NNN.ppm`.

**Mesh limits (2026-10-08):** the menu's track previews (`AMAPn.3DX`) are the biggest
models in the game — up to 2337 vertices (AMAP5) and 1579 faces (AMAP6). `mesh.h` had
1024 / 1024 and silently dropped whole blocks ("mesh: block N ... array full" on the
console; Danik saw broken previews). Now 2560 / 1792. That message is never harmless.
Picture: `build/png/port_menu_track_previews.png`.

**The two screens' 2D layout is the game's** (see the comment above `menu_frame`).
**Stand-ins:** where the model sits and how it turns; no main menu / options /
championship / fly-through / menu sounds; the mirrored tracks are not offered.

### 19. GAMEPLAY PASS (2026-10-08) — difficulty, opponents off the road, the cameras' sources

- **Difficulty = the letter of the track's data file.** `0x13251`: `MAPn` + `'A' +
  [0x1d2b0]` + `.DTR`. `0x1d2b0` is the OPTIONS line: 0 ROOKIE, **1 NORMAL (default)**,
  2 PRO; MENU.TXT also has AFTERBURNER and DUMMY, which would be D and E, and above 2
  every opponent drives car 12, the Bullet (`0x14373`). On track 1 the opponents' top
  speed is 14000 / 15000 / 16800 and `bonus_time` 50 / 40 / 35. **The port drove file A
  (ROOKIE) until now; its default is B.** `OS_DIFF=0..4`; the menu's line works.
  (`0x1d270 & 8` swaps the data directory for the mirrored track's.)
- **An opponent pushed off the drivable area** at over 3000 units/s is stopped dead
  (`0x1cb9c`) — which is a speed loss of more than 5000 and so a crash (section 15).
  Over a minute on each of the six tracks no opponent crashes by itself.
- **What the cameras follow.** Chase (`0x1a53d`): heading `+0x4cc` WITHOUT the skid
  angle, lean `+0x4dc` and pitch `+0x4d4` — the wheel heights taken a second time with
  the heading alone and no grass jolts (`0x11a30`'s second pass). Bumper cam and cockpit (`0x1a2ac`):
  the car's pitch + the body's (`+0x4e0`), lean + 3 × the body's (`+0x4e8`), height +
  the suspension's drop (`+0x4c0`): the view dips and leans with the suspension.

### 20. THE SPECIAL MODES — cone carnage and slalom (2026-10-08; `build/decomp/modes.c`)

Menu: SPECIAL MODES → CONE CARNAGE / SLALOM (the game's page 4; TIME ATTACK is listed
dimmed). Command line: `OS_MODE=cones|slalom`. Picture: `build/png/port_special_modes.png`.

The game-mode word `0x1d25c`: 1 normal, 2 championship, 4 time attack, `0x10` network,
`0x20` cone carnage, `0x40` slalom, `0x100` (not read; plays song slot 0). Per mode the
game has its own set-up (`0x12964` dispatches: `0x12c08` normal, `0x12e14` time attack,
`0x1311c` cones, `0x1309c` slalom) and its own race frame (`0x20350` cones, `0x203b4`
slalom, `0x202b0` time attack). For cones and slalom:
- **the player is alone** — no opponents are made and only the player is updated;
- **one lap** (`0x2a216` passes lap option 0), the difficulty letter is the OPTIONS one
  (option 3 becomes file E, `0x2a131`); laps for the normal game = table `0x21e0`
  (2, 4, 9, 24) + 1;
- the start place is the LAST OPPONENT's grid slot (`0x13ded`: spline `[0x18ed0] +
  opponents − 1`);
- at the line and at checkpoints the countdown gets TRACK_DATA's fourth number,
  **`carnage`**, instead of `bonus_time` (`0x1962f`) (track 1: see the three files);
  far too little to get round, which is the point: the rest must be earned;
- **cones** (`0x18b50`): props from `MAPnCNG.SPL`; each frame in which a NEW prop went
  down adds 1 s (a prop counts once: its `+0x3c`). PERFECT = all of them;
- **slalom** (`0x18b98`): props from `MAPnSLA.SPL`, gates from **`MAPnSLA.MRK`** (count,
  then 32 bytes: radius 1024, `+0x10` seconds 2 or 4, `+0x14` x, `+0x1c` y — the middle
  of each pair of props). Through a gate for the first time: + its seconds, sample 9
  (`door`). PERFECT = every gate and no prop touched;
- end (`0x23fbc` / `0x23f68`, drawn by `0x24204` / `0x241ac`): `YOU MADE IT!` at y 90 and
  `PERFECT!` at y 120 (GAME.TXT 14, 15); out of time is `TIME OVER` as everywhere;
- commentary masks: hurry only in modes `0x17`, lead / loslea `0x13`, lapped 3; cones
  mode says **cones** once past the line and **all** when every prop is down; slalom
  says **all** when every gate is done with no prop hit.
`OS_MODELOG=1` prints each prop / gate with the time left; `OS_GATEWIDE=1` doubles the
gates for testing — the robot follows the racing line and passes OUTSIDE the gates
(and hits only a couple of cones), so in both modes it runs out of time, as it should.
**TIME ATTACK (mode 4; `attack_*` in main.c, `build/decomp/time_attack.c`).** Menu:
SPECIAL MODES → TIME ATTACK → NEW / RESUME; command line `OS_MODE=attack` (+ `OS_GHOST=1`
= RESUME). One lap, alone, normal countdown (`bonus_time`). The run is recorded, one
40-byte record a frame (`0x231d0`): x, height, z (ints); pitch, heading, roll (floats);
texture-frame word (+0x7b4); reflection picture (+0x7b0); lap progress (+0x680); race
clock (`0x18e0c`). Two recordings, 36-byte headers at `0x18e88` (`[max, write index,
count, active, which of the two, 0x1d270 flags, car, total time (float), pointer]`),
current one `0x18ed0`. Finish (`0x23ef0`): beaten = total time less than the other
recording's (or there is none) → flag 8 → `0x1fd64` writes **`REPLAYS\MAPn.ATK`**
(`0x1b964`: header, then the records), sets the ghost flag `0x1d260` and starts again;
the set-up (`0x12e14`) then loads it (`0x1ba3c`), makes a second car for it
(`0x14d28`, the recorded car model) and records into the other one. Playback
(`0x23018`, pose written by `0x23308`) goes by the clock, a curve through four records
(`0x22724`, ported 2026-10-11 as `ghost_curve`: a cubic from record P1 to P2 with inner
points `P1 + (P2-P0)·(|P2-P1|/3)/|P2-P0|` and `P2 + (P1-P3)·(|P2-P1|/3)/|P1-P3|`; the
angles are NOT run along anything, they are the earlier record's). The ghost is not
collided with. Grid slot = recording index + opponents − 1 (`0x13ded`), so the two
alternate between the last two slots. End text (`0x24174`): `YOU MADE IT!` / `TRY AGAIN`.
RESUME with no file runs without a ghost (`0x2a1df`).
Checked with the robot: a lap is saved (36 + 40 × records bytes), reloaded, and the
ghost stays 0–1 units from an identical run started in the same slot. NOT checked: the
ghost on screen, and whether the DOS game and the port read each other's `.ATK` files
(pitch / roll signs and the +0x7b4 word are the port's own in the file).

### 21. THE CHAMPIONSHIP (2026-10-08; `champ_*` in main.c, `build/decomp/championship.c`)

Menu: CHAMPIONSHIP → NEW / RESUME. Picture: `build/png/port_championship.png`.
`OS_CHAMP_STAGE=n OS_MENU_PAGE=6 OS_MENU_SHOT=0,0,file` shoots a stage's screen.

**A table of 53 STAGES at `0x22b8`** (44 bytes: kind; a number; a number; text block;
next stage; stage on failure; picture name `champN`; ..; race index; font) drives it;
current stage `0x203d8`, the one to return to `0x203dc`. It is perfectly regular
(`champ_stage_at()` rebuilds it): four leagues × 13 stages —
- kind 0, a text screen (`WELCOME TO <league>` before race 1, `PREPARE FOR THE NEXT
  RACE!` before the rest); ENTER (`0x28ef6`) starts the race: **tracks 1..6 in order,
  3 laps, opponents' file by league = A, B, C, E** (bullet league: all Bullets);
- kind 3, the standings (`0x2ce58` draws, `0x292c8` decides);
- kind 1 after the sixth, `CONGRATULATIONS!` (`0x290a5`); stage 51 = won everything
  (kind 2, "NOW THE BULLET IS YOURS", sets `0x207ec` = 1), 52 = `LOSER!!`.
**Rules:** points 9 / 7 / 5 for 1st / 2nd / 3rd (`0x21ec`); to go on you must finish in
the first three (bullet league: win), not run out of time, and after race 6 have 42 /
46 / 50 / 54; `0x29acc` also fails you as soon as the target is out of reach with wins
in what is left. RESUME (`0x28c9e`) goes back to the text screen before the race that
was lost; NEW (`0x28c6b`) = car selection, then stage 0. A race left with ESC records
nothing. Texts = the `#`-separated blocks of `CHAMP.TXT` (0–3 column heads, 4 QUALIFY,
5 TOTAL SCORE, 6–9 league names, 10–13 welcome, 14–16 congratulations, 17 prepare,
18 champion, 19 loser); screen layout numbers are in `champ_draw()`.
**The game's settings file is `CONFIG\CHOICE.DAT`**: `0x460` bytes read straight to
`0x203c0` (`0x1bbb8` load / defaults, `0x1be5c` save) — car, track, player name, options,
championship state (`0x203d8`..`0x20428`), and per track (0x50 bytes from `0x2042c`) five
best laps and (from `0x2060c`) five best totals with 3-letter names; defaults 180 s /
900 s. `0x2d348` draws the VIEW RECORDS page.
**Port:** its own state file `openscreamer.champ` (not the game's); menu and race are
two runs of the program, the result comes back in `OS_CHAMP_RESULT=pos,time,timeover`;
a finished championship race leaves by itself after the end screen's 5 s (ENTER cannot
skip the end screen anywhere: Danik, from the original).
Not done: what winning unlocks (the Bullet, `0x20814` level), records / VIEW RECORDS.

### 22. THE REPLAY AND "PLAY AGAIN?" (2026-10-08; `build/decomp/replay.c`)

**The game records the CONTROLS, not the cars.** `0x22c10` allocates, `0x22f34` writes
one 20-byte record a frame — the frame's two time values (`0x18e00`, `0x18e04`) and six
16-bit control words (`0x1987c`..`0x19890`) — and `0x22e74` reads them back in place of
the keyboard, so a replay is the race RUN AGAIN, everything worked out afresh. The loop
round a race is `0x1fd64`: after a normal race (modes 1 and 2) that was finished — not
time over, not left — the recording is written to `REPLAYS\TEMP.RPL` (`0x1b728`: six
0x1c-byte records from `0x197d4`, 0x28 from `0x192a0`, ten ints from `0x19700`, a
0x18-byte header at `0x18e70` with count / car / laps / difficulty / mode / flags, then
the records) and, in mode 1 only, the race is set up again with `0xcf50` = 1:
`0x22d98` turns playback on, view-table row 3 (trackside cameras) is requested
(`0x12afe`), views cut instead of flying (`0x10a98`), the crash camera is not used
(`0x1857a`). On screen (`0x2346c`): labels, lap, position and countdown stay; the
instruments, clocks and radar are skipped and an **"R"** blinks at (8, 170), font 0
colour 6. When the recording runs out the race is left (`0x22dd8`).
Then, except in the championship and network game, **`PLAY AGAIN?` / `YES` / `NO`**
(`0x219d0`; GAME.TXT 19–21): the picture darkened three times, the texts at y 52 / 92 /
122 in font 0, the chosen one blinking between colours 2 and 14. YES sets the race up
again, NO leaves. The special modes get the question without a replay.
Mode bit `0x80` (`0x18e28`) plays a replay file by name (`0x1b830`) — an attract/demo
mode, not read.
**Also found here: the view a race STARTS in is the option at `0x1d2a4`, default 2 = the
NEAR chase** (`0x12b31`, default set in `0x1bbb8`). The port starts in the far chase.
**Port** (`g_rec`, `dialog_draw`): the controls of every 1/50 s step are kept and the
race re-run from them after 5 s of the end screen; `C` in a replay cycles trackside and
the four driving views, ENTER / ESC skip it; then the question (UP / DOWN, ENTER).
`OS_REPLAYCHECK=1` (with `OS_DRIVE_DEMO`, `OS_LAPS=1`) runs race + replay with the robot
and prints how far the replay strays from the race; `OS_NOREPLAY=1` skips replays.
Not written to a file. In the championship ENTER still just leaves the finished race.

### 23. RECORDS, THE NAME, UNLOCKS (2026-10-09; `build/decomp/records.c`, `build/png/port_records.png`)

**Records** live in `CHOICE.DAT`: per track five best laps (`0x2042c + t·0x50`: 5 floats,
5 four-byte names at +0x14, 5 flags at +0x28) and five best totals (`0x2060c`: the same,
plus the game mode at +0x3c; the flag there is the NUMBER OF A SAVED REPLAY, shown as an
"R" — `REPLAYS\REPLAY<track><n>.RPL`, the renamed `TEMP.RPL`). Defaults 180 s / 900 s,
name `GRA`. `0x2e5e4(track)` enters a result: called for the normal game (`0x2a1a8`, and
`0x1febf` before a PLAY AGAIN restart) and the championship (`0x28f42`) only; nothing when
the race ran out of time (**flag `0x1000` at `0x18e18` = time over**) or was left (`0x20`);
the lap always, the total only when the lap option (`0x1d2ac`) is 2 = THREE laps. A time
goes in ahead of the first entry it is not slower than.
**`0x1d268` is the lap to beat**: set before a race to the track's record (0 in time
attack / cones / slalom, `0x2a264`), and the THIRD CLOCK ON THE RACE SCREEN (text at
`0x1f88`, drawn at (8, 0x58)) is that record, not this race's best lap. A lap under it
(`0x196ea`) replaces it and queues **newlap** (event 7, mode 3, delay 0.2). At the finish
`0x20eb8` writes the race's total and best lap to `0x1d264` / `0x1d268`, so after PLAY
AGAIN → YES the clock shows the last race's best lap.
**VIEW RECORDS** (page 6 = the six tracks, then screen 6, `0x2c844` / `0x2d348`): over the
track's `LOADnL.PCX`, box (0x30, 0x24, 0xe2, 0xa0) darkened twice, track name y 2 font 3,
BEST TOTAL TIME y 0x28, BEST LAP TIME y 0x78 (font 0), rows `"\x01%d. %3s %02d.%02d.%02d"`
from y 0x3c and 0x8c, 10 apart, font 1, colour 0, the chosen total colour 1. UP / DOWN
choose a total (sample 4); ENTER on one with an "R" plays its replay (`0x291f7`, not ported).
**ENTER YOUR NAME is screen 4 and comes before EVERY game** (`0x28b80`…`0x28cba` set the
screen to go on to in `0x1d234`: 3 tracks, 5 championship, 2 cars for network; ENTER on
the tracks goes to the cars, ESC on the tracks back to the name). Drawn by `0x2e4f8` over
`MENU2L.PCX`: title y 2 font 3, the name at (0x83, 0x4c) font 2 fixed pitch, a dash at
x + 21 × position, y 0x58. UP / DOWN move between the letters (sample 5), LEFT / RIGHT
turn the alphabet — ONE alphabet position (`0x1d238`) for all three letters.
**Unlocks:** `0x20814` = level: tracks 0..level+2 can be chosen (`0x287a8`) — a new game
has THREE (Danik confirmed: 3 tracks, 12 cars); a won league sets level to the kind-1
stage's +8 = 1, 2, 3 (`0x290e8`); `0x20818` = the level already shown, NEW TRACK AVAILABLE
(MENU.TXT 53) blinks at y 0x20 until the track screen is left. Winning everything (stage
51, `0x29141`): `0x207ec` = 1 (car 12, the Bullet, can be chosen: `0x28726`), level 9
(the mirrored tracks), difficulty's highest value 3 (AFTERBURNER = file E).
MENU.TXT lines are pointed to from `0x1fb7c + 4·line`.
**Championship correction:** a race that ran out of time or was left DOES get PLAY
AGAIN? (`0x1fe86`: the question is skipped only when flags & 0x1020 is clear).
**ESC in a race = the question, not the menu** (Danik, from the original, 2026-10-09;
same code): in every mode but the network game leaving a race brings PLAY AGAIN? up;
the port now does this (ESC again, or NO, leaves).
**Port:** its own `openscreamer.records` (never CHOICE.DAT); `OS_UNLOCK=1` opens
everything (twelve tracks, thirteen cars) without saving; `OS_RECORDTEST=1` lets the robot's race be entered (delete the
file afterwards); `OS_MENU_PAGE=-2` name, `7` list, `8` + `OS_REC_TRACK=n` a track's
records, for `OS_MENU_SHOT`. Not done: replays kept with records.

### 24. THE REVERSED TRACKS (2026-10-09; `build/png/port_reversed_track.png`)

Tracks 7–12 are tracks 1–6 DRIVEN THE OTHER WAY, not mirrored (their names in
`TRACKDAT.TXT` are the six names spelt backwards). `0x2a264`: a track number over 6 is
reduced by 6 and bit 8 of the option word `0x1d270` is set for the race. That bit does
exactly two things: the data comes from **`DATA\MAPnI.DAT`** instead of `MAPn.DAT`
(`0x13225`, `0x15995`: the string "I" or "" before `.DAT`) — its own `OPP` splines and
grid, `MRK`, `CNG` / `SLA` / `OST`, `SLA.MRK`; the `.DTR` files are the same — and the
road's direction is turned round (`0x12329`: + π on the centre point's angle in
`0x120c0`, which also turns the props). Pictures, walls, records and song are the
ordinary track's. Bits `0x10` / `0x20` of the same word are the picture-flipping cheats
(`0x1075f`, `0x107a1`), bit 4 a clock cheat — not read.
Offered only at level 9, i.e. after winning the whole championship (section 23).
**Port:** a data directory ending in `I` (`build/dec/MAP3I 3`) is the reversed track;
the menu lists twelve tracks when unlocked (preview model `AMAP(n−6)`); the time-attack
ghost of a reversed track is its own file `MAPnI.ATK` (the game keeps one file and the
direction in its header). Robot: reversed laps on tracks 1–4 (51.6, 81.2, 97.7, 106.8 s);
on 5 and 6 the robot does not get round in the time either way.

### 25. CHEATS, MENU SOUNDS, SAVED REPLAYS, SMALL GAPS (2026-10-09; `build/decomp/cheats.c`)

Picture: `build/png/port_cheats_and_record_replay.png`.

**Cheats** (`0x288dc`, run from the menu's key handler; table of 9 × 0x18 bytes at
`0x2e00`: word, pointer, value, flip-or-set; the words come from `CHEATS` in `MENU.DTR`).
Typed letters collect in a 5-letter buffer (`0x20828`, count `0x20878`); when they fit
none of the nine words the buffer is emptied; a full match writes the value (or XORs it)
and plays menu sample 8.

| word | does |
|---|---|
| TAZOR | `0x207ec` = 1: the Bullet can be chosen |
| VTELO | level `0x20814` = 3: the six tracks (it LOWERS a level of 9) |
| INVER | level = 9: the reversed tracks too |
| ABURN | the difficulty line's value = 3 (AFTERBURNER); pointer set at `0x27eab` |
| MONTY / JOINT | option word `0x1d270` = 1 / = 2: the second / third column of `OSTACLES` props (SET, so they clear the three below) |
| CLOCK | `0x1d270 ^= 4`: the countdown is not counted down (`0x18ad0`) |
| MIRRO | `^= 0x10`: the finished 3D picture is swapped left-right (`0x1075f`, before the instruments are drawn) and the steering reversed (`0x1631c`) |
| UPDOW | `^= 0x20`: the picture is swapped top-bottom (`0x107a1`); the cockpit view is skipped (`0x19b9e`) |

`0x1d270` is not in CHOICE.DAT (gone when the game is left); level and Bullet are.
Port: `OS_CHEAT=<word>` carries the option word between menu and race.

**Menu sounds:** in the menu the sample numbers are the nine rows of `MENU.DTR`'s own
`SOUND_FX` (`smenu1..9` = MENU1..9.RAW): 1 line up / down, 5 name position, 4 record row,
6 car / track left / right, 3 an item that starts a game, 0 an item that opens a page,
8 a cheat. (2 for an option and 0 for ESC / ENTER on the selection screens are the port's
reading of `0x28df0` / `0x280ec`.) Menu lists do not wrap (`0x2848e`).

**Opponents never knock props over:** `0x124c0` is called only from the player's three
per-frame lists (`0x15af8`, `0x15b34`, `0x15b70`). Nothing to port.

**A race starts in the near chase** (port now too).

**Replays in files.** Port: `openscreamer.replays/replay<track><n>.rpl` (own format: a
header with track / reversed / car / laps / difficulty / option word / steps, then the
steps), written when a 3-lap total enters the records — n = the lowest number no other
total of that track has, as in `0x2e9bd` — and marked "R" in VIEW RECORDS; ENTER there
runs it (`OS_REPLAYFILE`), then back to the menu. Robot check: 9201 steps, 0.0 units
off. Not compatible with the game's `.RPL` (different physics step). The port's
BACKSPACE reset is now a flag in the recorded step, so replays with it are right.

Robot laps with the countdown stopped (`OS_CHEAT=4`): track 5 173.6 s / reversed 190.6,
track 6 119.2 / reversed 111.4 — all twelve tracks get round.

### 26. THE REAL MENU SCENE, AND THE INTRO (2026-10-09; `build/decomp/menu_scene.c`)

Pictures: `build/png/port_menu_3d_walk.png` (name, tracks, cars, pages, records),
`port_menu_3d_name.png`. Port: `ms_*` in main.c.

**The game's main routine** (`0x10020`): set-up `0x127a0`, then for ever
`0x276a0` (the INTRO) → `0x27eb4` (the menu), until the quit flag `0x7ed4`.

**THE INTRO / ATTRACT SEQUENCE (`0x276a0`) — PORTED 2026-10-09 (`intro_*` in main.c,
`build/png/port_intro.png`; details below this paragraph).** A real race is set up
(`0x128dc`) with no player, on track `0x207f4 + 1` (a counter in CHOICE.DAT that goes
round the six tracks, one further every time the menu is entered), and each frame only
the opponents, the camera, props and moving scenery are run (`0x2780c`). Over it
(`0x279d4`): the logo sprites of `LOGOL.SPR` eased in (`0x279fc`: sprites 0, 1, then 2)
and the CREDITS, the `#`-separated blocks of `LOCAL.DAT`'s `ATTR.TXT` ("… PRESENTS",
"A … PRODUCTION", "COPYRIGHT 1995", DIRECTED BY ANTONIO FARINA, …), one block at a time
for 4 s (`0xed6`), each line sliding in (`0x27b64`, font 0 colour 2, 20 rows apart).
A key starts a fade of picture and sound (`0x27d28`), then the menu. The menu's first
page, left alone for **30 s** (`0xedc`, timer `0x1d248`), goes back to it (`0x27f9c`).

**The intro in detail (`build/decomp/demo_camera.c`).** Mode word `0x100`; set-up
`0x12f4c` (flags `0x2001`: started at once, flag `0x2000` = attract), difficulty NORMAL,
option word 0, sprites from **`LOGOL.SPR`** and the font from **`ATTRL.FNT`** (`attr%c.fnt`;
with GAMEL.FNT the lines overlap), song slot 0.
- Camera = view-table row 4, routine 8 (`0x1aeb4`): every opponent has a camera of its
  own (`0x1a920`, record +0x4f8): the node of **`MAPnCAM.SPL`** nearest the car, turned to
  the car; `|node height| % 10` is a code — bit 1 close in to 1500 units always, bit 2
  while the car goes away, bit 4 while it comes. The one shown is car `0x10a14`; every
  1000 timer ticks (`0x27e64`; taken as 10 s — the tick is the one the menu's blinks use)
  `0x19d82` lines up the next car that is at ANOTHER node, and it takes over when the
  shown car changes node (`0x1aef3`).
- Logos (`0x279fc`): block 0 sprite 0 at y 4, block 1 sprite 1 at y 28, from block 2
  sprite 2 at y 4; x = centred + an offset eased (`0x22b60`) from −320 to 0 (stiffness
  `0x50`; sprite 2 `0x5a`) and back out to −320 while the block leaves.
- Credits (`0x27b64`): line i of the block at y = 20·i, x = centred + an integer offset
  eased from 350 to 0 (`0x22b94`, stiffness `0x5a`); after 4 s the target is −400 and when
  the offsets pass −360 the next block starts. After the last block the state is 3,
  which has the "leave" bit: fade (1 s, picture and sound volumes) and on to the menu.
- Keys (`0x211d0`): ESC, ENTER, SPACE or a joystick button. **Same routine: `M` steps the
  RADAR's zoom** (`0x1d2a0`: 1, 2, 3 = 24, 48, 72, then 0 = off; saved in CHOICE.DAT,
  default 1 = 24 — the port had 48 and no key) and `P` opens the CD-player screen.
**Port:** the intro is a third kind of run of the program (`OS_INTRO=1` + a track): the
player's car is moved out of the world and only opponents, props, scenery and the
credits are stepped. `openscreamer` with no arguments runs it first (`OS_INTRO_DONE` in
the environment afterwards; `OS_NOINTRO=1` skips it), the menu's pages left alone for
30 s go back to it. The track counter is the fifth number of `openscreamer.records`'s
first line and goes on one per intro (the game: one per visit to the menu).
`OS_INTRO_TEST=1` saves eight frames `intro_N.ppm`, 2.5 s apart, and stops;
`OS_INTRO_LOG=1` prints every change of car, post and close-in.
**After Danik compared it with DOS (2026-10-09):** (1) the original HAS the cars' engines
— `sound_world()` (scenery + opponents' engines, `0x1dffc`) is now heard from the camera;
(2) the picture FADES IN and the cars go the moment it is fully there, no start-light
wait — the port fades in over 1 s (the length is the fade-out's; not read) and starts
them then; (3) flicker before a change of shot: the camera was worked out once a PICTURE,
and two pictures inside one 1/50 s step saw the same distance twice = "coming" and
"going" at once, flicking the close-in; it is worked out once a step now, and at a new
post the comparison starts from where the car was a step before.
Since 2026-10-11 every car has its own camera and the fly-across when a post's close-in
code switches is in (section 30); the sound fades with the picture.

**THE MENU IS ONE 3D SCENE** over a still picture (`0x27f9c`: picture copied, keys
`0x28080`, camera `0x29530`, per-screen routine `0x29484`, 3D render, 2D by `0x203b4`).
Screens (`0x1fac0`): 0 the pages, 1 changing, 2 cars, 3 tracks, 4 name, 5 championship,
6 records, 8 network. Pictures: pages `MENU1L`, **cars `MENU4L`**, tracks `MENU3L`,
name `MENU2L` (the port had the cars on MENU2L).
- **Letters are CUBES** (`ALETTER.3DX`, 516 units, 80 objects, records of 100 bytes at
  `0x1db74`). The cube's texture (MENU.MLB material 1, 24×48) has **76 frames**, one per
  character: object `+0x20` = `0x35e × (char − 32) + 0x226`. Front face = the frame's
  top half (bright), back = the bottom half (dim). (`MAT_FRAMES` was 64: blank cubes.)
- **A page** (`0x29838`, `0x2af30`): line i of n on row `8 − n + 2i`; letter k at
  x = `0x280·(len−1)/2 − 0x280·k` (+x is screen LEFT), y = `0x820 − 0x180·row`, z = 0;
  camera at (0, 0, 4100) looking along −z, lens 128, centre row 100. Per frame
  (`0x2b8b8`): the chosen line's cubes turn to pitch 0, the others to π, at 4π rad/s;
  every cube ripples in z by `150·sin(2π·fx + π·fy + t·k)` (t += 9.42/s, wraps at 8π;
  chosen line fx = 0.5, k = 0.25). Entering the screen the letters FLY IN: progress p
  1→0 in a second, position = target + (1 − cos(pπ/2))·(start − target) + a swing
  `amp·p·(cos, sin)(2πp)`; start and swing from one of eight rows of the table at
  `0x2218`, taken in turn, the start turned 0.09 rad per letter. Turning to another page
  or changing an option: all cubes turn away, and past a quarter turn the new page is
  simply there (`0x2b380`), its chosen line finishing the turn.
- **Cars / tracks** (`0x2bdcc`, `0x2c078`, `0x2ca10`, `0x2cb88`): ALL of them stand in a
  row, cars 4000 apart, tracks 40000, at height 0, z = 2900, turning at π/2 and π/4
  rad/s. Camera: cars (−4000·n, 500, 4100) pitch −22.5°; tracks (−40000·n, 4800, 13000)
  pitch −36°. LEFT / RIGHT gives the camera a new target and it slides (factor 1→0 at
  0.95/s, position by factor⁴); the next press is taken only under 0.8.
- **The name** (`0x2c320`, `0x2cd74`, `0x2b54c`): 26 cubes in a ring of radius 3500, one
  every 2π/26, each dropping from 2560 (odd ones 3060) to 160 with bounces of 0.4
  (fall 2.5/s, rise 2/s, squared), 0.04 s after its neighbour; the chosen letter's cube
  tilts to −36° (front to the camera), the rest to 126°. The camera walks round outside
  at radius 4500, height 1800, pitch −36°, in 1/8 s per letter (linear), next press
  under 0.1. THAT is why one alphabet position serves all three letters.
- **Changing screen** (`0x29738`, `0x27de8`): the palette fades to black at 1.5/s (2/s
  from the pages), the scene standing still, then the new screen fades in (1.5/s; the
  pages 0.6/s). Leaving the pages (`0x293e8`) the camera also pulls back to z = 20000
  (factor at 0.6/s) with its roll set to −π and turning π rad/s.
- The two logos slide in from the left at 350 and 700 px/s (`0x2df38`); NETWORK GAME
  shows NETWORK NOT FOUND (MENU.TXT 55) blinking at y 190.
**Not checked by eye:** the fades and the pull-back have not been seen by
me (stills only). `OS_MENU_KEYS="ESRS…"` presses menu keys by itself (U D L R, E enter,
X escape, S save `menu_script_N.ppm`), `OS_MENU_STEPS=n` runs n frames of 0.04 s before
an `OS_MENU_SHOT`. **The name screen WRITES the name: back up `openscreamer.records`
before a scripted run.**

### 27. HIGH RESOLUTION AND THE SETTINGS FILE (2026-10-09; `build/png/port_hires.png`)

**The game's hi-res set is 640×400, not 640×480**, and exactly twice the low set:
every `…H.PCX` is 640×400, every sprite of `GAMEH.SPR` / `MENUH.SPR` / `LOGOH.SPR` twice
its L twin (give or take a pixel), font heights 60/54/18/12/36/14/54 against
30/27/10/6/18/7/27. `MAPnH`: `.MLB`, `.COL`, `.SMG`, `.BCK` (still 512×256) and the cars
are byte-identical to `MAPnL`; **`.UNR` and `.MMP` are bigger — the hi-res tracks have
more scenery** (track 1: 798 K / 131 K against 662 K / 106 K). `CARS.DTR`'s `COCKnnH`
sections: needle length and pivots are the SAME numbers as `COCKnnL` (320×200 units),
only the gear-lamp positions are doubled (pixels). STARTL.EXE itself only ever asks for
the L files (`push 0x4c`); the H layout numbers would be in STARTH.EXE, not read.
**Port:** OPTIONS has a line of its own, LOW RESOLUTION / HIGH RESOLUTION, under the
game's DETAIL line (section 30). `FB_W` / `FB_H` are variables (320×200 or 640×400,
both shown 4:3), `g_s` = 1 or 2; the lens is `128 × g_s`; everything flat keeps its
320×200 positions and is multiplied by `g_s` inside `hud_text`, `hud_sprite_draw`,
`dash_needle`, the radar, `champ_darken` (`hud_sprite_px` for the lamps, whose H
positions are pixels); `res_name()` turns the L before a file's extension into H;
the backdrop is drawn doubled. A font's `height` is in 320×200 rows, `rows` its own.
So the hi-res LAYOUT is the low one doubled — not read from the hi-res program.
**`openscreamer.cfg`** (text): `resolution=low|high`, `difficulty`, `laps`, `speech`,
`music` — the options are remembered now (they were not). Changing the resolution line
saves and restarts the menu in it. **Fall-back to low** (`res_init`, `res_fail`): the H
files missing; no memory / no window; or `starting_high=1` still in the file — written
before a start in high, removed at the first picture or an orderly exit, so it survives
only a crash. Each puts `resolution=low` back and starts the program again.
`OS_RES=high|low` overrides the file for one run without touching it (tests);
`OS_RES_TESTFAIL=1` pretends the files are missing. Also fixed here: `sel[7]` in the
menu was indexed with page 7 (now 9 long).
**Speed (2026-10-09; Danik: 180 fps low, 59–70 high on a 5800X).** Profile of a hi-res
race frame, 14.8 ms: 45 % was `hud_radar` searching the palette for EVERY pixel of its
window (now remembered per colour), 41 % `raster_tri` testing every pixel of each
triangle's bounding box and rebuilding 1/z, u/z, v/z from the weights (now the inside
stretch of each row is worked out and the three are stepped; the row's two ends are
settled with the old test, so coverage is the same — 0.1–0.4 % of pixels differ, in
depth ties and texel rounding). After: 4.8 ms hi-res, 1.9 ms low (profiling build).
**Bug fixed with it:** `starting_high=1` outlived the process when it handed over with
`execl` (menu → intro / race), so the next process took it for a crash and fell back to
low: the hi-res setting was never kept. `res_started()` is called before every hand-over.
The MiSTer target stays 320×200.

### 28. THE CD PLAYER, AND THE PASS OVER THE UNREAD GAME LOGIC (2026-10-09)

**The CD player** (`0x21328`, P in a race; `cd_*` in main.c; `build/png/port_cd_player.png`).
Over the stopped race, darkened twice: `CDFRONTL.PKC` (a car stereo), the disc's track
numbers (font 5 colour 9, from (64, 78), 13 apart, the chosen one blinking), a cursor
sprite on one of seven buttons along y 97 (table `0x1f1c`: x, sprite, pressed sprite —
59/7/11 play, 83/8/12 eject, 113/9/13 previous, 137/10/14 next, 167/57/59 volume −,
167/58/60 volume +, 217/61/62 which volume; **GAMEL.SPR sprites 7–14 are these buttons,
the gear lamps start at 16**). For the last three the volume is shown (`0x23dac`): its
name `CD` / `FX` / `MU` at (64, 69) and value/1024 strokes 3 apart from x 85, colours 22,
214, 208 by tens (`0x1fa0`). The three are the master volumes `0x1d2bc` (CD audio),
`0x1d2b8` (digital), `0x1d2c0` (MIDI), start `0x6000`, saved. While the screen is up they
fade to 0 at 16384/s and the music stops; leaving restores them and plays the chosen
track. Play leaves; eject runs the disc-change texts (GAME.TXT 16–18); ESC leaves.
Port: the seven songs stand for the disc's tracks, CD and MU both act on the music,
eject does nothing, one press of − / + = one stroke; the volumes are in
`openscreamer.cfg` (`volume_cd` / `_fx` / `_mu`) and `snd_masters()` applies them.
`OS_CDTEST=n` opens it with the cursor on button n in a robot run.

**The pass.** Of the game-logic range (`0x10000`–`0x30000`) 115 functions (19 KB) had
never been looked at; all are in `build/decomp/remaining_logic.c` now. What they are:
the object table (create / clone / show / hide: `0x10ed4`–`0x11a08`), the memory pool and
the music driver's glue (`0x2eb2f`–`0x2fd44`), loaders (`0x1545c` .DTR, `0x1b384` text,
`0x1b594` .PCX, `0x1f2d0` sounds), spline / angle / vector helpers (`0x1e660`…`0x22644`,
`0x1c538` = two segments' crossing), the commentary queue's helpers (`0x248b8`…), the
CD's disc check (`0x157b4`) and swap screen (`0x15870`), and the NETWORK game (`0x12424`,
`0x12d08`, `0x1496c`, `0x1ead0`–`0x1ef58`: eight remote players of 0x2a8 bytes at
`0x13cd4`). **Three things the port did not have came out of it:**
- **Every race fades in and out** (`0x21e50`, `0x21d8c`): picture AND the three volumes
  from nothing to full in half a second at each set-up (first start, PLAY AGAIN, the
  replay, the intro), and down again over half a second when the race is left. Ported
  (`g_fade_in`; the intro's guessed 1 s is 0.5 s now).
- **Who listens** (`0x212c0`, called from the camera update): the PLAYER'S CAR, place and
  heading, while a race runs (flag `0x100`, set at set-up as `0x102`) — the camera only
  in the intro (flag `0x2000`). The port had "the camera, always" for a day (section 17);
  corrected: `g_ear` / `g_ear_yaw`.
- **The timer tick is 1/100 s** (`0x21d8c`: ticks × 0.01), which settles the intro's
  change of car every 1000 ticks = 10 s.
Nothing else single-player turned up. Still unread anywhere: inside the network code,
and the 3D engine / drivers that the port replaces.

### 29. ONE PROGRAM, ITS DIRECTORIES, DEVELOPER MODE (2026-10-09; `port/src/port.c`, `port.h`)

**One process, one window.** The menu, a race and the intro used to be separate runs of
the program (`execl` of itself), each wanting everything as the program first finds
it; the window closed and opened between them. Now `main()` is a loop over
`session()` (the old `main`): before the first part the program's own writable statics
are remembered (`os_state_save`: a copy of `.data`, and the few non-zero words of
`.bss` — the C library's `stdout` / `stderr` copies and `port.c`'s own pointer), and
between parts they are put back (`os_state_restore`: also frees every block the port
allocated — `malloc` / `calloc` / `free` are `os_*` through `port.h`, on a list).
What outlives that is one heap block (`keep` in port.c): directories, arguments, the
`-d` switch, the window / renderer / texture (`os_slot`), "done once" marks (`os_once`:
`atexit`, the help text). A part hands over with `os_next(argc, a1, a2)` + return, or
from deep inside with `os_go()` (a `longjmp` to the loop). The environment variables
that carried state between the runs still do: they belong to the process.
Six menu → race → menu rounds: memory steady at 90 MB (84 MB is the statics).
**Needs:** GNU ld's `__data_start` / `_edata` / `__bss_start` / `_end`, and SDL linked
DYNAMICALLY — a statically linked SDL would have its statics inside the range that is
put back. `.bss` is 84 MB because the meshes are static arrays; zeroing it takes a few
milliseconds per change of part.
**Directories** (`os_path()` rewrites the names the code has always used):
`build/dec/…` → the DATA directory: `-data DIR`, `$OPENSCREAMER_DATA`, else the first
that exists of `./build/dec`, `<program dir>/data`, `<program dir>/../../build/dec`,
`./data`. `openscreamer.*` → the HOME directory: `-home DIR`, `$OPENSCREAMER_HOME`, else
the current directory when it already holds the port's files, else
`~/.config/openscreamer`. `REPLAYS/…` (time-attack ghosts, which the port used to put
in the GAME's directory) → `<home>/openscreamer.ghosts/`.
**Keys.** `F1`–`F4` the four views again, `F5` FPS counter, `F11`, `F12`.
(The `O` / `SHIFT+O` drawing-distance keys and `draw_distance=` are gone since 2026-10-11:
ULTRA is a fixed 140000.) **`-d` = developer mode**: only then TAB / 1–4
(DRIVE, RIDE, WALK, FLY), mouse look, `[` `]` HOME, V, I, G, K, L, N, SPACE. The
`OS_*` environment switches are unchanged (they are for tests, not keys).
`OS_CYCLETEST=n` + `OS_MENU_KEYS="EEEE"`: n rounds of menu → race → menu, printing the
memory held.


### 30. THE 2026-10-11 AUDIT -- what was still the port's own, and what replaced it

Who wrote what: the whole of 2026-10-07 was a DEEPSEEK session (3118 messages; Claude
from 2026-10-08).  Its unpacker and extractor were re-verified independently: the flat
image matches the EXE's own page table on all 227 pages, and a strict second LZW decoder
agrees with every extracted file (428 streams, each ending on its END code at exactly the
declared length).  Its "zero mismatches" test had proved less than it said (the decoder
stopped at the expected size).  `unle.py`'s header still says "no relocation needed":
wrong, and only text.

- **DETAIL (OPTIONS' first line, `0x1d2b4`) = HOW FAR THE GAME DRAWS.**  `0x8baa4` writes a
  far-plane depth into seven compare instructions of the face routine (`0x839af`…: a face
  wholly beyond is dropped, one across it is clipped); the depth is the table at `0x21d4`:
  **LOW 27808, MID 27808, HI 43808** (the menu uses 43808).  LOW also loads NO moving
  scenery (`0x15374`: `0 < detail`).  Pieces are still only kept within 52000 units.
  Port: `g_far_z`, `g_detail`; a fourth level **ULTRA** (the port's own: no far plane,
  pieces to a fixed 140000 units -- no key, no setting); `detail=low|mid|hi|ultra` in the
  settings file, default HI as the game's; `OS_DETAIL=0..3`.  The OPTIONS page has seven
  lines now (the game's six rows squeezed to fit) and 96 letter cubes (the game: 80).
- **What is under a point** (`track_under()`): the game stops at the FIRST face it finds
  -- nine cells in the order of the table at `0x1cec` (own, +x, −x, +z, −z, corners),
  run-0 records in stored order, the piece's FIRST block only, its bounding circle
  (header +0 radius, int16 centre at +4 / +8, not turned with the piece), three tries
  (as is, +10, +20 in the piece's frame), faces in the order of the list at header
  **+0x28** (NOT storage order: `mesh.coll[]`), height = corner − (dx·Nx + dz·Nz)/Ny in
  whole numbers.  A wheel over nothing keeps its height.  `OS_STACKED=1` looks for
  drivable faces one over another: none on any track (largest difference 32 units), so
  "first" and "nearest in height" never disagree by more than a seam.
  `mesh.c` reads a block's faces through the header's EIGHT row offsets (+0x28..+0x44, one
  draw-order row per octant, each ended by 0xFFFF and leaving out the faces unseen from
  that side): `block_faces()`.  The first-day scan (`find_faces`) is gone; over all 5174
  blocks / 83900 faces the two gave the same records.  `coll[]` is row 0, as the game's
  ground test reads it.
- **The running order** (`0x20418`): every car has a place (player last, others in grid
  order: `0x12c24`, `0x1491b`).  Progress = laps × 100 + marker last reached + distance
  from it / distance to the next (`0x1820c`, `0x1db84`; `marker_progress`).  Once the
  player has crossed the line, each frame the first opponent on the wrong side of the
  player swaps places with it; opponents never swap among themselves.  The opponents'
  catch-up (`0x1ced0`) and "lapped" go by the same numbers.  Time attack: 2nd while the
  ghost is further on.
- **The skid is LOP-SIDED in the game** (`0x17d04`, checked instruction by instruction):
  the cornering force and the turning radius are signed; the spin test compares the
  force itself with the limit (`0x17efa`), and the running-wide speed `surface × 0.2 ×
  speed² / radius` is wiped when under 10 (`0x1809b`), so a car runs wide or spins from
  cornering only when turning the way that makes the heading grow -- in the port's axes a
  LEFT turn.  Turning right the body swings out and that is all.  The port had "always
  outward" both ways.  Also new: each wheel on grass adds 1000 (front) / 600 (rear) to
  the force (wheel record +0x2c).  **Danik to judge against the original.**
- **Smoke** faces the screen by the game's own means: `0x118a4` sets bit 0x8000 of the
  puff's object record (+4), and the engine (`0x8abc7` → `0x8b26c` → `0x4bca0`) turns such
  an object by its angles LESS the camera's.  `scene_object.sprite`.  Block flag 0x20 is
  read nowhere.
- **Props** stand on `track_under()`'s surface, TILTED to it (`0x15240`: the face's normal
  turned by heading − 90°, × π/46080 -- the opponents' rule, `slope_tilt()`); a prop over
  no track gets the previous prop's height; in flight the ground is that surface, and at
  rest it lies on its side along the slope (`0x20880`).
- **The intro's cameras** (`0x1a920`, per car; `intro_cam_car`): at a new post the
  comparison starts from the distance itself, so the first picture from a post with code
  2 or 4 is the close one; when close-in then changes at the same post the camera FLIES
  (view-table row 4 asked for as "2" → `0x19ad8`'s fly path → `0x19ef0`).  The next car
  is searched on from the last one lined up (`0x19da3`).
- **Settled:** the menu's view centre row is 100 (`mov ebx,0x64` survives both calls
  before `0x4c610`); the three commentary inputs (section 17).
- Housekeeping: the `OS_OLDGROUND` code is gone, `spl.c` is in the attic.
- **Two objects in one place** (Danik: striped flicker on Sandrock's hotel by the start,
  `build/png/port_sandrock_hotel_ab.png`).  Track 6's hi-res `V363` and `V366` are two
  pieces of the same outline at one spot, 10591 units apart in height, sharing their walls
  for 4400 units.  The engine paints whole objects in order of the distance from the camera
  to each object's own position (`0x83164` builds that list: `dist² >> 2`, record), so one
  simply covers the other; a depth buffer makes the walls fight.  Port (`raster_tri`): a
  pixel within one part in 10000 of the depth already there, from ANOTHER object, goes to
  the object whose own position is nearer the camera (`g_obj_d2[]`).  About 100 pixels a
  picture change elsewhere (seams).  Do not "fix" such overlaps in the data.
- `openscreamer --help` (port.c `os_help`): usage, options, keys, files, the everyday switches.
- **The lawn under the road** (Danik: a wedge of grass over road and rail in Lake Valley's
  chevron bend, `build/png/port_lake_valley_carpet_ab.png`).  Every track has ground-fill
  objects named `akk…` (tap = carpet, acq = water, mare, spiag, dir, tr, sper…), 80-250
  placements a track, in both resolutions.  The `akk…tap…` ones are big FLAT grass tiles
  laid right under road pieces, sometimes higher than a road that dips (tile at 875 across
  road piece V34).  The engine paints them before the road; a depth buffer shows the tile.
  Port (`raster_tri`, `mesh.carpet`; the `tap` tiles of tracks 1-4 and `akksper` on
  Sunbeach Hill -- Danik's second screenshot, grass on the tarmac at the lighthouse bend):
  against anything that is not a carpet, a carpet
  counts as TWICE as far away -- for a flat tile that is exactly "whatever lies less than
  the camera's height above the tile below it shows through".  NOT the other `akk…`
  objects: `acq` / `mare` are water, `rai` the tram rails laid ON the road, most others
  are not flat -- they are left to the depth buffer.  `OS_NOTIE=1` switches this and the
  same-depth rule off for comparing; `OS_INSPECT=x,y` names the piece at a pixel.
  Checked over 120 frames (20 a track): one frame changes by 857 pixels (Sunbeach Hill,
  where an `akksper` tile used to speckle through the pavement of road piece V29 -- now
  clean), the rest by under 250.
- Sound starts SILENT (`sound_load`): the song used to play at full volume for the moment
  the rest of the loading took, before the fade-in pulled it down (Danik).

**What is still the port's own, by necessity:** the renderer (depth buffer instead of the
engine's sort), the SDL mixer / pan law, the chip's loudness against the effects, the fixed 50 Hz step and the
30 frames/s taken for per-frame quantities, the save formats, the CD's stand-in songs.
**By decision:** heading snap, the wall put-back, ULTRA, the hi-res layout, BACKSPACE,
developer mode, the one-second hold of the loading screen, the game controller's layout.

- **Smoke is not cut off by the road** (Danik, from the original; `port_smoke_uncut.png`,
  with / without).  A puff is a rectangle 568 high whose MIDDLE is at the car's own height
  (`0x19970`: the slot's offset, turned by heading + slip and tilted by the car's pitch,
  its height zeroed, plus the car's place), so its lower half is under the road; the engine
  paints the puff over the road and all of it shows.  Port (`raster_tri`, `g_obj_sprite`,
  `g_obj_ground`): a screen-facing object is never hidden by a run-0 (drivable) piece;
  walls, scenery and cars still hide it.  **Where a puff starts is not random**
  (`0x198b7`): outwards by `(x & 7) x 51` and back by `(z & 7) x 51` of the car's own
  whole-number place -- 51 being the low six bits of the word at `0x7fd`, the middle of a
  floating-point constant (a slip of the makers').  The port had `rand()`.
  Danik also finds the original's smoke "a bit darker": NOT explained.  The texture's
  strengths are 1..19 = SMG's brightening rows, the routine is `0xcd9dd` as ported, eight
  puffs overlap in both.  Not ported: the puff offset's tilt by the car's pitch.

- **The effects go through a sound card's output filter** (Danik: every effect brighter and
  thinner than the original's, the aeroplane "like a phone speaker").  Checked first: the
  pitch is right -- HMI's sample block (template at `0x1e88`, 0x74 bytes a sample at
  `0x16030`) has no rate of its own, +0x38 is the pitch, 0x10000 = as recorded, and
  `0x1f7d4` treats 0x40000 as the top; the driver runs at 11025 Hz; the engine's constants
  are 14 and 32768.  So the difference is tone.  Holding each 11025 Hz value for two output
  frames (the 2026-10-09 fix for "muffled") lets the mirror image above 5.5 kHz out almost
  as loud as the sound; no card did.  `sound.c` now puts the effects (not the FM music)
  through a two-pole low-pass at 3.2 kHz, the Sound Blaster Pro's: a robot lap's mix loses
  20 dB above 5.5 kHz and 4 dB from 2.5 to 5.5 kHz, nothing below.  `OS_SFXLP=<Hz>` moves
  the cut, 0 = off.  **Danik's original runs in DOSBox** (not on this machine; which
  DOSBox and which `sbtype` not known).  DOSBox brings an 11025 Hz Sound Blaster stream up
  to the output rate by sliding from one value to the next, which is itself a low-pass:
  about -2 dB at 3.5 kHz, -6 at 5.5, -15 at 8 -- close to the filter here (-3 at 3.2,
  -10 at 5.5, -16 at 8), so the cut was left alone.  A recording from DOSBox (Ctrl+F6)
  would show whether anything else differs.

- **THE MIXER IS IN `HMIDRV.386`, AND IT IS READ** (2026-10-11; Danik: the original's engine
  has another timbre, "as if the formants sat lower", the plane more body; he put the three
  `HMI*.386` files back into `screamer/` for this).  STARTL.EXE only fills HMI "start
  sample" blocks (0x74 bytes; +0x14 volume, +0x28 flags, +0x30 / +0x34 loop, +0x38 pitch,
  +0x40 pan; flags 0x4000 looping, 0x400 pitch shift -- the DTR pitch column's top digit,
  set for the engines and all scenery sounds -- 0x200 pan, 0x100 volume, 0x40 loop points)
  and stores a new pitch in the driver's voice (`0xc6846`, voice +0x44).
  `HMIDRV.386` = about a hundred modules, each `name.com` + 0x30 bytes of header then
  32-bit code: one small module per sound card (DMA / port work, a capability
  header whose +0x68 names its mixer) and the MIXERS `tr8m` 0x1000, `tr8st` 0x1001,
  `tr16m` 0x1002, `tr16st` 0x1003 (the Sound Blasters use these four), `tr8mu`, `tr8mx`...
  A mixer keeps the 32 voices (0x6c bytes each, at its own 0x398) and, on a timer, BUILDS
  each voice's inner loop from numbered code pieces (table at 0x15, 10 bytes a piece;
  builder `0x1ca6` / `0x1e6b`).  For an 8-bit sample:
      al = data[ecx];  bp += pitch low word;  ecx += pitch high word + carry     (nearest sample)
      al ^= 0x80;  8-bit card: al = al x (volume >> 8) >> 7;  16-bit: ax = (al << 8) x volume x 2 >> 16
      added into the DMA buffer, held at the limits (8-bit: +-127; 16-bit: +-32767)
  volume = voice x master x 2 >> 16; pan (+0x54, moved by +0x56 a tick): under 0x8000 one
  side is full and the other is `pan`, over it the other way round -- a straight-line
  fade, the middle full on both.  **So: 0x10000 IS "as recorded"; there is no filter and
  no smoothing anywhere; the port's mixer is the driver's.**  Two things the port does not
  do: (a) on an 8-bit card each voice is cut to 8 bits AFTER its volume (a quiet voice
  keeps few steps); (b) the mixer writes AHEAD of the card's play position: a voice starts
  0x100 bytes ahead and each tick mixes 1/8 more than was played, until it is nearly a
  whole DMA buffer (0x2000 bytes: 0.74 s on an 8-bit mono card, 0.19 s at 16-bit stereo)
  ahead -- so a change of pitch or loudness is heard that much late.  Which card Danik's
  SETUP chose is not known.  **The timbre difference is therefore NOT explained by the
  game or its driver**; what is left is DOSBox's Sound Blaster and the port's numbers
  (revs), and only a recording from DOSBox can tell.

### 31. THE LOADING SCREEN AND THE CONTROLS (2026-10-11; `port/src/controls.c`, `loading_show()`)

Picture: `build/png/port_loading.png` (low, high).

**Loading screen** (`0x2a264`, the one routine every race is started through -- menu game,
championship race, a record's replay): screen cleared, `load<track><L|H>.pcx` put up with
its own palette, turned over when the picture cheats are on, `LOADING` (MENU.TXT line 47)
centred at row 0x50 in the menu's font 3, colour 0.  No fade: the race fades in from it.
A reversed track shows the ordinary track's picture.  Port: drawn in `session()` between
the menu and the hand-over; **held one second, the port's own** (setting a race up takes
0.1 s here; `OS_LOADHOLD=<s>`); `OS_LOAD_SHOT=<file>` saves it.  Not shown for a race
started from the command line, nor for PLAY AGAIN (the game does not either).

**Controls.**  `CONFIG\CONTROLS.CFG` (`0x1b2f4`; 1328 bytes) = six 0x1c-byte records
(accelerate, brake, left, right, gear up, gear down) + 0x28 joystick-calibration records
(per stick and axis: centre, divisor, two float scales) + ten ints (game-port numbers).
A record, seven ints: `+0` in use; `+4` 1 = a joystick AXIS (else button); `+8` kind
(1 key, 0 joystick as it reads, 2 joystick as a switch); `+0xc` axis; `+0x10` stick;
**`+0x14` the side that counts, +1 or -1** (a control fires only when its word has that
sign: one axis, two controls); `+0x18` the key's PC scan code or the button's bit.
Shipped: Up, Down, Left, Right, Alt, Ctrl.  `DEFCTRL.CFG` differs only in the joystick
records' first int.  SETUP.EXE wrote it; the game has no screen for it.
`0x24d40` fills the six words `0x1987c`.. once a frame: key down = 0x3ff; axis =
`(raw / divisor - centre) x scale`, held to +-0x3ff, **nothing under 0x66**; kind 2 snaps
over 0x1ff to full.  How the driving code takes them:
- accelerate (`0x1611c`): throttle = |word| / 1023, key or axis alike;
- brake (`0x16174`): an axis IS the brake (|word| / 1023); a key or button eases it to
  full (stiffness 0x52);
- steering (`0x1633b`), by the LEFT record's kind: keys -- target (left - right) / 1024
  of the lock, eased; axis -- the one signed word / 1024 of the lock, **taken as it is**
  while the stiffness is the normal 0x50 (not sliding, not on the start line), eased
  otherwise;
- gears (`0x18e0a`): one a press (latch at player `+0x140`).
**Port** (`controls.c`): keys from the game's file (a PC scan code's twin works too: both
Ctrls, both Alts, arrows = number pad), or `key_accelerate=` / `key_brake=` / `key_left=`
/ `key_right=` / `key_gear_up=` / `key_gear_down=` lines in `openscreamer.cfg` with SDL's
key names (`key_brake=Space`); A / Z stay gear keys when nobody else has them.
**Game controller: the port's own layout** (the file's joystick records are a DOS
game-port calibration): left stick = steering axis, triggers = accelerate / brake axes,
A / B and d-pad = switches, shoulders = gears, Y = view, START = ENTER, BACK = ESC; in
menus (and the question, the replay, the end screen, the CD player) d-pad / stick =
arrows, A = ENTER, B = ESC (`controls_menu_mode`).  Every `SDL_PollEvent` in main.c is
`controls_poll` (a macro), which turns those buttons into key events.  A step's
"stick / trigger" bits ride in the replay step's `reset` word (bits 1, 2), so old replay
files still read.  `car_state.analog` carries them to `drive.c`.
`OS_PADTEST=stick,gas,brake` (SDL raw, +-32767) pretends a controller and switches the
robot off: `OS_PADTEST=11000,32767,0` gives 6.0 deg of lock at rest (a third of 18).
**Not tested with a real controller.**

**Revving on the grid** (Danik, from the original; `0x18df0` -> `0x193e7`).  Until the
green light (flag 1 at `0x18e18`) the engine routine skips gearbox and drive and only
eases the revs to `throttle x maxrpm` (stiffness 0x4b, the neutral path); the pedal
routine runs as always.  So the pedal revs the engine, and an automatic car held over
8500 rpm at the green takes first with its wheels spinning (state 5) -- about a second
quicker off the line than a standing start.  Port: `car_state.waiting`; the port used to
zero the pedal and hold the brake until the green.  (A replay file saved before this,
whose driver held the pedal on the grid, now starts differently.)

### 32. THE MUSIC, AS THE GAME PLAYED IT (2026-10-11; `port/src/music.c`, `fmchip.c`)

Decompiled text: `build/decomp/hmi_fmmidi.c` (the driver, whole), the module itself
`build/hmi_fmmidi.bin`.  The header comment of `music.c` has every rule; in short:

**Three layers.**  (1) HMI's MIDI player, linked into STARTL.EXE at `0xd2000`..`0xe4000`
(the game's glue is `0x2f7a0`.. in `remaining_logic.c`): `0xe3d51` is the timer's routine
(120 a second, the song's own number), `0xe2ca9` passes one message on, `0xe37eb` ends a
song, `0xd3a54` / `0xe29d9` the volumes.  **An immediate in a `push` is object-relative
like every other reference: the timer routine is pushed as `0xd3d51` and lives at
`0xe3d51`.**  (2) The driver: `HMIMDRV.386` = modules `name.com`, 0x30 bytes of header
then 32-bit code whose addresses count from the module's start; `fmmidi.com` (first
module, 12.7 K, plain unoptimised C -- Ghidra reads it at once as `x86:LE:32:default`,
entries `0x80d` a message, `0xf6b` note on, `0x157b` note off, `0x1846` controller,
`0x19bb` pitch bend, `0x201e` instrument, `0x1ed7` which voice, `0x1a7d` bent frequency,
`0x1d64` one register write, `0xd37` packing a bank).  (3) The chip.

**The player:** track 0's messages are never sent; controller 7 is the only message it
touches -- the driver gets `master x (song x value >> 7) >> 7`, song 127, **master = the
game's music volume >> 8 (96 as the game starts it)** -- so the game's music volume and
its fades reach the chip as LEVELS OF THE NOTES, not as a gain; at a song's end every
track's channel gets all-notes-off, controller reset, bend to the middle and volume 0,
and the game starts the song again.
**The driver:** nine two-operator voices, no rhythm mode, register `0xBD` = `0xC0`;
voice = first free, else the first voice of the lowest channel that never had a pitch
bend; before a note the voice's releases are set to the fastest five times over (a
delay loop, in effect); level = `64 - (64 - t)(64 - own level)/64` with `t` from a
64-step table of channel volume x velocity; a drum = channel 9, instrument by note from
`DRUM.BNK`, pitch from its name record; the pitch bend's high byte only, by straight
line between frequency numbers; modulation, pan, reverb, pressure ignored (mono).
Its slips are kept (see the file) except the one that would write over its own memory,
which no song reaches (checked by counting: `OS_MUSICLOG=1` prints the count each time
round a song).

**The chip** is behind `fmchip.h` (reset, write, one frame): on Linux Nuked OPL3
(`port/src/nuked/`, LGPL 2.1+, chip-exact, an OPL2 until told otherwise), writes 40
microseconds apart as the driver's own write routine spaced them.  The driver chosen is
the OPL2 one; `fmmidi3.com` (OPL3, what a Sound Blaster 16 set-up would use, possibly in
stereo) is not read.
**The port's own:** the chip's loudness against the effects (the card's analog mixer):
2.6 x `OS_MUSICVOL`, which makes the music as loud as the old synthesiser made it
(measured on songs 1 and 5: within 3 %).
Checked: all seven songs play round with no note lost and no clipping; old and new play
the same notes at the same times (pitch-class match 0.7..1.0 a window, mostly over 0.9).
Not checked by ear by me, of course.

---

## What is next

(Rewritten 2026-10-11; the list that stood here was from 2026-10-08 and done.)

1. Danik: a real game controller on the controls of section 31; the lop-sided skid, the
   signs' backs, the heading snap, the music's loudness (sections 14, 16, 17, 30).
2. The MiSTer hybrid core.  Before it: look at the 84 MB of static arrays.
   **DECIDED (Danik, 2026-10-11): on the MiSTer the music's FM chip goes in the FPGA** (an
   OPL core; the ARM only sends register writes), to be done when the hybrid core is
   built -- not a software emulator on the ARM.  What that needed first, and Linux too: a
   register-level music player in the place of `music.c`'s own voices, read from HMI's
   OPL driver.  **DONE 2026-10-11, section 32**; on Linux it drives Nuked OPL3 (LGPL).
   The MiSTer core replaces `fmchip.c` alone.
   **The chip goes into the FRAMEWORK, not into this core** (Danik, 2026-10-11):
   `~/Hybrid_MiSTer` (`github.com/ItsDanik/Hybrid_MiSTer`, GPL3, the `hybrid/` submodule of
   every core), so the other ports can use it.  Screamer is its first user.  As looked at
   that day the framework's FPGA side (`rtl/hybrid_host.sv`) has a 44.1 kHz audio ring fed
   from shared DDR3 and no FM chip.  The work, when the time comes:
   - `rtl/`: an EXISTING open OPL core (not written by us; jtopl is the candidate -- its
     licence, state, and OPL2 or OPL3 to be checked first), mixed with the audio ring, fed
     from a queue of TIMESTAMPED register writes in the shared memory (so the tempo does
     not follow the game's frame rate);
   - `hps/`: a call "write value to chip register at time t"; `tools/fakecore`: a software
     chip in its place for testing on the PC;
   - the host `VERSION` goes up; cores built against the old host keep working, without
     the chip.
   A port gets the chip only with a new FPGA build against the new framework AND a change
   in the game to send its music there.  ECWolf would gain: it runs DOSBox's DBOPL on the
   ARM today (`~/MiSTer-ecwolf/ecwolf/src/dosbox/dbopl.cpp`).  Dethrace: not checked.
3. Not built: the network game, the attract mode that plays a replay file (mode bit
   `0x80`), the CD player's eject.  STARTH.EXE (the real hi-res layout) is unread.

---

## The repository and its version (2026-10-11)

This directory is `github.com/ItsDanik/openscreamer` (public, GPL-3, branch `main`): `port/`,
`tools/`, this file, `README.md`.  `.gitignore` keeps out the game (`screamer/`), everything
made from it (`build/`), builds, Ghidra's project and whatever the program writes when run
from here (settings, saves, pictures).  **Check `git status` before a commit: nothing of the
game may ever be added.**

Version = `VERSION` (first push: `0.4.0b`) with its last number raised by one for every
commit since `VERSION` was last changed -- `tools/version.sh` works it out, the build puts
it into the program (`port/version.cmake` -> `os_version.h`, looked up at every build;
`openscreamer --version`, the first line it prints, `--help`).  Nobody edits a number to
commit; to start 0.5 write `0.5.0b` into `VERSION`.  No zip releases yet (Danik).

**The MiSTer port is another repository** -- `ItsDanik/openscreamer_MiSTer`, locally
`~/MiSTer-openscreamer`, core name `OpenScreamer` -- and no MiSTer work is done here.  It
takes this source as a submodule on a branch `mister`, kept as small as possible (its own
MiSTer file, a replacement music file, a few marked lines), so that x86 work is picked up
with a merge of `main`.  Decided with Danik, in this order: (1) this repository; (2) the
hybrid framework gets jtopl's OPL2 (`jtopl2`), proved with ECWolf's music first; (3) the
register-level music player read from HMI's `fmmidi.com`, done on this machine; (4) the
core: resolution as an OSD option (Lores 320x200, Hires 640x400 -- interlaced at 15 kHz
and 30 frames a second on every screen), one frame per field in Lores, music on the FPGA
chip.  Known to need solving there: the port's state restore expects a dynamically linked
SDL (section 29) and the framework's is static; the 50 Hz physics against 59.64 fields.

**Where the MiSTer work stands (2026-10-11, night).**
- `~/Hybrid_MiSTer`: host version 10 (the FM chip) is PUSHED.  Danik compared it by ear
  with ECWolf's software chip on his MiSTer; JTOPL needed six corrections first (listed in
  the framework's CLAUDE.md), found with `sim/fm_song.sh`, which replays a game's register
  writes on JTOPL and on Nuked OPL3.  FM volume: 32 = as the chip comes, 127 = x4.
- `~/MiSTer-ecwolf`: released as ECWolf_20261011 (music on the chip, OSD *FM Music*).
- This repository: 0.4.2b pushed; Danik finds the new music player "ok".
- `~/MiSTer-openscreamer`: stamped from the template (`hybrid/new_core.sh`), nothing
  committed, no game in it yet.  Its first FPGA build missed timing by 0.062 ns on the
  HDMI clock (the MiSTer system's, not ours): try another `SEED` in `core/OpenScreamer.qsf`
  once the core has its real options.
- Idea for the static SDL: rename the `.data` / `.bss` of the GAME's own objects (objcopy
  `--rename-section`) so that the state save covers only those, by `__start_` / `__stop_`
  symbols -- no dependence on how SDL is linked.

**2026-10-12: the MiSTer branch is merged into `main`, and there is a Linux release.**
- `main` was fast-forwarded to `mister` (25 commits; Danik's decision, reversing "no MiSTer
  work here"). What that brought to the PC build: the renderer rewritten for speed (triangles
  listed, then filled by two threads in lanes; fast paths for plain pieces and for cars; the
  backdrop in whole-number steps -- pictures checked against the old ones while it was done),
  `track_under` by cell, RUMBLE on a game controller (the port's own: `rumble_*` in main.c,
  `OS_NORUMBLE=1`), the taller and the wide picture (`OS_TALL=1`: 240 rows, `OS_WIDE=1`: 640
  columns with the high set's flat artwork; the MiSTer core's four sizes -- not in the PC
  menu), and the files of the MiSTer core itself (`port/src/mister.[ch]`, everything behind
  `MISTER_HYBRID`: off in an ordinary build).  The MiSTer side's own notes are in
  `~/MiSTer-openscreamer/CLAUDE.md` (the core is released: `ItsDanik/openscreamer_MiSTer`).
  From now on `mister` is kept equal to `main`: work on either, fast-forward the other.
- **Linux release** (`tools/release_linux.sh`, README text in `tools/release_linux_readme.txt`):
  built in a Debian 11 container (the hybrid cores' `mister-hybrid-tc` image; Debian 11's
  package servers are gone, so SDL2 is built there from source as a bare shared library to
  link against), dynamic SDL2.  The binary asks for glibc 2.27 and for nothing of SDL2 newer
  than 2.0.9.  `dist/openscreamer-<version>-linux-x86_64.tar.gz`; GitHub release tagged
  `v<version>`, pre-release while it is a beta.  Tested from a clean folder with the robot
  (start-up check, a lap, a menu picture), headless; not on another distribution.
- **Windows build, 2026-10-12** (`tools/release_windows.sh`: MinGW-w64 on Linux, SDL2's own
  MinGW package fetched for headers / import library / `SDL2.dll`; README in
  `tools/release_windows_readme.txt`; `dist/openscreamer-<version>-windows-x86_64.zip`).
  Everything that differs is in `port.c` under `_WIN32` (its comment lists it): a stream over
  a container's bytes is a self-deleting temporary file (no `fopencookie`); the statics put
  back between the program's parts are MinGW's `__data_start__` .. `__bss_end__` -- so **SDL2
  must stay a DLL** (linked in, its statics would be among them: `-static` was tried by
  mistake and took `libSDL2.a`); `%APPDATA%\openscreamer`; `_putenv_s`; files opened binary
  (`_set_fmode`) as on Linux; no console (`-mwindows`): output to the terminal it was started
  from, else `openscreamer.log` in the settings folder, and a message box when the game's
  files are missing.  `long` is 32 bits there: nothing minded.
  Tested under wine 11: the data check; a robot lap with telemetry the same as Linux to the
  last digit; race and menu pictures byte-identical to Linux; four menu -> race -> menu rounds
  (the state restore) hidden and two in a real window with the Direct3D renderer; the log
  file.  Sound: identical but for a few thousand samples (the maths library's last digit in
  the grid's revving, and `rand() % 3` choosing the commentator's recording).  **Not tried on
  a real Windows**, nor with a controller there.  With `SDL_VIDEODRIVER=dummy` under wine the
  Direct3D renderer makes the menu leave at once: test hidden with `SDL_RENDER_DRIVER=software`.
- The PC builds keep their own two picture sizes, LOW 320x200 and HIGH 640x400 (the game's
  two sets; both shown 4:3), in OPTIONS.  320x240 and the 640x200 / 640x240 "wide" sizes are
  the MiSTer core's and on a PC only the test switches `OS_TALL` / `OS_WIDE` (Danik, 2026-10-12).

## Legal

The game is **still sold on GOG and Steam** — it is not abandonware. Follow the Dethrace
pattern: publish the reimplementation, never ship game assets or the original binaries.
Keep the whole of `screamer/` and everything made from it (`build/dec/`, `build/png/`,
`build/screamer_flat*.bin`, `build/decomp/`) out of any public repository.
