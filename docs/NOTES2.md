# Screamer 2 — the record

The full record for Screamer 2 (1996), kept apart from `docs/NOTES.md` (Screamer 1).
Same rules: every address, proof, measurement and dead end goes here; `CLAUDE.md` gets one line.
Addresses are flat addresses in `build/screamer2_flat.bin` unless said otherwise.

## S2-0. Decisions (Danik, 2026-10-13)

- Reference program: the 256-colour `screamer2/STARTL.EXE`. 65K colours later.
- One program for both games. A command-line option selects the game; with no option and
  both games' data found, a menu offers Screamer 1 / Screamer 2.
- PC first. The MiSTer port is not to be touched until Danik says so.
- Multiplayer left out for now. Music is CD audio; Danik provides it later.

## S2-1. The files (`screamer2/`, 197 MB, never published)

Programs, all DOS/4GW LE: `STARTL.EXE` / `STARTH.EXE` (256 colours, low / high),
`START65L.EXE` / `START65H.EXE` (65K colours), `S2_3DFX.EXE` (Glide), `SETUP.EXE`.
`S2.BAT` runs `STARTH.EXE`. `SCREAMER.CFG` = `C:\S2<tab>D:<tab>C:\S2<tab>c` (install
directory, CD drive, ...). `CONFIG/SETUP.CFG` 160 bytes (sound card: port 0x220, IRQ 7, DMA 1).
The `.dll` / `.ovl` files are a Glide wrapper from the shop's package, not the game's.

`STARTL.EXE` md5 `15da1f4e6dd271919536e7009f0ae243`, 1,055,478 bytes. LE at `0x2C90`,
223 pages, **four** objects: 1 code `0x010000` (869,383), 2 code `0x0F0000` (2,476),
3 data `0x100000` (130), 4 data + stack `0x110000` (171,152). Entry `0xB3D10`.
`CONFIG/HMIDRV.386` and `HMIMDRV.386` are byte-identical to Screamer 1's (the copies in the
game's root differ).

Containers (`tools/dat_container.py` reads them unchanged; `tools/extract.sh screamer2`
unpacks all into `build/dec2/<CONTAINER>/`, 1,904 files):

| container | holds |
|---|---|
| `DATA/GEN` | `CARS.DTR MENU.DTR SFXSBASE.DTR SPFXBASE.DTR SPFXFULL.DTR`, `P_1..4.MCM`, `SCENE0..3.PTH` |
| `DATA/GAME` | `CARS.DTR` (the fuller one: + suspension, camera position), `MISC.DTR`, `CRUSC.DTR` (dashboards) |
| `DATA/LOCAL` | `ATTRACT.TXT STRINGS.TXT GAME.TXT` |
| `DATA/MAPn` (1–7) | `MAPnOPP.SPL MAPnDIR.SPL MAPnOST.SPL`, `CAMERASn.MCM`, `MAPn.PTH`, `MAPnDAT.DTR` |
| `DATA/SOUND` | 70 `.RAW`, `GAME.DTR MENU.DTR AMBIENT0..6.DTR` (sample tables) |
| `GFX/MAPn{L,H}{0,1}{1,2}` | the track: `MAPn.MMP .UNR .MLB .BCK`, `DEPTH1/2.DPT`, then `.COL .WLB .SMG` (set 1) or `.HIC` (set 2), `MISC.*`, props `.3DX` |
| `GFX/CARS{team}{0,1}{1,2}` | `AUTO1..5` `.3DX .MLB .COL .WLB` (or `.HIC`) |
| `GFX/ENVM*`, `ENVP*` | `ENV.MLB` + `ENV.COL`: reflection maps (menu / per track) |
| `GFX/GAME1,2` | `GAME{L,H}.SPR .FNT`, `COCK1..8_{L,H}.PKC`, `MIRROR_{L,H}.PKC` |
| `GFX/GRAF1,2` | 56 `.BKK` pictures (backgrounds, league, loading, logo), `MENU{L,H}.FNT .SPR` |
| `GFX/MENU{team}{1,2}` | a menu scene built like a track: `MAP1.MMP .UNR .MLB .COL .BCK`, `P_n.MCM` |
| `GFX/MAPS`, `PLATES`, `LOGO` | `.3DX` models of the tracks, number plates, logo + their `.MLB` |
| `ANIMS/SCENEnn{1,2}`, `SCCAR*` | 3D scenes (`MAP1..4.*`, `SPLINES.PTH .TXT`) and the cars shown in them |
| `CONFIG/SETUP` | SETUP's own pictures, `MELODIC.BNK DRUM.BNK MIDITEST.HMP FX_TEST.RAW` |

File-name digits (inferred from contents, the selecting code is not read yet): last digit
1 = 256 colours, 2 = 65K colours; `L` / `H` = resolution; the middle digit 0 / 1 is a second
look of the same track — `MAP1.MMP` and `.UNR` are byte-identical between `MAP1L01` and
`MAP1L11`, `.MLB` and `.BCK` differ. Tracks 5 and 7 have look 0 only. Between `L` and `H`
`MMP`, `UNR`, `MLB` differ; `COL`, `BCK`, `DPT` are the same.

No `MUSIC.DAT`: the game's music is CD audio (`CD_TRACKS` in `MISC.DTR`: `9 8 6 7 4 3 5`,
and in `MENU.DTR`). The program still names `melodic.bnk`, `drum.bnk`, `%s.hmi`: unread.

## S2-2. Against Screamer 1 (first pass, 2026-10-13)

**Same:** the `.DAT` container and LZW; Watcom + DOS/4GW; `MAPn.MMP` (two-level 256×256
grid, cell header, 64-byte piece records; track 1 has 391 cells against 182); `MAPn.UNR`
(size, count, offsets; 594 objects against 192); `MAPn.MLB` (16-byte materials, fillers
16 / 64 / 128 seen on track 1; 209 materials); `.SPL` header; `.DTR` syntax; `.COL`, `.SMG`
(12,288 bytes here = 48 rows), `.PKC`, `.FNT`, `.SPR`, `.RAW`; `CONTROLS.CFG` (1,496 bytes
against 1,328); the cheat words in `MENU.DTR`.

**Revised:** `.3DX` — the block table at `+0x18` is shorter (first block at `0xC8`, not
`0x330`), a block has 16 row pointers where Screamer 1 had 8, carries its name, and the
36-byte face begins with a filler-like word (`0x40`, `0x50`, `0x60`) and has a dword at
`+0x1C`. **Not to be guessed: read the loader.**

**New:** `.PTH` (count, then named records such as `MV_1bird`, floats), `.MCM` (count,
then camera records: ints + floats), `.DPT` (65,536 bytes = 256 × 256, row 0 identity: a
colour-by-distance table; 196,608 in the 65K set), `.WLB` (starts like an `.MLB`: 16-byte
records with filler 16), `.HIC`, `.BKK` ("Invalid BKK magic number!"), `TABS/VIS.TAB`
(161,024 bytes of uint32), replays as `REPLAYS\*.RPL`.

**The game logic is new.** Longest-identical-run comparison of the two code objects
(24-byte seeds): 36.6 % of Screamer 1's code bytes reappear in Screamer 2, nearly all of it
from `0x80000` up (libraries and tables: Watcom's, HMI's); in `0x10000`–`0x30000`, the game
logic, 2 %. A lower bound (recompiled code does not match byte for byte), but the data files
agree: `CARS.DTR` is a different model (`ADERENZA RUOTE F R` per-wheel grip, `TRAZIONE`
front / rear, `DRITTIZER`, `FRENO`, `SOSPENSIONI`), `MAPnDAT.DTR` has `TRACK_DATA` with road
kinds (grip for slick / rain / snow tyres, smoke), `MISC.DTR` has `SKILLS` (a catch-up table
by distance, three levels) and `DAMAGES`. No commentary samples; an engine sample per car
with its own rate (`SOUND/GAME.DTR`), ambient sounds per track (`AMBIENTn.DTR`).

## S2-3. Tools (2026-10-13)

- **`tools/unle.py --fixups`** applies the program's own LE relocation records (fixup page
  table at LE `+0x68`, records at `+0x6C`; record = source type, flags, int16 offset in the
  page or a list when source `& 0x20`, target object, target offset 16 or 32 bits by flags
  `& 0x10`). Screamer 2: 15,427 32-bit pointers (+ 15 selector fixups, left alone).
  The image then holds real flat addresses: **no `RelocateRefs.java` for Screamer 2**, and
  the layout file lists every pointer site (`fixup_sites`), so a pointer is never confused
  with a constant.
  Cross-check on Screamer 1: 11,249 records, deltas `+0x100000` (6,677), `+0x10000` (4,533)
  — the empirical rule — and `+0xF0000` (29, object 2), which the rule could not know.
- `tools/CreateEntry.java <entry>` takes the entry address (default Screamer 1's).
- Ghidra project: `build/ghidra2` / `screamer2`, program `screamer2_flat.bin`.
- **`tools/SeedFuncs.java <list>`**: from the entry point alone the analysis reached 847
  functions / 87,948 instructions. The relocation records name every function whose address
  is taken: pointers into the code object that are not jump-table entries (a site in the code
  with a neighbouring site 4 bytes away is a table: 2,055 skipped) = 792 seeds
  (`build/ghidra2/seeds.txt`); 400 new functions, 380 refused (labels inside known code).
  After re-analysis: **1,300 functions / 140,551 instructions**. Anything else: on demand.
- **`tools/strrefs.py <layout.json> <flat.bin> word...`**: which pointers point at a string
  (from the same records). `RefsByText.java` finds nothing here.

```bash
python3 tools/unle.py screamer2/STARTL.EXE -o build/screamer2_flat.bin \
    --map build/screamer2_layout.json --fixups
/opt/ghidra/support/analyzeHeadless build/ghidra2 screamer2 -import build/screamer2_flat.bin \
    -processor x86:LE:32:watcom -loader BinaryLoader -loader-baseAddr 0x0 -noanalysis \
    -scriptPath tools -postScript CreateEntry.java 0xB3D10
/opt/ghidra/support/analyzeHeadless build/ghidra2 screamer2 -process screamer2_flat.bin \
    -analysisTimeoutPerFile 1800
# then the seeds (the python lines that write seeds.txt: see SeedFuncs.java's header and S2-3)
/opt/ghidra/support/analyzeHeadless build/ghidra2 screamer2 -process screamer2_flat.bin \
    -scriptPath tools -preScript SeedFuncs.java build/ghidra2/seeds.txt -analysisTimeoutPerFile 1800
/opt/ghidra/support/analyzeHeadless build/ghidra2 screamer2 -process screamer2_flat.bin -noanalysis \
    -readOnly -scriptPath tools -postScript DecompFunc.java 0x4b4d4
```

First anchors (pointer sites, from `strrefs.py`):

| string | site | |
|---|---|---|
| `%s.3DX` | `0x4B547` | in `FUN_0004b4d4`, the object loader (`build/decomp2/obj3dx_load_4b4d4.c`) |
| `…%s.3dx` errors | `0x4B1F9 0x4B218 0x4B237` | `FUN_0004b1a4` |
| `%s.UNR` / `%s.MMP` / `TABS\VIS.TAB` | `0x4F78E` / `0x4F905` / `0x4FABE` | |
| `%s.MLB` / `%s.WLB` | `0x4FFE2` / `0x4FFDA` | loaded side by side |
| `depth1.dpt` ×3, `depth2.dpt` | `0x2F10F 0x2F14F 0x2F17F`, `0x2F1C1` | |
| `%s.mcm` / `p_%c.mcm` | `0x14D37` / `0x40A40` | |
| `%s.pth` / `%s%c.pth` / `splines.pth` | `0x30EDE` / `0x351D5` / `0x112E0` | |
| `%s.spl`, `%sopp` `%sost` `%sdir` | `0x321D8`, `0x30A66 0x30AC6 0x30AFF` | |

`FUN_0004b4d4` is Screamer 1's loader again: the free-slot table ended by `0xFFFFFFFF`
(`DAT_00134790`, next index `DAT_00133378`), error returns 2 / 3 / 5, a `3D_HEADER` and a
`3D_BLKOBJ` allocation (`count × 0x20 + 0x60`). The engine layer is the same family; so the
split is: engine around `0x4B000`–`0x50000` and up, game logic below.

## S2-4. The original in DOSBox (2026-10-13)

DOSBox Staging 0.83 (flatpak, user install). `screamer-dos [1|2] [low|high|65l|65h|setup|dos]`
(`~/.local/bin`), settings `~/dosgames/screamer.conf`, the games as a COPY in
`~/dosgames/c/S2` and `~/dosgames/c/SCREAMER`, captures in `~/dosgames/capture`.

- Each game's CD is the `.bin` / `.cue` in its own `cd/` directory (`c/S2/cd/SCREAM2.cue`: data +
  8 audio tracks; `c/SCREAMER/cd/Screamer.cue`: data + 7 audio tracks), mounted as D: by the
  launcher (`mount D <cue> -t cdrom`). Without it Screamer 2 stops at "PLEASE INSERT SCREAMER 2
  DISK" and Screamer 1 leaves without a word.
- **Both run** (checked on a hidden display up to the intros). `~/dosgames/c/SCREAMER` is now
  Danik's complete install (`TABS/PERSP01.TAB VIS.TAB`, `CONFIG/SETUP.CFG SETUP.DAT`, `REPLAYS/`).
- Unseen runs: `Xvfb :77`, then `env -u WAYLAND_DISPLAY DISPLAY=:77 SCREAMER_DOS_FLATPAK="--nosocket=wayland
  --env=SDL_AUDIODRIVER=dummy --env=SDL_VIDEODRIVER=x11" screamer-dos 2`, picture with
  `import -window root`, stop with `flatpak kill io.github.dosbox-staging`.

## S2-5. The first pictures: the track files ARE Screamer 1's (2026-10-13)

**The port's Screamer 1 loaders and renderer draw all seven Screamer 2 tracks as they are.**
Tried first with a scratch data directory (Screamer 1's files, `MAP1L` swapped for Screamer
2's `MAP1L01`): the only thing in the way was the limit of 512 objects.

- `.3DX` / UNR objects: the block is Screamer 1's block. Rows of face order at `+0x28` (block
  relative), name at `+0x68`, faces from `+0x78`, vertex / UV / position-record offsets from
  the object's start. What S2-2 called "revised" is only the object header: the block-offset
  table at `+0x18` is 44 entries long (first block at `0xC8`) and, for cars, holds further
  runs of offsets after the first `count` — more sets of blocks (detail levels by the look of
  it; **unread**). Sixteen row pointers: Screamer 1's header has the room for sixteen too, the
  port reads eight.
- `MAPn.MMP`: cells, five (base, count) pairs, 64-byte records, road-direction points and
  wall segments all parse: track 1 = 1,609 records (948 + 661), 665 wall segments, 1,441
  direction points. `MAPnOPP.SPL` builds a path that lies on the road (2,209 of 2,343 samples
  over a track surface).
- Objects a track: 594, 672, 586, 580, 529, 678, 345 — all parse.
- `MAPn.MLB`, `.COL`, `.BCK` (PCX) load unchanged. `.SMG` is 48 rows, not 64 (loaded; what
  uses it is unread). Track 7 has no `.BCK` and no `.SMG` (it is indoors).

**Not in the pictures yet (all unread):** the distance table `DEPTHn.DPT`, `MAPn.WLB`,
`MISC.MLB` / `MISC.COL`, the second run of blocks in an object, what chooses the look
(read since: S2-5d; look 1 of track 1 has a dark sky and a 20,996-byte backdrop against
62,958), cars, props, moving scenery (`MAPn.PTH`).

**In the port:**
- `-game 1|2` (`$OPENSCREAMER_GAME`), else by what is found: `./screamer`, `./screamer2`
  (and `build/dec2`, …); `-data DIR` is recognised by its files (`GFX/MAP1L01.DAT` = Screamer
  2). `os_game()`. Both found and no option: Screamer 1 for now, `os_both_games()` is set
  for the menu that will choose. The start-up check has its own list for Screamer 2
  (11 DATA + 18 GFX containers, `MAPnH01` optional); `ANIMS/` is searched for containers too.
- `openscreamer -game 2` = a ride round a track along the opponents' line (`OS_TRACK=1..7`,
  default 1; `-d` to fly; `OS_RES=high`; `OS_COND=1|2`). `openscreamer -game 2 build/dec/MAPn n
  --shot f.ppm t` as for Screamer 1. No cars, no driving, no menu.
- `mesh`: its arrays are allocated to the object's size (they were fixed at 2560 vertices /
  1792 faces, about 150 KB an object); the track's objects are an allocated array
  (`UNR_MAX_OBJ` 2048 is only the offsets' table). Screamer 1 pictures byte-identical before
  and after (ten shots: six tracks, a reversed one, the robot, hi-res, cones — the check is a
  script comparing against a build of the commit before).

## S2-5b. Moving parts: the frame is the whole matrix (both games) — 2026-10-13

Danik: track 1's `V246` (the watermill) hung in the air on its side. So did **Screamer 1's**
windmills on track 3, unnoticed: the port's rule for moving parts was wrong for both games.

The engine code is the same in both (Screamer 1 `0x8ad14` / Screamer 2 `0x7ec30`): the 12
words of a frame are copied, sign-extended, to the block's `+0x28` — **exactly where the
matrix and position built from a placement's angles go** (`0x7f270`: matrix at `+0x18` of
its argument, the position's three words after it, the inverse at `+0x48`); the 12 words
after them are the inverse, to `+0x58`. So a frame is the block's whole matrix, rows in the
engine's order (x, up, the other ground axis — the order of a piece record's X, HEIGHT, Y),
applied to the vertex as stored (row vector × matrix):

    x'  = (m0*x + m3*y + m6*z) / 16383 + t0
    up' = (m1*x + m4*y + m7*z) / 16383 + t1
    z'  = (m2*x + m5*y + m8*z) / 16383 + t2

The vertex is a ROW multiplied into the matrix (the words go down the columns). The first
fix of this day read them across: the mill stood up but its wheel turned about the vertical
(Danik saw it). Read down, the wheel's own axis stays where it is in every frame, and the
sails, the big wheel and the swings turn in their planes (pictures at three moments each).
**The mirror, and the back faces.** That turn (y and z exchanged) is a mirror, and the engine
allows for it: drawing a block it first writes `0x7c` (`jl`) into three places of the face
routines (`0x7ec1e`; Screamer 1 `0x8ad02`), and when the block has frames `0x7d` (`jge`)
instead (`0x7ec0c` / `0x8acf0`) — the back-face test the other way round. In these objects
the stored normals point TOWARDS the side that shows (the mill's walls: outwards), in all
others away from it. The port turns a moving part's normals round when it poses it, so its
renderer keeps one rule. Before this the mill was inside out — far walls seen through the
near ones, the wheel's picture only from inside the house (Danik saw it).
Objects with frames are modelled Z-up (the mill: roof at +z, wheel a disc in x–z), and every
frame's matrix carries the turn to Y-up — the "identity" `(1,0,0 / 0,0,1 / 0,1,0)` is that
turn. The port had read the rows against (x, z, y) and swapped t1 / t2, which cancels the
turn. The position record (`+0x10..+0x18`) of such a block is in the model's own axes: it is
frame 0's translation with its last two swapped.
Checked by eye from four sides: Screamer 2's mill (wheel upright, its foot at the house's
foot, the flat wheel picture 7 units outside the wall) and donut sign (track 4); Screamer 1's
windmills (track 3), big wheel and swings (track 4), cranes (track 5).
Animated objects: Screamer 1 `V335` (t3), `V213 V235` (t4), `V240 V327` (t5), `V211 V275`
(t6); Screamer 2 `V246` (t1), `V538` (t4). Names: first block `aMV_…`, parts `z…`.

## S2-5c. The cars' line, and the ride — 2026-10-13

- `MAPnOPP.SPL` in Screamer 2 is ONE spline (36 nodes on track 1, node `+0x0C` = 0 / 10 / 20 /
  30, interpolated along the curve by `0x1099a`). The curve is Screamer 1's: `0x1099a` picks
  (in, out, next in) / (out, next in, next out), `0x4a831` is the quadratic B-spline (weights
  `(1-u)^2/2`, `u^2/2`, the rest) and also gives the direction; `0x2d59c` moves a cursor
  along it by distance; `0x309b0` walks each spline once at load and writes node `+0x34`.
- **The line is not on the road everywhere** (track 1: up to 6,800 units from the nearest
  road-centre point; over the void at one bend). In Screamer 2 it is what the cars aim at,
  not where they are put — how they use it is unread (users of the table `0x127350`:
  `0x15441`…`0x1792a`, `0x30074`…).
- So the port's ride goes over the MMP's road-centre points (array 2), chained from the one
  nearest the grid by "nearest unused one ahead", smoothed, resampled: every sample is over a
  track surface on all seven tracks (was 96 % along the line). `OS_PATHLOG=1` lists samples
  over no surface.
- Piece record `+0x3C` (the last word; always 0 in Screamer 1) holds 36 / 39 / 27 … on track
  1: the ids of `ROAD KINDS` in `MAPnDAT.DTR` (36 asphalt, 27 grass) by the look of it. Unread.

## S2-5d. A track's condition, the fade, the far plane, the clear colour — 2026-10-13

**The race record** is at `0x130c48` (0x124 bytes, copied to `0x130d6c` by the race loader
`0x2e1f3`): `+0` cars (4 in the attract race), `+4` track 0–6 (`0x130c4c`), `+8` the
**condition** 0–2 (`0x130c50`; `0x127358`, when not −1, overrides it), `+0xC` detail
(`0x130c54`), then 0x2C bytes a car from `+0x10`. The attract race (`0x3c9ac`): condition 0.

**The container** (`0x2e676`): `"%s\gfx\%s%c%c%c.dat"` = `map<1+track>`, **`L` when detail
is 0 and `H` otherwise** (so L / H are the DETAIL's sets, not the resolution's: STARTL and
STARTH are separate programs), the look `'0' + (condition & 1)`, and `'1'`.

**The track loader `0x2ef4e`:**
- `TRACK_DATA` of `MAPnDAT.DTR` → `0x1240f8` (0x110 bytes). First number = how far the
  track is drawn, negative (−50000; track 4 −60000): `0x7f504` writes it into the face
  routines, as it is at detail 2, × 0.8 (truncated) below. The fifth = "background 256".
- condition 0: `depth1.dpt`, fade off (`0x113740` = 0), backdrop on (`0x113744`), the picture
  cleared to the background-256 colour; 1: `depth1.dpt`, fade on except on track 3 (the
  fourth), cleared to 0; 2: `depth2.dpt`, fade on, backdrop off, cleared to `0x13`.
  Track 6 (the seventh): `depth1.dpt`, everything off. Only tracks 1 and 5 HAVE a
  `DEPTH2.DPT`: condition 2 is theirs (fog; snow). Tracks 5 and 7 have no look 1.
- the table: `0x5030a` reads 0x10000 bytes into a 0x10400-byte block (`0x11374c`).

**The fade** (`0xbd5e9`; the same lines at `0xbf0ba`, `0xcc409`, `0xcd49f`, `0xce29b`) is per
FACE: `z = ((z0 + z1) / 2 + z2) / 2` of the face's first three corners (`0xbcf1a`: their
depths as transformed, unclipped), `d = |z − near|` held below `range = near − far`,
`row = d·16384 / range & 0x3f80`, `<< 2` = the byte offset: 128 steps on the table's even
rows; then every texel goes through that row. near / far (`0x113738` / `0x11373c`, set by
`0x12d31` when the view changes; depths are negative in the engine): condition 1 — 0 / 27000
in view 1, 6000 / 35000 in the others; condition 2 — 0 / 25000. (View 5 leaves them.)
A face with material flags `& 0x60000` (`0xae5d8`) takes a row beyond the table instead
(`+0xff00 + flags >> 9`: the 0x400 bytes after it, filled at `0x50341`…): **unread**.
`0xc9ff0` sets the three depths to `near` (no fade) for something: **unread**.
`DEPTH1` darkens to black by row 200; `DEPTH2` lightens to the fog's grey.

**In the port:** `OS_COND=0|1|2` (was `OS_LOOK`); the far plane from `TRACK_DATA` (detail 2
whole, else 0.8; ULTRA none); the clear colour; no backdrop in condition 2 and on track 7;
`fade_for()` gives every face its row on both drawing paths (`OS_NOFADE=1` turns it off).
Pieces are looked at out to 1.2 × the far plane (the port's own). The view-1 distances and
L / H by detail are not in yet (the port still takes H with the high resolution).
Pictures: `build/png/s2_conditions.png`.

## S2-5e. `.WLB`, the cars' files — 2026-10-13

- **`.WLB` is the 8 MB version's material library.** Start-up (`0x2b1a9`): free memory
  below `0x55c120` — refused ("You need another %d bytes"); below `0x6ddd00` — "Starting 8Mb
  version", `0x11f14c` = 1 (`/8MEGS` forces it). With that bit the material loader `0x4feb4`
  gets flag 8 and opens `%s.WLB` instead of `%s.MLB` (`0x4ffce`), and the track loader reads
  no `ANIMATIONS`. The port is the 16 MB version: `.WLB` is not read.
  (`0x4feb4`'s flags: 1 = a library, 2 = with a backdrop — `0x20000` more and the `.BCK`
  through the PCX reader `0x4fcc2` —, 4 = make it the current one, 8 = `.WLB`. It keeps up to
  32 libraries by name, 0x24 bytes each at `0x133448`.)
- **A car** (`0x2f519`, per car record): container `"%s\gfx\cars%c%c%c.dat"` = team
  (`0x130c58 + car·0x2C`) + 1, condition & 1, `'1'`; in it `auto<1 + model>` (`0x130c5c + …`):
  its `.MLB` (or `.WLB`) and `.3DX`; the blocks `fauto`, `MV_mauto`, `rauto` and one more
  (`0x110d98`) are looked up by name (`0x4bc03`). Its figures: `0x11c600 + (team·5 + model) ·
  0x58` (from `CARS.DTR`). `misc` + `ashadow` first.
- The object is Screamer 1's car: twelve blocks, the same names and flags (`MV_mauto 0x6a00`,
  `fauto` / `rauto 0x0a00`, tyres `0x0001`, hubs `0x0800`, `fai 0x0201`). After the first
  twelve the header's table has twelve more (entries 13–24), and one each at 26 and 41:
  further sets (unread; S2-5).
- **Palettes:** every `.COL` of the game (tracks, `MISC`, cars of every team and both looks)
  has the same colours 0–119; a car's textures use indices up to 118. Tracks own 120–255.
- **In the port:** `mlb_add()` puts a library after those loaded and `mesh_rebase()` moves an
  object's faces onto it; `s2_cars_show()` stands one car of each team on the road past the
  start (`OS_CAR=0..4` the model). Bodies, liveries and the night containers draw; the
  reflections (`ENVP*` / `ENVM*`), shadows and wheels are not looked at yet.
  Pictures: `build/png/s2_cars.png`.

## S2-6. Plan

1. Groundwork: decompiler project, tools (this section and S2-3).
2. ~~Split the port first~~ — changed 2026-10-13: the engine already serves both games, so
   Screamer 2 goes in beside Screamer 1 (`os_game()`), its own logic in its own files as it
   is written; Screamer 1 pictures are compared with the commit before at every step.
3. First picture: DONE for the tracks (S2-5). Next: `.DPT`, `.WLB`, the second look; cars.
4. Driving: car model, road kinds, walls, cameras (`.MCM`), opponents, `SKILLS`.
5. Race screen and sound: HUD, dashboards (`CRUSC.DTR`), mirror, engine / ambient, CD music.
6. Menus and modes: `.BKK`, the 3D scenes, arcade, championship, time attack, customisation,
   records, replays (`.RPL`).
7. Later: 65K colours, hi-res, Windows, MiSTer.
