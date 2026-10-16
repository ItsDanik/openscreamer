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

## S2-5f. The decompiled text, and where the car's physics is — 2026-10-13

**Tools.** `tools/patch_fpu2.py` inlines Watcom's FPU helpers (here they work on ST0 itself:
`0x4e99a` cos, `0x4e9a4` sin, `0x4e9f1` tan, `0x4ea60` sqrt, `0x4ea12` = to a whole number
with the control word set to TRUNCATE — Ghidra's `ROUND()` is a cast, as in Screamer 1);
the patched image is its own project, `build/ghidra2f` (seeded BEFORE the first analysis:
1,540 functions). `tools/DecompRange.java <dir> <lo> <hi>` writes every function of a range
and `INDEX.txt` (entry, size, callers, callees): `build/decomp2/` holds all 499 of
`0x10000`–`0x52000` (200 KB of its 270 KB; the gaps are mostly `0x39000`–`0x44000`, menus).
`tools/s2consts.py` writes each FPU constant's value after its name in that text (float or
double by the instruction that reads it; 699 constants). `tools/s2dec.sh <addr>…` does one
function and the constants. `FUN_0004eaed` is sprintf (its arguments are on the stack:
Ghidra drops them — read the pushes with objdump).

**The cars** are 0xCC0-byte records at `0x119300` (count `0x130c48`); word 0 = its number,
`+0x5C` (word 0x17) the 0x58-byte figures from `CARS.DTR`.

**A race frame, `0x2baa4`** (`0x2bd7f` and `0x2c132` are its twins for other modes; step
`0x133350`):
1. every car: `0x1b36f` — grip from the four wheels' road kinds (table `0x11cdf0`, 0x24 a
   kind: the `ROAD KINDS` of `MAPnDAT.DTR`), engine / skid sounds, then `0x1c7f8` (4.6 KB) and
   four calls of `0x19cac` (5.8 KB, one a wheel), `0x1d9fc`;
2. every car: `0x16824` → `0x28f96`, `0x29571` (3.6 KB);
3. every car: `0x1b92b` → `0x19810`, `0x1bef1`, `0x1c4fb`, `0x1d9fc`, `0x1e529` (3.3 KB);
4. `0x245a3` (18 KB; with `0x2348b`): returns how much of the step could be done before
   something touched — below 2 the step is shortened and the cars flagged `+0x4C & 8` are
   done again (a real collision solver, not Screamer 1's two circles);
5. `0x22d35` → `0x20a18` (9 KB); then every car `0x1684e`, `0x16878` (3.2 KB), and
   `0x167e2` → `0x1f3ce`, `0x1f756`, `0x174ca`, `0x1756f`, `0x19409` → `0x153c6` (5 KB),
   `0x17726` (7.4 KB: uses the opponents' spline cursor `0x2d59c` — the drivers).
About 5,000 lines of decompiled text for the car alone. The plan for it: translate routine
by routine into the port keeping the record's layout (word numbers as in the game), then
name things — not a rewrite from understanding, which is how feel gets lost.

## S2-5g. Carrying the car's routines over: the converter — 2026-10-13

- `tools/DecompRange.java` / `DecompTo.java` now (a) retype every POINTER parameter as a
  plain int before decompiling — with a pointer the text reads `(float)p[0x135]`, which does
  not say whether a float is read or an int converted; as an int it is `*(float *)(p +
  0x4d4)` / `*(int *)(p + 0x50)` — and (b) write a `// GLOBAL name @addr size type` line for
  every global a function uses (the decompiler's own types).
- **`tools/s2c.py <out.c> <roots> [--hook …] [--fix file]`** turns routines into C the port
  compiles: names stay addresses, memory is one block standing for the data object
  (`port/src/s2_mem.h`: `MP(address)`, the decompiler's type words, `ROUND` = cut off),
  constants become numbers, everything from `0x4a000` up (engine, libraries) and anything
  named with `--hook` is left as a call for the port to answer. From the three per-car
  routines (`0x1b36f`, `0x16824`, `0x1b92b`) it takes 25 routines, 3,550 lines; what is left
  to settle by hand is one family, below, and the routines it lists as having stack /
  register artefacts (a float argument on the stack is dropped by the decompiler: `0x19810`,
  `0x14dc0`, `0x10388` … — read the pushes).
- **The ground under a wheel** (`0x28f96`, for each of the four points at car `+0x514 + 12·i`):
  x, y into `0x11cdcc` / `0x11cdd4`, then `0x28ce6(0)`: the 3 × 3 cells round the point
  (`(x + 0x100000) >> 13`; offsets from the table at `0x112e84`, 12 bytes an entry) and in
  each `0x28db7`: every record of the cell's first run, the object's bound circle, then
  `0x4c24c` (`0x4c474` when the argument is 1) up to five times with the point moved by 4 —
  Screamer 1's test again. Results in variables kept in the CODE object: `0x4cb98` the
  height, `0x4cba0` / `a4` / `a8` the normal (turned back by the record's heading),
  `0x4cbec` the road kind (to wheel `+0x5C`). With three wheels found the fourth's height is
  put on their plane; with fewer, the missing take the mean.
  For the port this family (`0x28ce6` down) is a hook answered from its own scene.
- The wheels are 0x70-byte records at car `+0x574`; `+0x08` on the ground, `+0x44` height,
  `+0x5C` road kind, `+0x64..+0x6C` normal.

## S2-5h. The converted physics compiles — 2026-10-13

`python3 tools/s2c.py port/src/s2_gen.c 0x1b36f 0x16824 0x1b92b --hook 0x28ce6 0x29571
0x204af 0x2a371 --fix tools/s2c_fix.txt` → 15 routines, 2,459 lines, no warning at `-Wall
-Wextra -O2`. What it took, beyond S2-5g:
- **`tools/FixSigs.java tools/s2_sigs.txt`** gives the functions with STACK arguments their
  real signatures in the Ghidra project, so the callers show the arguments (they were
  dropped): `0x10003 0x1016b 0x111d6 0x11212 0x126ad 0x16878 0x19810` (EAX, then a float —
  and an int — on the stack), and the all-stack ones that leave a float in ST0, declared
  `__cdecl` returning `double`: `0x14dc0(a, b)` = a − b brought into −π…π, `0x11253(double)`
  = square root or 0 below 0, `0x19c61(a, b, c, d)` = 1 − d·a·b·step, not below 0,
  `0x10d9e(x, y)` = an angle from a vector (through `0x4ea98`, arc cosine).
  (A float10 return with custom storage in ST0 is not taken up by the callers; a 10-byte
  `__cdecl` return grows a hidden pointer argument. `double` works.)
- the converter: a float compared with a hexadecimal constant is compared as its BITS
  (`cmp dword, imm`: `BITS(x)`); the engine's variables in the code object (`_FUN_0004cb98`…)
  are memory at `0x4c000` (a page of its own); a C-pointer local handed on as an address goes
  back to a number (`PM()`).
- `tools/s2c_fix.txt`: `0x19cac` hands a local float to `0x111d6` by address (through a spare
  word of the block); **`0x1bef1`** (the wheels' springs) reaches the wheel record through a
  float pointer, which hides its whole-number fields — they are, from the instructions, wheel
  `+0x04 +0x28 +0x2C +0x30 +0x34 +0x44 +0x48 +0x5C +0x60` (floats: `+0x00 +0x14 +0x18
  +0x20`).
- hooked for now: the ground query `0x28ce6` (the port's own), the walls `0x29571`, the
  wheels' smoke `0x204af`, `0x2a371`; and the sound calls `0xaebf0 0xaee18 0xaee98`,
  `0xae6e3` (a copy of bytes: to, from, count), `0xaea54`, `0xafe84`.
Next: the port's side (s2_hooks.c) — the car record's first values (`0x2f519`, `0x14e20`,
`CARS.DTR`, the road kinds), the keys, the frame, drawing the car from its record.

## S2-5i. No hand corrections left; the whole race frame converts — 2026-10-13

- The decompile scripts now also retype, before the final pass, every pointer LOCAL
  (`HighFunctionDBUtil.updateDBVariable` to int, up to three rounds) and every pointer
  GLOBAL above `0x119000` (defined as an int in the listing; it is then named `INT_…`).
  With parameters, locals and globals all plain numbers, every field access in the text is
  `*(float *)(x + off)` or `*(int *)(x + off)`: nothing is left to tell apart by hand.
  The wheel routine's field list of S2-5h came out the same from the tool; the fix file is
  empty again.
- The converter puts a routine's locals whose address is taken, and its local arrays, on a
  stack of their own inside the block (`s2_push`), the routine becoming a body and a wrapper
  that gives the stack back: `&local` is an address like any other.
- The three per-car routines WITH the walls (`0x29571`): 21 routines, 3,327 lines, no
  warning. The whole race frame `0x2baa4` + the car set-up `0x2f519` + `0x14e20`: 65
  routines, 9,839 lines, 94 messages to go (mostly the set-up's file names and the engine's
  object lists), 47 hooks.

## S2-5j. The game's race routines RUN in the port — 2026-10-13

**State:** `openscreamer -game 2` is a race on any of the seven tracks: four cars on the grid
under the gantry, held three seconds (the game's own: `0x2baa4` sets `0x11f0a8 & 0x20` when
the race clock `0x13335c` passes 3.0), the player's car on the port's six controls, three
opponents driving themselves (`0x17726`), walls, the game's collision solver. Headless on
track 1 with the accelerator held: 17.6 km/h after half a second, gears 1-2-3-4 by two
seconds, 106 km/h. No HUD, no sound, no laps, no start lights; the view is the port's own
chase camera.

**How it is put together**
- `tools/s2_set.txt` is the conversion: roots `0x2baa4` (the race frame), `0x2f519` (a
  car's set-up), `0x309b0`; engine routines taken as well: `0x4a831` (the spline's curve),
  `0x4af7d`, `0x4b016`; 58 routines, 13,669 lines into `port/src/s2_gen.inc`, which
  `s2_hooks.c` includes after its hooks. `python3 tools/s2c.py port/src/s2_gen.inc $(cat
  tools/s2_set.txt)`.
- **The block** (`s2_mem.h`): `0x110000`–`0x210000`; the data object, then what the port
  allocates (`0x140000` up: the cars' line, the objects' records), then the routines' stack
  (`0x1e0000` up). Variables the engine keeps in its code object get pages on demand.
- **A routine's stack frame is in the block**, each local at the offset in its name
  (`local_9c` = 0x9c below the return address): the compiler lays structures out on the
  stack and the decompiler shows them as separate locals — `0x309b0` clears a 0x78-byte
  cursor through the address of its first word. `&local` is an address; `(int)(array + n)`
  counts in elements (`PM()`).
- **`s2_hooks.c`:** the data tables as the loaders read them (`s2_dtr` = `0x31235`:
  `GAME/CARS.DTR` `CAR_PARAMS` → `0x11c600` 0x58 a car, `MISC.DTR` `SKILLS` → `0x11f0f4`,
  `DAMAGES` → `0x11cce0`, `MAPnDAT.DTR` `TRACK_DATA` → `0x1240f8` and its road kinds copied
  to `0x11cdec + id·0x24`); the easing tables `0x11f2a0[rate][i]` = (0.5 + i·0.005)^(50/rate)
  (`0x315a4`; `0x4b016` picks the row for the frame rate, `0x1016b` / `0x111d6` ease with
  it); `MAPnOPP.SPL` in the game's layout (table at `[0x127350]`, 0x24 an entry); the race
  record; the player's controls record `0x127708` (`+0xA8` accelerate, `+0xAC` brake, `+0xB0`
  / `+0xB4` left / right, `+0xB8` / `+0xBC` gears, each 0…1023 with its sign in `+0x14 +0x30
  +0x84 +0xA0`; `+0x40` = 1 for keys); a camera record per view at `[0x1192f4 + 4·view]`
  (`+0x48 +0x50` place, `+0x58` heading: the engine sound is placed by it).
- **Hooks:** the ground under a point (`0x28ce6` → the port's `s2p_ground`, the normal turned
  to point up); the nearest road-centre point / wall segment (`0x2a371` → `s2p_nearest`, a
  copy of the record kept in the block); the engine's object list as far as the car reads
  and writes it (`0x4bc94` a car's model, `0x4bc03` a block by name, object record 0x40 bytes
  + 0x20 a block at `[0x11da20 + n·0x1c]` / `[0x11da24 + …]`: `0x16878` writes the car's
  place and angles there each frame — what a renderer would read); copies, fills, `rand`;
  nothing for sound, smoke, the replay recorder, the network.
- `0x10d9e` (angle of a vector) is written by hand; the power routine `0x0afed2` (ST0 to the
  power ST1) and arc cosine `0x4ea98` take their arguments on the FPU stack — their four
  call sites are rewritten in `tools/s2c_fix.txt` from the instructions (`0x19409`: |cos|^4;
  `0x1f756`: the torque curve `(1 − (G·0.22 + r·c − 1)²)·G²·0.8`, G the gear's factor from
  `0x112e24` = 2.5 2.5 1.6 1.2 1.0, r = revs / most revs, c = 1.6, or 1.5 in gear 0;
  `0x2d59c`).

**What went wrong on the way (each a class, fixed in the tools)**
- *Seeding functions before the first analysis* breaks Ghidra's switch recovery (the tables
  are in the code): 37 routines came out in pieces. `tools/s2project.sh` analyses first.
- *A float kept in a local the decompiler leaves `undefined4`*: `local = 1.0 − x` then reads
  in C as a conversion to a whole number — the grip's blend (`0x19c61`) came out 0 and the
  car had no drive. The scripts now make such locals `float` when floating-point operations
  write or read them.
- *A float result in ST0 from a routine with register and stack arguments* (`0x108f5`):
  declared through `__watcall` with all four register parameters written out.
- *Stack structures*, above. *`x = f()` of a void routine*. *Arrays of byte pointers.*
- The car's place is whole numbers, set by cutting the fraction off (`0x1f09a`: the helper,
  then `fistp`) — the game's own; with no drive it alone moved the car a unit a frame.

**Known / next:** (the opponent that stopped after half a lap was the 50-a-second step,
S2-5l: at 25 all three keep going for two minutes on every track, means 96-130 km/h);
smoke not drawn (wheels and springs: S2-5o; the shadow: S2-5p; the race screen: S2-5q); the
trackside cameras, the race's end, sound; `0x204af`
(smoke) hooked away; the ground hook uses the port's stored-normal plane where the engine
builds the plane from the triangle under the point (`0x4c24c`).

## S2-5k. The car's tilt, the game's views — 2026-10-13

Danik: "cars might be a bit small (camera?)", and "on slopes they do wheelies".

- **Tilt.** The car's angles are `+0x4e4` nose up / down, `+0x4e8` heading, `+0x4ec` lean;
  `0x16878` writes them as they are (× 1024 / π, & 0x7ff) into the object record `+0x14 +0x18
  +0x1c`. Measured on track 1's climb (`OS_S2PHYS=900 OS_S2WHEELS=1`): `+0x4e4` is **+ with
  the nose up** (0.086 where the front wheels' ground is 72 above the rear's over 864), and
  `+0x4ec` is + with the +X side up (wheel 0 = front, +211, −432). The port's `tilt()` is +
  with the TAIL up, so `o->pitch = −angle[0]`; the lean goes over as it is. It had been drawn
  the wrong way: nose into the road uphill, front wheels in the air downhill. Checked side-on
  (`OS_S2ORBIT=<degrees>,<distance>,<height>` with `OS_S2RUN`) and from both ends on a bank.
- **Size.** The model is the physics' size: the wheel blocks are at ±211, −432 / +400, the
  car record's wheels at ±211, ∓432. The port's stand-in camera was 2300 behind; the game's
  is 1300.
- **The views** (`0x12dff(camera record, 1)` before each picture, `0x2b873`; the record is
  0xaec bytes at `[0x1192f4]`: `+0x48` place, `+0x54 +0x58 +0x5c` down / heading / lean,
  `+0xadc` the car followed, `+0xae0` flags, `+0xae4` the view, `+0x2c` the view last shown
  — a change snaps, otherwise the angles ease by the tables at `0x1240c0`; `+0x60` up to 80
  trackside cameras of 0x20 bytes from `CAMERASn.MCM`, count `+0xa60`):
  - **2** `0x13bab` from the car: its place, + body height (`car +0x218`) + 400; the car's
    angles (lean + half of `+0xc20`). **6** `0x13cbd` the same, 300 above.
  - **4** `0x14200` at the car's `POSIZIONE TELECAMERA` (`CARS.DTR`, car `+0xac +0xb0`: −10,
    550 for most), turning with the car, nose-up and lean at half, looking 0.3 down.
  - **3** `0x13f38` the far one: 1300 behind (eased from the last), looking 0.274 down, level;
    its heading eases towards the car's, pulled 0.3 × speed / top speed towards the way the
    car is really going (`+0x508`); the easing gets quicker with speed (95 − 10 × speed / top).
  - **5** `0x13e1a` backwards (the mirror, 35 rows). **1** `0x13223` trackside: the nearest
    camera of the `.MCM` list (`0x14acf`), five kinds (`0x13514`…`0x13a8e`). Not carried over.
  - Keys (`0x132b4`, the order of the bits): 2, 6, 4, 3 → the port's F1–F4 and C.
  - For views 2, 6, 5 the car (`+0xbf0`) and `+0xbf8` are taken off the draw list for the
    picture (`0x2b6b4`) → the port's `g_hide`.
  - `0x12d31` (set the view) also sets the fade distances of S2-5d.
- In the port: `0x12dff` and `0x12d31` are in `tools/s2_set.txt` (`0x132b4`, `0x13223`
  hooked); `s2_view()`, `s2_camera()` (`s2_hooks.c`), `s2_camera_show()` (`main.c`: the
  camera's lean is the other way round from the port's, checked on a bank). A race starts in
  view 3 (the game takes it from its settings, `0x130d68`). `OS_VIEW=0..3` for shots.
- Not read: the lens. `0x4f6c4(320, rows, rows × 0.65)` is the picture's size and eye line,
  as Screamer's; the port keeps Screamer's 128.

## S2-5l. The step, and what the game cuts off — 2026-10-13

Danik: "in map 3, when the race starts the car slightly drifts to the right (I only press
accelerate)". Not the road: the game's own arithmetic, made worse by the port's 50 frames a
second.

- **The step** `0x133350` is the time the last pictures took, eased (`0x4ae9a`: (9 × old +
  1.1 × measured) / 10, at most doubled), kept between **0.02 and 0.2 s**; the clock counts
  1/100 s. `0x2baa4` is the race; `0x2bd7f` plays a replay back (recorded steps, and it draws
  between two frames: `0x16878`'s second argument), `0x2c132` is the mode of `0x130d10 & 4`.
- **A frame, for one car:** the wheels (`0x19cac`) move the four corners (`+0x514`, floats)
  from last frame's (`+0x770`) by their speeds; `0x1c7f8` sets the place to the corners'
  mean with the fraction **cut off** (`0x4ea12` sets the FPU to chop: its control word's high
  byte is 0x1f; then `fistp`); `0x1d9fc` rebuilds the corners from that whole-number place;
  `0x1e529` measures the corners' speeds as (corner − last frame's) / step, and the car's
  speed and direction of travel (`+0x500`, `+0x508`) from place − last place.
- **A wheel keeps** `1 − grip × step` of its sideways speed (`0x19c61`; grip = the road kind's
  × the car's `+0x74` × the wheel's, less under power at the driven end) — about 6 % lost a
  frame at 0.02 with the accelerator down. Slides are meant to last.
- **Together:** what is cut off is up to one unit a frame per axis, always towards the map's
  origin (0, 0) — a step of +0.3 towards it becomes a whole unit, a step of −0.9 away from it
  nothing — and it comes back as speed. Sideways it settles at about (cut off) / (grip ×
  step²): measured on level ground of kind 36 (`OS_S2FLAT=36 OS_S2AT=x,y,heading`,
  `OS_S2STEP`), units a second after 3 s:

  | step | at (50000, 50000) facing 0.785 | facing 0.3 | at (50000, −50000) facing 0.785 |
  |---|---|---|---|
  | 0.02 | 528 sideways | 436 | does not leave the grid (0 km/h) |
  | 0.033 | 182 | 87 | drives |
  | 0.04 | 124 | 233 | drives |
  | 0.05 | 99 | 144 | drives |

  At (−50000, 50000) facing 0.785 (both axes towards the origin) there is no slip at any
  step and the car gets away fastest. Rounding to the nearest instead (an experiment only)
  turned track 3's slip round. Track 6's grid at 0.02: the car crept sideways at 5 km/h.
- **In the port** (`main.c` `s2_step()`): the race frame is run **25 times a second** and the
  cars and the camera are drawn between the last two frames (`s2_race_step`, `s2_race_show(t)`,
  `s2_camera_show(cam, t)`). `OS_S2HZ=5..50` to try another; `OS_S2RUN` / `OS_S2PHYS` count
  these frames now. The port's own choice — the game has no one step — and **Danik to judge**
  against the original: on track 3's start the port still slips about 270 units a second
  (4 degrees at 45 km/h) at 25; if the original does not, it runs slower than that or
  something is still not carried over right.

## S2-5m. Cars that touch: the solver's answer, and statements out of order — 2026-10-13

Danik: after the start, as the opponents line up, the second and third stop dead for a while
and then go on; rear-ending one sometimes locks both cars in place. Two faults of the
conversion, neither the game's:

1. **`0x245a3` leaves its answer in ST0** (`fld [ebp-0xc]` before the epilogue): how much of
   the step was done before two cars touched, 2 = none did. The decompiler had it returning
   its (unused) register arguments, so the frame (`0x2baa4`) never saw a touch: the solver
   had already flagged the two cars (`car +0x4c | 8`), pulled their places back to the moment
   of contact and set their speeds, and the frame's second pass — the flagged cars done again
   over the REST of the step — never ran. `tools/s2_sigs.txt`: `0x245a3 0 ->float`. A scan of
   all 65 converted routines for an `fld` before the epilogue with no float result declared
   finds no other.
2. **The decompiler prints some statements out of order** (`tools/s2order.py`, its header has
   the instructions). The compiler inlines vector arithmetic as copies between stack
   temporaries, the last of them read through a pointer kept in another slot; Ghidra shows
   that read as the variable but prints the store it depends on LATER in the block (once in
   the header of a following `for`). In `0x245a3` the cars' outlines — the wheel rectangle
   (832 × 422) scaled 1.4 about its centre — were scaled about a centre whose height and y
   were stale: cars "touched" 1900 units apart, which is the line-up after the start. The
   rule: a local assigned ONCE in the routine and read earlier in the same block gets its
   assignment moved up to before the first reader, when all it uses is set by then; what
   cannot be moved is reported. It moves 16 statements in three routines — `0x245a3` (10),
   `0x20a18` (4), `0x22d35` (2), all of them the two last coordinates of a `× 0.25` centre —
   and none in the car's own physics. `s2c.py` applies it to every routine it reads and
   prints the count.
3. On the way: **the arc cosine `0x4ea98`** is π/2 − atan(x / √(1 − x²)), and with 1 − x²
   negative its square root reports a domain error and the arc tangent is skipped. Plain
   `acos` gave a NaN heading for a cosine of 1.0000001, and `0x2d59c`'s "bring the angle
   under 2π" loop never ended (track 4, frame 548 with the grid hold). `s2_acos` in
   `s2_mem.h`: 0 out of range.

After: a parked car is hit at 1190 units nose to tail (1.4 × 832 = 1165), pushed, and let
go; 80 set-ups of a parked car in the opponents' way and 120 of the player driving into them
(`OS_S2AT`, `OS_S2LOG=1`: every frame, every car's place and speed, `touch` = `0x11cdb4`)
leave no car standing and no two locked; two minutes on each track with the grid hold
(`OS_S2HOLD=1`) end with every opponent running, 0–41 frames with a touch.
Opponents do touch each other and the rails now and then — whether as often as in the
original is Danik's to say.

## S2-5n. The level: the opponents' handicap — 2026-10-13

Danik could not keep up with the opponents. They had been running with no handicap at all:
`0x2da06` was hooked away, and once carried over a comparison in it was converted wrong.

- **`0x130d4c`** = the level: **0 pro, 1 normal, 2 rookie** — the row of `SKILLS` and of
  `DAMAGES` (`MISC.DTR`, loaded to `0x11f0f4`, 7 floats a row, and `0x11cce0`).
- **`0x2da06`** (every frame, from `0x2baa4`): (a) each car's place in the race, `+0xc30`, by
  its distance covered `+0x4b8` (ties broken by car number; cars flagged `+0x4c & 2` are
  out of it); (b) while the whole seconds of the clock `0x13335c` divide by 8 (and the mode
  `0x130d10 & 0x70` is 0): the mean of the HUMAN cars' `+0x4bc` (a car is human when `+0xb4`,
  its controls record, is set); each other car gets its handicap **`+0x48`** = 0 before the
  sixth second, then the level's row read at `3 + 3 × (its +0x4bc − that mean) / 200000`,
  kept within 0.01 … 5.99 and taken between two entries — the row's seven columns are "+3 …
  0 … −3", behind → level → ahead (rookie 1.8 2.0 2.2 2.4 3.1 3.6 4.5, pro 0.4 … 2.7). With
  no human car: the row's fourth entry. The driver `0x17726` slows by it (`+0x48 × 0.3 ×
  …`, and brakes when `speed × +0x48 × 1.5 / …` is over its mark).
- The conversion: its `0.01 … 5.99` is a double the decompiler keeps in two halves
  (`tools/s2c_fix.txt` `@2da06`). And **`(int)DAT_0013335c < 0x40c00000`** — a float's BITS
  compared, "clock < 6.0" — was written as the float cut to a whole number, always true, so
  the handicap stayed 0: `s2c.py` had the rule for locals only, now for the program's own
  floats too (four places: this, two in the driver `0x17726`, one in `0x4b016`).
- In the port: `s2_level` (`s2_hooks.c`), **rookie unless `OS_DIFF=0|1|2`** — the port's
  choice until there is a menu. Opponents' mean over two minutes with the player parked,
  track 1: 115 km/h with no handicap, 88 in rookie, 97 normal, about 102 pro; 82–93 on the
  seven tracks in rookie.

## S2-5o. The wheels and the body on its springs — 2026-10-13

What `0x16878` hands the engine for a car's blocks each frame (block records of 0x20 bytes:
`+0x08 +0x0c +0x10` three angles in 2048ths, `+0x14 +0x18 +0x1c` its place), and what the
port draws from it (`s2_car_state`, `car_pose.per_wheel`, `s2_race_show`):

- **The four body blocks** (`car +0x1f8 +0x200 +0x208 +0xbf4`): angle `+0x08` = `car +0xc18`,
  `+0x10` = `car +0xc20` — the body's own nose-up and lean on its springs — and height =
  the block's own (`car +0x214…+0x220`) + `car +0x4f4` (the lift; 16 at rest). → the port's
  `body_pitch`, `body_roll`, `body_drop = −lift`, same senses as the car's angles.
  They are SMALL: `0x1c4fb` makes them 2 × atan(difference of the wheels' spring heights /
  wheelbase or track), eased by table entry `car +0x30 × 68 + 30`; a spring's travel is 30.
- **A wheel** (record `car +0x574 + 0x70 n`; two blocks, `+0x78` and `+0x80` — the port goes
  by the block flags as in Screamer: 0x800 = the hub that turns): place = `+0x38`, `+0x3c −
  +0x48`, `+0x40` (so `+0x48` = how far the spring is pushed in; −16 at rest); the turning
  `+0x10` (less `speed / +0x5c × π` a frame; the driven pair by the engine's speed when
  slipping) on the first block; the steering `car +0x13c` (eased towards π / (4 × `+0x6c`) ×
  the input: 0.785 at full lock) on wheels 0 and 2. Wheel 0 = front +X, 1 = rear +X, 2 =
  front −X, 3 = rear −X.
- Signs: the game's steering is + towards +X, like its heading (the left key raises both);
  the port's wheel turns its front edge towards −X, so `steer = −`. Checked from the front
  quarter with each key (`build/png/s2_wheels.png`).
- Not done: the shadow (`car +0xbf8`, an object of its own, `0x4b843`), smoke, the brake
  lights and other flag bits of the object record `+0x20` (0x400 / 0x8000 wheels turning
  fast, 0x200 braking).

## S2-5p. The shadow; material routine 208 — 2026-10-13

- **The shadow** is `ASHADOW.3DX` of the track's picture container (one quad 792 × 1534, the
  car's size, seen from above), its material in the library **`MISC.MLB`** of the same
  container (`0x2f519`: `0x4feb4("misc")` then `0x4b843("ashadow")` → `car +0xbf8`, an object
  of its own). Each picture `0x1f4a8` puts it at the car's place moved 30 units away from the
  camera, at the ground's height under the car (`car +0x4f8`) − 2, with the car's heading
  (`0x16878`) and the ground's slope along and across the car (`car +0xc0c`, `+0xc14`). The
  port: on the ground + 4 (its depth test's way, as in Screamer), no 30-unit move.
- **Routine 208** (new in Screamer 2: the shadow and the two smokes of `MISC.MLB`). The
  table of routines is at `0x75ef0` (`call [type + 0x75ef0]` at `0x7770a`, `[type + 0x75ef4]`
  at `0x7784a`): 208 is set up as a textured face (`0xc90a8`, like 16 and 64) and its row
  ends in the pair 224's does (`0xc73fc`, `0xc7310`) — texels are STRENGTHS looked up
  through the SMG table, which is what the port's 224 does; the port reads 208 as 224.
  (Not read to the end: where the second pair of a row is called from.)
- **`MAPn.SMG` is 48 rows** here, two ramps: rows 0–31 and rows 32–47, each from "as it is"
  to darkest. The shadow's texels are 32 (edge) … 41 (middle); the smokes' (materials 112,
  144, six frames each) 0…31 and 48….
- The shadow is faint and lies almost wholly under the car (869 pixels of a 640 × 400 side
  view differ, `OS_NOSHADOWS`): `build/png/s2_shadow.png`. Whether the original's is stronger
  is Danik's to say.

## S2-5q. The race screen — 2026-10-13

`0x45882` (after the pictures, `0x2b873`) draws it; the port's is written by hand from it
(`main.c` `s2_hud_draw`), reading what it shows out of the game's memory (`s2_hud`).

- **Files:** `GAME1/GAMEL.FNT` / `GAMEH.FNT`, `GAMEL.SPR` / `GAMEH.SPR` — Screamer's formats
  but for the font file's head: the first byte is the number of fonts itself (9; Screamer's
  is one less), and each font has 32 bytes — rows, first letter, letters, a fourth number
  (19 for 18 rows; unread) and the picture's name (`font2l.pcx fnum2l fgearl font4l fnum3l
  fcdl fnum1l font3l font3_2l`). 115 sprites (`OS_S2SPRITES=n` with `OS_S2RUN` draws them
  from n on, numbered): 0–3 the start lamps' panel, 4–9 the cars' marks, 10–25 the CD
  player, 26 the dial, 27–32 the bend arrows, 37 the bar, 60–65 small cars, 66–69 "3 2 1
  GO", 70–109 gear lamps, 114 "1st".
- **Text** `0x50f45(0x12420c, x, y, text, font, colour, SPACE)`: the seventh argument is what
  is left between two letters (−1 for the small font 3, 1 or 2 for figures); `0x51020` the
  same text's width; `0x3327c(text, y, font, colour, space)` centred; `0x50c0a(…, x, y, n,
  0)` a sprite, `0x333c1(n, y)` one in the middle. `hud_text` got the space (`g_hud_space`).
- **What is where** (320 × 200), one player:
  - `LAP` (8, 2) font 3 colour 3; `"%d%c"` = min(`car +0x4ac` + 1, laps `0x130d48`) and
    ':' (the figures' font draws it as a stroke) at (8, 11) font 1; the laps at (its width +
    5, 20) font 4.
  - with more than one car: `POSITION` right-aligned to 312 at y 2, `car +0xc30` at (286,
    11) font 1, sprite 0x72 at (270, 4) for the leader; the bar `0x456fb`: sprite 0x25 at
    (108, 2), every other car's mark (sprite 4 + its team) at 160 − 50 × d − 4, d = (the
    player's `+0x4bc` − its) / 200000 within ±1, the player's (sprite 9) at 156.
  - `THIS LAP` (8, 56) and `car +0xc3c` (8, 64); `TOTAL` (8, 72), `car +0xc38` (8, 80);
    `BEST LAP` (270, 56), the text at `0x132fa4` (270, 64); `BEST TOTAL` (270, 72),
    `0x132fb0` (270, 80) — the bests are `0x130d18`, `0x130d14`, 3599.999 = none, written by
    `0x201ee` through the time routine `0x37e06` ("%02d:%02d:%02d", each cut off; hooked: its
    sprintf's arguments are on the stack). Times begin with the letter 1 = "fixed width".
  - the checkpoint's message `0x132f98` centred at y 35 while `0x133078` > 0 (font 8 colour 3
    if its second letter is '+', else font 7 colour 1); `BEST LAP!` at y 15 blinking while
    `0x13307c` > 0; `WRONG WAY` at y 15 blinking while `car +0x1f0` is −1.
  - the start: sprite 0x42 + whole seconds of the clock, in the middle at y 60, for the
    first four seconds (3, 2, 1, GO; the cars are let go at 3).
  - the instrument `0x46b76`: sprite 0x1a at (238, 132); the speed "%03d" at (267, 177) font
    2 colour 5 space 1 = `car +0x500` × 1.513 eased by entry 0x32 (÷ 1.6 when `0x124654`);
    the gear `car +0xc7c` at (299, 177) font 2 colour 4; the rev needle from (271, 168), 30
    long, colour 0x5b, at revs / most revs × π − 2 radians (`0x49071`, Screamer's needle).
- Laps: `OS_LAPS` (3) → `0x130d48`. An opponent's first lap of track 1 in rookie: 112 s.
- Not done: the bend arrows `0x454fc` (the list at `0x12734c`, `MAPnDIR.SPL` by the look of
  it), the dashboards of views 2 / 6 (`0x46589`, `COCKn`), the mirror, the end of the race
  and the other modes' screens (`0x47040`), the box under the bar (`0x51698`).

## S2-5r. The trackside cameras; the end of a race — 2026-10-13

- **View 1** (`0x13223`, now converted): `0x14acf` picks the camera of `CAMERASn.MCM`
  (`DATA/MAPn`: a count, then 0x20 bytes each — place, three angles, kind `+0x18`, a figure
  `+0x1c`; loaded to the camera record's `+0xa60` / `+0x60`, `0x14d00`) nearest the car, taking
  only the list's next or last one unless none is chosen yet; by its kind: 0 `0x13514` as it
  stands; 1 `0x135b6` at its place, moving along its own heading at figure × the car's speed ×
  111, looking at the car; 2 `0x13768` between its place and the car (0.8 − 0.75 × figure of
  the way), looking at the car; 3, 4 `0x138e8`, `0x13a8e` unread. "Looking at" = `0x338ed`
  (written by hand: its arc cosine's argument is on the FPU stack — length on the ground /
  length). `OS_VIEW=4` for shots; `build/png/s2_trackside.png`.
- **A car that has done its laps** (`0x201ee`): `car +0x4c |= 3`; the best total `0x130d14`;
  the player's camera is given the car again (`0x12d00`), view 1 (`0x12d31`) and flags 0xb,
  the view it had kept in `0x130d68`.
- **The end's screen** (`0x47040` → by the mode `0x130d10 & 7`: 1 `0x4746a`, 2 `0x47e75`, 4
  `0x48980`; a plain race's read): for four seconds (`0x133050`) `CONGRATULATIONS !` at y 70 if
  first, "%s %s" at y 100 (taken as the place's word and `POSITION`), the total time at y 130,
  all font 0 colour 1 space 2. Then the track's name (sprite 0x27 + track) in the middle at y
  4, a band `0x51698(0, 180, 320, 20)`, and the choices at y 184 through `0x332dd(text, y,
  width, x0, font, colour)` (centred in `width` from `x0`): REPLAY / RACE AGAIN / EXIT in
  thirds, or RACE AGAIN / EXIT in halves when there is no replay; the chosen one (`0x13306c`)
  blinks font 8 colour 3, the others font 7 colour 1; left / right (`0x131f20` 4 / 8), ENTER
  (0x10). And the track's table of fifteen best times (`0x12b18c…`, names, cars), scrolling.
- In the port (`s2_hud_draw`): the four seconds, the name, RACE AGAIN / EXIT (the band
  darkened by half — what `0x51698` does is unread), left / right / ENTER; RACE AGAIN starts
  the same race again (`os_go`), EXIT leaves. Not there: the replay, the table of best
  times. `OS_S2ENDTEST=<seconds>` shows it in a shot; `build/png/s2_end.png`. Not driven to a
  finish by me: what the car does after the line, and RACE AGAIN itself, are untested.

## S2-5s. What happens at the finish, read and checked — 2026-10-13

Danik: "check what the camera does when the race finishes and be sure to match it."

- **In the game** (`0x201ee`, the player's car on its last line): the car is flagged `+0x4c |=
  3` and given `+4 = (cars − place) × 0.2` seconds; the view it had is kept in `0x130d68`; the
  camera gets the car again (`0x12d00`: no trackside camera chosen yet, so the NEAREST of all
  is taken, afterwards only the list's next or last), view 1 and flags 0xb.
  - Flag 2 of those stops the view keys (`0x12dff` calls `0x132b4` only while it is clear).
  - The car is the game's driver's from then (`0x19409`: no controls record OR `+0x4c & 1`),
    and once `+4` has run out (`0x1f3ce`) its throttle `+0x1ec` is 0 and its brake `+0x1e8`
    1: it stops. The winner drives 0.6 s longer than the last of four.
  - Kinds of trackside camera now read: 3 `0x138e8` circles the car at 1100, its height 440 ±
    220, both turning by the camera's figure; 4 `0x13a8e` rides beside the car at a fixed
    offset (1320 + 500 × figure back, 550 + 200 × figure up) turned by the camera's heading.
  - The race screen is not drawn in view 1 (`0x45882`, `0x46b76`); the end's screen is.
  - At night the fade is 0 … 27000 in view 1 (`0x12d31` → `0x11373c`, `0x113738`).
- **In the port** all of that is the game's own converted code; three things were the port's
  and are put right: `s2_view` refuses while the game has the view locked; the picture is not
  blended across a CUT from one trackside camera to the next (`s2_camera_state.shot`, as for
  a change of view); the fade's distances are read from the game's two words each picture
  instead of being fixed per condition.
- **Checked** with `OS_S2ROBOT=<frames>` (the game's driver has the player's car for so many
  frames, then the keys) and `OS_LAPS=1 OS_S2LOG=1`: track 1, the line at frame 2420 — view
  1 on that frame with camera 22 riding beside the car; the car slows 128 → 0 km/h in 80
  frames (3.2 s) and stands; a cut to camera 0 at frame 2445, which follows it to where it
  stops. `build/png/s2_finish.png`.

## S2-5t. Stand-ins and guesses gone through against the code — 2026-10-13

Danik: "replace anything temporary or previously guessed with the code-correct one, if it
has been found."

**Replaced:**
- **The ground under a point** was Screamer's routine (any winding, the face's stored
  normal). Now the game's own, `s2p_ground` in `main.c` from `0x28ce6` / `0x28db7` /
  `0x4c24c` / `0x4c504` / `0x4c7de` (its comment has the whole of it): whole numbers; nine
  cells in the order of `0x112e84`; a piece when the point is in its first block's circle;
  the point turned into the piece's frame and tried up to five times, 4 further along both
  axes each time; a face holds it by ONE of its triangles (0 1 2, else 0 2 3), one winding
  only; the plane is that triangle's own, from its corners; the normal goes back to the
  world unturned. All seven tracks run as before (opponents' means within 2 km/h).
- **The darkened band and the bar's box** (`0x51698`): not "half black" but a table of the
  palette built by `0x5105e` — each colour's nearest to 0.67 of it, or to 0.7 of that again
  where the nearest is the colour itself. `s2_dark_box`. The bar now has its box (108, 4,
  104 × 7).
- **The view a race starts in** is the options' (`0x128a68`), and the options start (`0x32d53`)
  with **2**, the first view from the car — not 3. (`0x2e1f3` takes 2, 3, 4, 6; else 2.)

**Read and found right as they were:**
- the lens: `shl 7` before the divide (`0xc9266`), 128 as Screamer's; the picture's middle
  and eye line from `0x4f6c4` (half the width; 0.65 of the rows);
- "%s %s" at the end = the place's word (`[0x124584 + 4 × place]`) and `[0x124598]`
  POSITION; the lap's "%d%c" letter is 0x3a ':';
- the easing tables `0x315a4`: (0.5 + 0.005 i) ^ (50 / (row + 1)) — and the game reads row
  `whole(1 / step)`, one past the rate it was made for; the port fills and reads the same;
- `0x11253` = square root, 0 for a negative; `0x50d04` = the sprite routine `0x50c0a` with
  its last argument 0;
- three laps (the options' `0x128a30` = 1 of the table 1, 3, 5, 10 at `0x1131fc`), rookie
  (option 0 of `0x1130c0` = level 2), four cars, the six settings 2 2 −1 2 2 2 (`0x339e7`).

**Still the port's own, with no answer in the code:** the 25 steps a second (S2-5l); the
shadow laid 4 above the ground instead of the engine's order of drawing; that EXIT leaves
the program (no menu yet). **Still not read:** what the arc cosine gives out of range
(the library's error routine `0xbb541`; taken as 0); where routine 208's second pair of the
table is called from (S2-5p); the font file's fourth number; the speed in miles
(`0x124654`); the camera's lean and the wheels' turning senses are settled by pictures, not
from the engine's matrix code.

## S2-5u. The driving checked against the program's own machine code — 2026-10-13

Danik: the original is easier to handle — more responsive to the steering, harder to spin
half round in a bend, faster without losing the car. "Make sure all driving code is read
and implemented properly."

- **`tools/s2emu.py`** runs the race frame of the PROGRAM ITSELF (`0x2baa4`, then `0x12dff`) in
  a CPU emulator (Unicorn, x87 and all) from the memory the port had at the start of a test
  race, with the same keys, and compares the player's car's record (0xCC0 bytes) after every
  frame with the port's. The test world is the port's `OS_S2FLAT=<road kind>` and
  `OS_S2NONEAR=1` (level ground, no road points) so that only the car's own routines are
  compared; `OS_S2DUMP` writes the block, `OS_S2TRACEFILE` the record each frame,
  `OS_S2PLAN="A:3,AL:0.6,A:0.6,B:1"` the keys by seconds. What the emulator answers itself is
  what the port answers itself. (Needs `pip install unicorn capstone` in a venv. The block
  starts empty in the port, so the program's data object is kept under it. The library's
  copy and fill load ES, which Unicorn does not survive: done in the script.)
- **Result:** after one fault was put right (below) the two are THE SAME TO THE UNIT — place,
  heading, speed, slip — through 4 s of acceleration and a full-lock spin (201 frames, the
  rear-drive car), and through accelerate / turn / straighten / brake with the four-wheel-
  drive car (132 frames). Only whole numbers one apart now and then (x87 against C floats).
  So the car's routines are converted right, and **the game's own code spins the car** when
  full lock is held a second at 115 km/h under power.
- **The fault it found:** `0x1c4fb`'s `(int)local_4c[i] < 0x3f800000` — an array element's bits
  compared ("this wheel's share of the weight is under 1") — was taken as a whole number:
  always true, so the springs of the wheels carrying MORE went up instead of down and the
  body's pitch and lean (`+0xc18`, `+0xc20`) stayed nought. `s2c.py` has the rule for array
  elements now. It changed nothing in how the car goes (the spring heights are only shown).
- **What was different from the original was what the port fed in** (`0x339e7`, the menu
  filling an ordinary race in, and `0x32d53`, the options as they start):
  - the player's car: every team's choice starts as **car 1**, not 0; the others get the next
    cars and teams round. `CARS.DTR`'s `TRAZIONE`: car 0 drives the REAR wheels (0 1), cars 1
    and 2 ALL FOUR (0.5 0.5), car 3 the front (1 0). A wheel's grip is less by throttle ×
    its share × 0.3 (`0x19cac`), so the rear-drive car lets go at the back under power. Left
    held 1 s at 115 km/h under power, then let go: worst slip 77° (car 0), 49° (car 1).
  - the player's six settings come from the options' first set-up, 2 2 **0** 2 2 2: tyres 0 =
    slicks (the port had −1 = the track's own for everybody; the other cars keep that);
  - the mode word `0x130d10` = 2; the controls record as `CONTROLS.CFG` has it.
  `OS_CAR=0..4` still chooses the car.
- **Also made the game's own:** the nearest road point / wall (`0x2a371`, `s2p_nearest`): only
  the nine grid cells round the point, rows then columns then stored order, squares of whole
  numbers, the first of equals, none = none (it was the nearest of the whole track).
- **RACE AGAIN** gave the part the program's arguments instead of its own two (the track's
  directory and number): `s2_race_again`; `OS_S2AGAINTEST=<pictures>` takes it by itself.
- **Not the same as the original and the port's own:** the keys are read once a picture, the
  race frame runs 25 times a second and the picture is drawn between the last two frames —
  up to a tenth of a second from key to picture. `OS_S2HZ` changes the rate.
- **Still to do with the rig:** slopes and walls (the ground and wall hooks against the
  program's own, which need the track loaded the engine's way), the opponents' driver, cars
  touching. And the end's screen for mode 2 is `0x47e75`, not the `0x4746a` the port's was
  written from: unread.

## S2-5v. Which block of a car is what: by the game's numbers, not by where it is — 2026-10-13

Danik, with the default car now car 1: "the car body rotates when I accelerate" — the body
shell stood on end and turned with the wheels.

- The port took a car's block for a wheel when its place was off the middle, and for the
  part that spins when its flags had 0x800 — Screamer's rule, and true of Screamer 2's car 0
  by luck. In the other models a body block is off the middle too.
- The game finds the blocks BY NAME when it sets a car up (`0x2f519` → `0x4bc03(name, to)`,
  the block's number to `to + 4`): `fauto` → `car +0x1f8`, `rauto` → `+0x200`, the name at
  `0x110d98` → `+0x208`, `MV_mauto` → `+0xbf4` (the body's four, which get the body's lean
  and lift); and per wheel the two names of the table at `0x112f00` → `wheel +0x50` (turns
  with the wheel and steers) and `wheel +0x58` (steers only).
- The port now carries those numbers (`s2_car_state.body_block`, `.wheel_block`) into
  `car_pose.role[block]` and the drawing goes by them; wheels 0 and 2 steer, as `0x16878`
  has it. All five models checked pulling away with lock on: `build/png/s2_models.png`.
- My miss: the default car was changed without looking at it moving.

## S2-5w. THE ORIGINAL ITSELF AS THE MEASURE — 2026-10-13

Danik, after S2-5u / v: "cornering is still not as forgiving as the original, and steering
does not feel as responsive; OS_S2HZ makes no difference."  The converted routines were
right against the program's code (S2-5u), so the question was what the original is GIVEN.

**The rig.** `tools/s2dos.py <prefix> <plan>` runs the original in DOSBox on a hidden display
(`Xvfb :77`), as its own child so that it may read `/proc/<pid>/mem`; goes through the menus
with ENTER (nine of them: team, name, options room, ARCADE, England, the car "HORIZON 4wd",
AUTOMATIC → the race); finds the game's data object in DOSBox's memory by a string of it
(`misc\0ashadow`); waits for the player's race (the clock starting again after the attract
race's); then holds the plan's keys (`"Up:3.9,Up+Left:1,Up:1.6"`, seconds of the RACE'S
clock, XTEST) and reads, every game frame, the clock `0x13335c`, the step `0x133350`, the raw
time `0x133354`, the six controls `0x127708 + 0xa8`, and every car's place, angles, speed.
A pointer of the game's is turned into the image's addresses by `[car +0xb4] − 0x127708`.
Read 15 ms after the clock moves: the clock goes on at the frame's START and a read during
the frame's sums is half old, half new. Needs `python-xlib` (keys), and the DOSBox flatpak
given `--socket=x11 --env=DISPLAY=:77`. `tools/s2replay.py <prefix>` then gives the port the
same steps and controls frame by frame (`OS_S2SCRIPT`, `OS_S2CLOCK`) and prints both.

**What it showed, in the order found:**
1. The original runs at **17.7 pictures a second** in this DOSBox (60000 cycles: a Pentium of
   its day): 0.0565 s a picture, **step 0.062** = that × 1.1 (`0x4ae9a` eases 1.1 × the time
   measured) — its race runs a tenth faster than the wall's clock. The race's clock goes on
   by the raw time `0x133354`, the routines use the eased step. (`s2_hz`: a frame of 1.1 /
   17.7 every 1 / 17.7 s; `OS_S2HZ`.)
2. **The race record in its memory** (an arcade race from the menu's defaults) was not what
   the port set: mode `0x130d10` = **1**; every car's settings **2 2 −1 2 2 2** (the port had
   given the player slicks, 0, last round from the menu's code for ANOTHER kind of game:
   grip 1.1 against the track's own tyre's 1.4 on England's asphalt, and the drive is limited
   by grip — the original's car pulled away 1.4 times as hard); with the mode's bit 1
   `0x14e20` leaves the computer's cars' settings alone as well (the port had them boosted
   to 4: max steer 1.3 against 0.91); `+0xc` of the others' entries 0; `0x130d54` = 1,
   `0x130d64` = 1, `0x130d6c` = 4; level `0x130d4c` = **1**; view 3 and one lap in Danik's
   saved options. `CHOICE.DAT` is the options block `0x1289ec` (0x81dc bytes), each word
   `(w ^ i) − 0x1050c × i` (`0x32d53`).
3. **The track's set is H**, not L: the cell at the first wall held four `V309`, which is
   `MAP1H01`'s (`MAP1L01` has `V261` there). Known since S2-5d ("L when detail is 0, H
   otherwise") and never done — the port took H only for the 640-wide picture. The two sets
   are different objects on the road itself. `OS_S2SET=L` for the other.
4. **The wall test's "is there ground under this corner"** is `0x28ce6(1)` → `0x4c474`: any
   face holding the point, a face on edge too, no plane (`s2p_ground(only_whether…)`); the
   pieces are walked by the cell they are LISTED in (`scene_object.cell`).
5. **Which corner's push the wall gives the car** (`0x297a6`): the one with the greatest
   |push x| + |push z|, the sums handed on in EDX / EBX through the library's `abs`
   (`0xafeca`). The decompiler's reading compared two leftovers: always true, so the LAST
   corner off the ground won — with two corners through a wall the car was pushed back by
   the lesser, went on into it and bounced where the original slides along. Rewritten from
   the instructions (`s2c_fix.txt @29571`).
6. **The game's sums are x87 at 53 bits** (its runtime loads the control word 0x127f,
   `0xbb603`): every step of an expression in double precision, cut to a float only when
   stored. The converted routines were computing float expressions in floats, which decides
   differently which whole number a place is cut to. `s2_hooks.c` is now compiled
   `-mfpmath=387 -fexcess-precision=standard` and `S2_FPU_IN` sets the same control word
   (x86 only — other processors: not done). `tools/s2emu.py` took the wrong word (0x37f) at
   first; with the right one and `--hooks` (the port's answers about the track replayed to
   the program's code) a four-car race on the real track stays within a few units for ten
   seconds, and the player alone through repeated wall hits is the same to the unit.

**After all six: the player's car in the port is the original's TO THE UNIT** through the
recorded run — the grid, the start, 1 s of left lock, into the barrier at 55 km/h, the slide
along it, away again (114 frames; `build/` holds nothing of it, the recordings are scratch).
The other cars are the same until about the 96th frame and then up to 570 apart: not looked
into (the random numbers' seed is the port's own).

**Also:** the level is 1 by default now, as the original's arcade race (`OS_DIFF=2` =
rookie). Track 7's shadow shows as a yellowish patch with the H set — to look at.

## S2-5x. The shadow again; a picture of the original — 2026-10-13

- `build/png/s2_original_road.png`: the original 5 s into the arcade race on England, view 3
  (taken by `tools/s2dos.py`, its `_end.png`); beside the port's at about the same moment:
  `s2_original_vs_port.png`. In it: the smoke off the tyres at the start, see-through by
  degrees; a soft dark shadow under each car; "1ST" drawn over the leading car; the race
  screen as the port has it.
- Track 7 has no `MAP7.SMG` and the program has no name for an `.smg` file at all, so it must
  make its blend table itself (`TRACK_DATA`'s "asmoke (rgb)" / "a2smoke" are likely what
  from). Not read. The port goes on using the files where they are and makes the shadow's
  rows (32–47: 1 − 0.045 a row) itself for track 7, which showed the shadow as a yellow patch.
- TRIED AND TAKEN BACK: "routines 208 / 224 darken everything under the face through the 0.6
  table" (the column loop at `0xc9030` uses that table, `0xc907c`). It gave a hard-edged dark
  rectangle the size of the car; the original's shadow is soft. Which faces that loop draws
  is not read.

## S2-5y. The lens, measured against the original's picture — 2026-10-13

- `tools/s2dos.py` reads the player's camera record too (`[0x1192f4]`: place `+0x48`, angles
  `+0x54`, rows `+0x1c`): in view 3 the port's is the original's to the unit and the fourth
  decimal for the whole run (1660 551 23186, 6.0092 0.0379 0 on the grid).
- So the port can draw the original's very moment: `OS_S2RUN=1 OS_S2SCRIPT=<prefix>.script
  OS_S2CLOCK=<clock> OS_VIEW=3 … --shot` plays the recorded race, then takes the picture.
  Against the original's own screenshot of that moment the port's was the same scene but
  LARGER: original = 0.871 × port + 43 rows of 400 (the START gantry, the car's roof and its
  rear bumper). → Screamer 2's lens is **112** where Screamer's is 128 (0.875; 112 = 0.7 × 160,
  as 128 = 0.8 × 160) — taken as measured, the engine's projection is not read. The eye line
  measured 0.65–0.675 of the rows; a search over both (difference of the blurred scenery) is
  flat between 109–112 and 0.65–0.67, so the eye line stays the code's 0.65 (`OS_EYE`,
  `OS_FOCAL` to try others).
- The engine's camera: `0x7e8b4(camera + 0xa64)` → `0x7f270` makes the matrix from the three
  angles through the sine / cosine table at `0xb7480` (8 bytes an angle of 2048: sine, cosine,
  16 bits) and `0x759b8`. Not carried over; the port's own turn-by-heading-then-tilt agrees
  with the original's picture.
- The speed shown is in MILES when `GAME.TXT`'s word for the unit begins with an M (`0x2ef4e`
  sets `0x124654`; the dial then shows the eased `+0x500` × 1.513 ÷ 1.6): the original read 054
  where the port read 086.
- `build/png/s2_original_vs_port.png`: the original (left) and the port (right) at the same
  frame of the same race. What the port still lacks there: the smoke, "1ST" over the leader.

## S2-5z. Two cars touching in the air: a value copied, not converted — 2026-10-13

Danik: a hit on an opponent while both were in the air gave a wild spin. Rebuilt with
`OS_S2AIR=height,speed,gap,turn` (car 0 lifted, car 1 lifted behind it and thrown at it;
with `OS_S2FLAT=36 OS_S2NONEAR=1 OS_S2CARS=2`) and run against the program's own code
(`tools/s2emu.py ... --cars 2`): the game left car 1's tilt and lean near 0, the port had
1.1 and 5.3 and the heading turned over.

The cause, at the end of `0x245a3` (the cars' collision): for each of the eight corners
the game reads the corner's UPWARD speed (`+0x70` from the corner's place, car `+0x548`
`+0x554` `+0x560` `+0x56c`), adds the hit to all three speeds, and then puts the upward
one back: a hit between cars never changes how fast a corner rises or falls. The
decompiler writes that read as `local_2c = (float)*(undefined4 *)(...)`: four bytes of no
known type "as a float", which in the machine code is a plain copy. Taken as C it is a
conversion of a whole number, so the bits of the speed became a speed of about 3.3e9. On
the ground the upward speed is 0, whose bits are 0: that is why it only showed in the air.

Fix in `tools/s2c.py`: `(float)*(undefined4 *)` is a read of a float. One place in the
converted set; `(float)*(int *)` is a real conversion and stays. Three more of the kind,
`(float)DAT_0004cbxx` in `0x28f96` / `0x2c475` / `0x30b29`, read stored zeros: harmless.

After it the nine frames of the test agree with the machine code (loose comparison: one
word, car 1's lean 0.015 against -0.006 in one frame). The recorded run of the original
(`s2replay.py r5`) and Screamer 1's pictures are as before.

## S2-5za. Where the game rounds: every store of a float, shown by the decompiler — 2026-10-13

Followed from the opponents leaving the original's recorded paths (S2-5w's open point). Not a
random seed: the same cars went the same way for a hundred frames and then parted by one unit.
The close comparison with the machine code (`s2emu.py --tol 1e-6 --cars 4`) showed real
differences in the fifth figure, of three kinds, all the converter's.

1. **The game's square root (`0x11253`) lost its fraction.** Its result sits in eight bytes on
   the stack that the decompiler leaves untyped (`undefined8`); the converter made them a whole
   number. Every distance in the opponents' path code (`0x108f5`, `0x153c6`, `0x2d59c`, ...) was
   cut. Rule now: eight untyped bytes in a routine's stack frame are a double.
2. **The roundings the decompiler hides.** The code keeps a sum on the processor's stack at
   full width (53 bits here) and rounds when it stores it into a float, often into a temporary
   that the next instruction reads back (`fstp dword [t]` / `fld dword [t]`). The stock
   decompiler prints the pair as one expression: the rounding is gone and the text cannot say
   where it was. `0x4a831` (a point and a direction on a path) takes the direction as the
   difference of two nearly equal sums, each rounded first: without the rounding it was off by
   5e-5. **Fix at the source:** `tools/ghidra-ext/install.sh` now builds a copy of the x86
   language (`x86f.sla`, from `ia.sinc` with four constructors changed) in which `FST` / `FSTP`
   to a 32- or 64-bit place write `f32(value)` / `f64(value)`, operations the decompiler cannot
   fold away. The existing project takes it as it is (no re-import). `tools/s2c.py` turns each
   into a cast, which under x87 arithmetic with `-fexcess-precision=standard` rounds there and
   nowhere else. What had to follow:
   - `DecompTo.java`: a local, an array or a double that such a store fills is typed float /
     float[] / double (the operation's result carries no type);
   - a temporary left untyped that only such stores fill becomes a float, and where it is
     copied on its bytes go (`S2_SETF`), whatever the decompiler calls the destination;
   - under a mark, untyped things are floats: `*(undefined4 *)`, an untyped local or
     temporary (`S2_ASF`, or through `MP()` for a stack local), one of the program's own
     variables, `(&local)[i]`;
   - `f32(E);` alone is a store whose reader the decompiler lost (the float arguments of
     `pow`, which `tools/s2c_fix.txt` supplies): dropped;
   - `tools/s2c_fix.txt`: the patterns no longer name the decompiler's temporaries.
   The text of `s2_gen.inc` does not settle from one decompile to the next (temporaries are
   typed a few more each time and renumbered); what it does is the same.
3. **`(float)` before one of the program's variables that the listing gives no type** is a
   read of a float, not a conversion (the same family as S2-5z): the checkpoint times at
   `0x130d1c` (compared with a car's own in `0x1ffcc`) and a field of the smoke records at
   `0x125dd8`. Both were read as whole numbers.

Also seen on the way: `local_114[0] = (float)(local_28 * 4)` in the old text of `0x153c6` was a
whole number stored in the first place of a table; the new text has it as an int of its own.

**Against the references.** The machine code (Unicorn): a four-car start on track 4 now differs
in 4 words of 4 cars' records in the first frame, all in one float's last place (a heading; the
emulator's own sine and arctangent are not the processor's, so that is the floor of this
reference). The original in DOSBox: recordings `r6` and `r7` (England, 89 and 88 frames) are
now the same for ALL FOUR cars in every frame; in `r5` opponent 1's creep on the grid is gone
and the first difference is one unit at frame 71 (opponent 2); track 4 (`t4b`, 840 frames):
first differences at frames 131, 145, 153, one unit each (before: 96). The player's car as
before. What remains is one unit once in a hundred frames a car; not found whether it is the
port's or DOSBox's arithmetic (its sine against this machine's).
Seven tracks, four cars, 900 frames each: no stall. Screamer 1's pictures as before.

## S2-5zb. The dashboard of view 6 — 2026-10-13

Only view 6 (F2, the second from the car) has a dashboard; view 2 is the plain one with the
dial. From `0x46b76` (the race screen), `0x46589` (the drawing), `0x2e1f3` (the loading),
`0x13cbd` (the view), `0x2ddea` (the table):

- **Which:** `0x124574[player]` = the car's model (race entry `+4`); a model 4's is its team
  `+ 4`. Picture `COCK<n + 1>_L.PKC` (`_H` in 640) of `GAME1.DAT`, Screamer's `.PKC` format,
  kept at `0x1242a0`. `MIRROR_L.PKC` (`0x1242b0`) is laid over every view but 1 when the
  mirror is on (`0x11f0c4`): not done.
- **The table:** `CRUSC.DTR`, section `CRUSCOTTI`, 0x58 bytes a dashboard at `0x1242b4` (four
  "Car", then "Bon"), places in the 640-wide picture's units and halved by the low-res
  program: `+0` the rows of the scene; `+4` the speed's needle: x, y, length, angle at
  nought, angle at the most, the most; `+0x1c` the revs' needle: x, y, length, two angles;
  `+0x30` five (x, y), the gear lamp's place by gear.
- **The scene's rows:** view 6 sets camera `+0x1c` to the table's first number / 2 (127 to
  137) and `0x12dff` gives `0x4f6c4` those rows with the eye line at 0.65 of them: the scene
  is drawn only above the dashboard and the horizon sits lower than in the other views.
  (View 5, the mirror's, has 35 rows.) The port: `s2_camera_state.rows` → `g_vh`, `g_cy`.
- **Drawn** (`0x46589(speed, revs, gear)`, the speed `0x133098` = the dial's eased figure,
  the revs `0x1330a0` = the eased part of the most): the picture; sprite `0x46 + 5 × n +
  gear` at the gear's place; each needle at `nought − part × (nought − most)` radians
  (under 0.001: 0), anticlockwise from the right, its tip `length / 2` away, as three lines
  of colour 12 to the tip from the centre and from one pixel either side. No dial, speed
  figure or gear figure in this view; the rest of the race screen as in the others.
- The port's own: black where the picture is open below the scene's last row (a few pixels
  along its upper edge). Picture: `build/png/s2_dashboard.png`.

## S2-5zc. The bends' arrows; "1ST" over the leader — 2026-10-13 / 14

- **The arrows** (`0x454fc`, called from the race screen `0x45882` while the player's car
  has not finished and the view is not the trackside one). The list is `MAPnDIR.SPL`
  (`"%sdir"`, loaded at `0x30b10`, its entry's address in `0x12734c`): nodes of 56 bytes, `+4`
  x, `+8` y, `+0xc` which arrow (England: 11 nodes). `0x133080` = the next node, from 0.
  When the car is within 80 of it (distances × 0.01: 8000 units) the arrow shows for one
  second (`0x133090`, counted down by the frame's raw time), `0x133088` = its number, and the
  node after becomes the next, round the list. Drawn as sprite `0x1b +` number, centred, row
  0x32. In the port: `s2_bend()` in `s2_hooks.c` once a game frame, `s2_hud_state.bend`.
  Picture: `build/png/s2_bend_arrow.png`.
- **"1ST" over the leading car** -- FOUND 2026-10-14, after a long search in the wrong
  places (it is sprite 0x26, not 0x72, and it is drawn by neither the race screen nor the
  engine): **`0x4962c`**, called from the picture routine `0x2b873` right after the scene
  (`0x2b6b4`) and before the mirror and the race screen, in every view. For each car other
  than the player's: in the ordinary modes (mode `& 0x30` clear) when mode bit 1 is set and
  the car's place (`+0xc30`) is 1, the point 500 above the car's place (`[car +0x990]`) goes
  through `0x492d1` -- minus the camera's place, through its matrix (`camera +0xaac`, nine
  whole numbers / 16384), shown only when the depth is between -128 and 20000, divided by
  depth + 128 and multiplied by 128 -- and sprite 0x26 is drawn at (x + 150, y + 0.65 × rows
  - 12), clipped to the scene's rows (view 6's are fewer). In the other modes (`& 0x30`: two
  players?) every other car gets sprite `0x21 +` its team and a name instead. The port:
  `s2_hud_state.leader`, drawn at the start of `s2_hud_draw` through the port's own lens so
  that it sits over the car as drawn. Danik: in the original it is over whoever is first at
  any moment. Picture: `build/png/s2_first_marker.png`.
- **`AZTRI.3DX` / `AZ1TRI.3DX`** are not markers (two triangles 28 wide and 24 tall, back to back, of `MISC.MLB`'s plain materials 1 and 0; that file's materials 2, 3 are the birds, 4-6 the cones): `0x30db4` makes 30 objects of each, the 60
  records of 0x3c bytes from `0x125dd8` that `0x2aeb9` sets off at the place of a collision
  (`0x245a3` calls it with the point, the direction and the speed): the sparks. Not drawn
  in the port yet.

## S2-7. THE GAME'S ROUTINES RUN FROM THE PLAYER'S OWN PROGRAM — 2026-10-14

**DISCARDED 2026-10-15** (Danik: "we should discard the interpreter build, i don't like this
approach. When everything is done, we will rewrite manually any routines so that we conform to
the law AND don't need the original binary or any interpreter"). `s2_cpu.c` / `s2_cpu.h`, the
interpreter's hooks and `-DS2_INTERPRETER` are removed; a build without `src/s2_gen.inc` has no
Screamer 2 (`s2_available()` 0, `-game 2` says so and ends; checked: it builds, and Screamer 1
runs). What stays of this section: `s2_exe.c` (with `s2_exe.h`), the loader of the player's
`STARTL.EXE`, which the game's menu still uses for the program's data object and the engine's
variables in its code (`s2_code_var`) -- to go with the hand rewrite. The rest below is the
record of what was built.

**Why.** The race routines converted to C (`port/src/s2_gen.inc`, S2-5g..) are the game's
program in another notation: not ours to publish, in source or in a binary. Danik's decision:
the port carries nothing of the program and runs the routines out of the player's
`STARTL.EXE` (the port needs the player's game files anyway).

**What there is now.**
- `port/src/s2_exe.c`: `<game's directory>/STARTL.EXE` → the flat layout its addresses mean
  (`s2_flat`, 0x210000 bytes): the LE objects at their bases, the relocation records of kind 7
  applied. `tools/unle.py --fixups` in C; `OS_S2EXEDUMP=<file>` writes it, and it is
  `build/screamer2_flat.bin` byte for byte. One version only: image 0x139c90 bytes, FNV-1a
  sum 0x0ee6b6c1 (the file is 1,055,478 bytes); another is refused with its numbers printed.
- `port/src/s2_cpu.c`: an interpreter, 386 flat 32-bit + 387. What the routines execute was
  counted first (the rig, `EMUINS=<file>`: 18,445 instruction addresses in three varied
  four-car races): moves, the eight arithmetic ones, lea, imul / idiv, shifts, push / pop /
  call / ret, the conditional jumps, and of the 387 loads, stores, the four operations and
  their reversed and popping forms, compare + `fnstsw ax` + `sahf`, fild / fistp, fchs fabs
  fsqrt fsin fcos fpatan fprem frndint, fldcw / fnstcw. The interpreter has those and the
  usual rest (set-on-condition, movzx / movsx, string moves, mul / div, ten-byte loads);
  anything else stops the routine and says where.
- **The 387 is a double.** The game sets the control word 0x127f: every result rounded to 53
  bits. So a register is a C double and the host's arithmetic gives the same bits on any
  processor (`-ffp-contract=off -fno-fast-math` for that file). The control word's rounding
  is honoured for conversions to whole numbers (the game switches to cutting off). Sine,
  cosine, arc tangent are the C library's: the last bit may differ from a real 387's.
- **Hooks** (`s2_hooks_set()` in `s2_hooks.c`): a call to a hooked address runs the port's
  function instead, with eax / edx / ebx / ecx and the stack as the routine would find them,
  then the routine's own `ret n`. The same list the converter had, less what now simply runs
  as the game's code: the angle of a vector `0x10d9e`, the look-at `0x338ed`, the power
  `0xafed2`, abs `0xafeca`. The arc cosine `0x4ea98` runs too, unless its number is out of
  range (then 0, as before).
- The block: the flat memory itself; the data object starts as the program has it (the
  converted build started from zeros plus three small tables). The port's heap from
  0x140000, the routines' stack down from 0x20f000.
- `s2_call(entry, eax, edx, ebx, ecx, n, stack)`: what `s2_hooks.c` calls `0x2baa4`,
  `0x12dff`, `0x2f519`, `0x309b0`, `0x12d31`, `0x1d9fc`, `0x4b016` through.
- **The converted routines are still the measure, for the developer only:** the build with `s2_gen.inc` (see "later" below) (ignored by git now, and to be
  taken out of the local commits that hold it before anything is pushed).

**Checked.**
- Against the converted build, frame by frame, four cars (the scratch script `ab.sh`): the
  same until cars touch or a long way in (England: frame 67, a touch, 102.1 against 102.0
  km/h; track 4: frame 106, 0.1 km/h) -- the two do sine, cosine, power and arc cosine with
  different last bits.
- Against the ORIGINAL's recordings (S2-5w): as good as the converted build or better -- `r6`
  and `r7` the same for all four cars in every frame; `r5` first difference frame 71; track 4
  frames 131, 153, 169 (converted: 131, 145, 153).
- Seven tracks, four cars, 1200 frames each: no instruction unknown, no stall. The five
  views, a robot's lap, the mid-air crash: run.
- **Speed.** A four-car frame is about 234,000 instructions (66,000 of the 387). On this PC
  6000 frames take 4.6 s in the interpreter against 0.6 s converted: 0.77 ms a game frame,
  17.7 of them a second. Nothing done for speed yet (flags worked out at every instruction,
  everything decoded every time). For the MiSTer's ARM my estimate is 10 ms a frame, a fifth
  of one core, before decoding once and keeping the result: NOT MEASURED there.

**2026-10-14, later: NOT THE DEFAULT.** Danik tried it: the frame rate on his Ryzen 5 dips
from 180 to 90, and he wants 60 a second on the MiSTer -- "a no-go". So the build uses the
converted routines again wherever `port/src/s2_gen.inc` is present (CMake says which at
configure time); the interpreter is what a copy of the source WITHOUT that file builds, or
`-DS2_INTERPRETER=ON` (`port/build_interp` here, for comparisons). The file stays out of git.
The question of what may be published is open again: with the file left out, the public
source builds the slow way. Not explained: a game frame measured 0.77 ms in the interpreter
headless, which is not a halving of 180 frames a second -- where the rest goes was not looked
for. What would be fast AND publishable: the routines rewritten by hand (as Screamer 1's),
checked one by one against these two builds; or the interpreter's step on a thread of its
own; or translating the program to native code when it is loaded.

**The swings in the frame rate were the main loop's, not the routines'** (found the same
day, when they stayed after going back). The loop ended with "if this frame took under 4 ms
there is no vsync: wait 8" -- written with 60 Hz screens in mind and timed in whole
milliseconds. On Danik's 180 Hz screen a frame is 5.6 ms, and any that came back a little
early was made to miss the next one or two: 180 one moment, 90 the next, with nothing going
on, in both games. Now: the frame's time from the fine clock (also `dt`), and the wait only
under 2 ms. `OS_FRAMELOG=1` prints once a second the frames and the port's own work on one
(without the wait for the screen). On this PC, headless: a Screamer 2 race 0.7 ms a frame
(1.6 ms in high resolution), Screamer 1 the same; with the interpreter 1.2 ms mean, 2.7 ms
the longest. So the interpreter was judged on a fault that was not its own -- its cost here
is half a millisecond a frame. For the MiSTer the estimate stands (not measured).

**Not done.** The start-up check does not ask for STARTL.EXE yet (a race says so when it is
missing); Windows build not tried with these files; the MiSTer's build compiles everything
with `-ffast-math`, which this file overrides for itself -- to be looked at when the MiSTer
is.

## S2-5zd. Smoke and sparks; the see-through table is made, not read — 2026-10-14

- **The see-through table** (routine 208's, S2-5p). The program reads no `.SMG`: `0x2ee76`
  (race set-up) calls `0x504f2(palette, asmoke r, g, b, a2smoke r, g, b)` and `0x508be`, which
  fill the table at `[0xbf3d0]` through `0x503a1(palette, to, r, g, b, rows)` -- for each of
  the 256 colours the palette's nearest (first of equals, sum of the three squared
  differences, 6-bit values) to the colour moved towards (r, g, b) by `row / (rows - 1)`:
  rows 0-31 towards `asmoke` (TRACK_DATA's rgb / 4; England 225 225 225), 32-47 towards
  black (the shadow), 48-79 towards `a2smoke` (England 52 149 54), 80-95 brighter (× 1 +
  row / 15, at most 63), 96-111 towards white. (With all three of asmoke negative only the
  shadow's rows are made. There is more when `0x11374c` is set: not read.) The files'
  `MAPn.SMG` has BLACK ramps where the smoke's rows are: with it the smoke could not show.
  The port: `s2_blend_make()` in `main.c` (112 rows; `OS_S2SMGFILE=1` = the file as before).
  Track 7's stand-in rows are gone with it.
- **Smoke** (`0x204af`, from `0x1b36f` every frame for a car on the ground; only at the top
  detail, `0x130c54 == 2`). Every 0.05 s, for the two REAR wheels: when the wheels slip
  (`car +0xc34 < 0.93`, argument 1) or the road kind's flag 4 is set and the speed is over 20
  -- by the kind's flags `& 3`: 1 = a puff of `asmoke` (car `+0x228`, eight records of 0x28
  bytes), 2 = of `a2smoke` (`+0x368`). The first free record (`+8`) is switched on, its
  animation set to 0 (`0x4c15f`) and its place taken from the wheel (`+0x18..0x20`; for the
  car the chase view follows it is kept relative to the car and turned with it, `0x2073c`),
  moved by the kind's "dispersion" × the low three bits of the car's place. `0x2073c` (every
  frame) puts the object there and moves its animation on by step × 1.5 (`0x4c0cf`: the
  object's `+0x20`, 0..65535; at the end the puff is switched off): a puff lasts 2/3 s.
  The objects are made in `0x2ef4e` (`0x4b843` by name, sixteen a car) -- which the port did
  not do: every puff pointed at object 0. `ASMOKE.3DX` / `A2SMOKE.3DX`: one square 588 × 568
  that faces the screen, materials 7 and 9 of `MISC.MLB`, six frames, texels = rows of the
  table (0-31 / 48-79). The port: `0x204af` runs (no longer hooked), `0x4c15f` / `0x4c0cf`
  are hooks in `s2_hooks.c`, the objects are made in `s2_race_start`, `s2_puffs()` →
  Screamer's sprite objects in `main.c`. All four cars smoke off the line:
  `build/png/s2_smoke.png`. `OS_S2PUFFLOG`, `OS_S2PUFFTEST`, `OS_NOSMOKE`.
- **A puff shows whole** (Danik: "the grass particles are much more visible in the DOS
  version"). The game draws its objects back to front, each whole: a puff, whose middle is
  at the wheel's foot, shows its lower half too, over the road in front of it and over the
  car's tail. The port's depth test hid that half under the ground -- the thicker one.
  Now a puff is tested as if twice as near (`zbias` 2; `OS_S2PUFFBIAS`): the road under it
  never hides it, things well in front still do. The port's own, by necessity (depth
  buffer). Checked against the original at the same moments of a recorded run (`s2dos.py`
  with `Up+F4:0.3,Up:8.5`, chase view, on a 1920 x 1440 hidden screen so that the whole
  window is caught): the white haze off the line and the green blades on England's grass
  look alike: `build/png/s2_grass_original_vs_port.png`. That run -- off the road and along
  the grass at 100 mph, 153 frames -- is also THE SAME as the original's for all four cars
  in every frame: the driving on grass had not been compared before.
- **A carried puff goes where the car is DRAWN** (Danik again: "still not as visible as in
  the DOS"). The port draws a car between two of the game's frames; the puffs of the car the
  chase view follows are kept relative to that car (`0x2073c`), and the port placed them from
  the game's LAST frame -- up to a frame ahead of the car as drawn, 900 units at 100 mph: on
  the car's tail instead of behind it, and smaller. A still picture (`--shot`) never showed
  it, being of one frame. Now `s2_puff.carried` and the drawn car's place. What the engine
  does with such an object (`0x7eacb`: flag 0x8000 of its `+4` -> `0x7f1a8`): `0xae8c8` gives
  it the camera's three angles (heading + half a turn), `0x759b8` makes the matrix, which is
  then turned inside out -- it faces the screen, as the port has it. Tried against the
  original's picture and NOT the cause: the picture upside down, twice the size, lying
  flat. Measured in the two pictures at 640 x 480: the same number of dust-coloured pixels
  (1456 against 1532) of the same colours; the original's sit about 20 pixels lower.
  `OS_S2PUFFQUAD=1` shows each square whole.
- **Sparks** (`0x2aeb9(place, direction, .., .., speed, which)`, from `0x245a3` when cars
  touch (which = 1: records 0-29, `AZTRI`) and from `0x29571` at a wall (records 30-59,
  `AZ1TRI`)): `speed × 3600 / 400000 × 30 / 60` of them, at most 30, each from the place +
  100 up with the direction × 0.9 + chance (± 1500 along and across, 1000-3000 upwards),
  three chance angles, two seconds to live. `0x2cc74` -- called by the main loop `0x2b9b1`
  after the race's frame, which the port now does too -- moves them: 7848 a second squared
  downwards, the angles turned by the speed, the object placed. Sixty records of 0x3c bytes
  from `0x125dd4`; objects from `0x30db4`. The triangles are 28 × 24 units: two or three
  pixels at the chase view's distance. `s2_sparks()`; `OS_S2SPARKLOG`.
- **Two more faults of the converter, found by the sparks** (the interpreter build had them
  right, which is how they were told from the game's own doing): `place = floatvar;` into
  a place without a type stored a whole number (rule: the float's bytes go, `S2_SETF`); and
  `(int)(&DAT_x + n)` for a four-byte variable stepped n bytes, not n fours (two places,
  both in `0x2aeb9`).
- **Seen in `0x2b9b1`, the frame of the game's main loop, and NOT run by the port yet:**
  `0x2c475` (after the race's frame; uses the ground and the walls: the obstacles of
  `OSTACLES`, by the look of it), `0x2cee4` (the track's animations, with their sounds),
  `0x2a639(car)` for every car within 20000 of the player's (sound). The port's moving
  scenery in Screamer 2 is still Screamer's way.

## S2-5ze. The obstacles (cones) — 2026-10-14

Three of the game's routines, none of which the port ran; all three are now run as they are
(converted, or by the interpreter) and the port only draws what they leave.

- **Set-up** (`0x30a4e`, `0x30b29`). `MAPnDAT.DTR`'s `OSTACLES` = rows of three names (12 bytes
  each) at `0x126be4`, how many rows in `0x124660`; when there are any, `MAPnOST.SPL` (`"%sost"`)
  is the list of places (`0x124658` its entry, `0x12465c` set): a node's `+4`, `+8` the
  place, `+0xc` which row. `0x30b29(1)` makes a record of each -- 0x3c bytes from `0x124664`,
  how many in `0x126d4c`, laid out as a spark's (S2-5zd): place (floats: x, height, y), three
  angles, three speeds, `+0x28` the object (`0x4b843` by the row's name), `+0x30` "to be
  placed again", `+0x34` flying, `+0x38` -- on the ground (`0x28ce6`), turned to the road's
  direction (`0x2a371`) and tilted to the ground's slope: level it stands at (pi, direction,
  pi). England 23 (a line of cones down the middle of the road at one bend), track 3 48,
  track 4 53, track 6 22; tracks 2, 5, 7 none.
- **A car against them** (`0x2a639(car)`, from the main loop `0x2b9b1` for every car whose
  |height difference| + |x difference| from the player's is under 20000): an obstacle within
  200 of the car's height and 2000 of it ... is sent flying (`+0x34`, `+0x38`) with a sound
  (`0xaea54`). Read only as far as that.
- **Flying** (`0x2c475`, every frame): as a spark -- 7848 a second squared downwards, the
  angles turned by the speed -- but it bounces: off the ground (0.7 of each speed, upwards
  again; under 300 it stops and lies along the ground's slope) and off a wall (turned about
  the wall's direction, half the speed). Then the object is placed (`+0x30` cleared: that
  flag is NOT "shown" -- taken for it, the port hid every cone after the first frame).
- **Drawn**: `AZCONE.3DX` (the name's first letter, `s`, is the table's mark for a name),
  materials 4-6 of `MISC.MLB`. Its middle is at the ground (the model is 318 high, from -159
  to +159): the game draws objects whole, back to front, and the cone shows whole; the
  port's depth test would bury half of it, so the road does not hide an obstacle
  (`scene_object.over`, the smoke's rule). The angles go to the port's as (first - pi) =
  tilt, second = heading, (third - pi) = lean: right for a standing cone, not checked for a
  tumbling one. `s2_obstacles()`; `OS_NOPROPS` leaves them out. Pictures:
  `build/png/s2_cones.png` (standing; knocked).
- The engine's list holds 200 objects (`0x4bd1b`); a four-car race uses 132 (4 cars, 4
  shadows, 64 puffs, 60 sparks) + the obstacles: 185 on track 4. The port's pool of drawn
  objects is 256 now (`MAX_OBJS`).
- Still not run of the main loop's frame: `0x2cee4`, the track's animations and their sounds.

## S2-5zf. Sound in the race — 2026-10-14

The race routines were already calling the game's sound manager; the port answered with
silence. Now the six calls drive the port's mixer (`s2_hooks.c`, "SOUND"), so what sounds, when
and how is the game's own doing.

- **The manager** (`0xae..`–`0xaf..`, an object at `[0x124274]`, over HMI's digital driver:
  `0xd05e1` start, `0xd075f` stop, `0xd029a` rate, `0xcffb4` volume, `0xd019e` pan, `0xd040c`
  which sample a voice plays). 64 slots, one sample at a time each:
  `0xaea54(m, sample, slot, rate, volume, replace)` start (rate 0 = the sample's own; volume
  0 = its own, 1 = silent); `0xaebf0(m, sample, slot, rate, volume)` the same unless the slot
  already plays that sample; `0xaee18(m, slot, volume)`; `0xaee98(m, slot)` stop;
  `0xaf45c(m)` forget the distances; `0xaf168(m, record)` a sound heard from a distance.
- **`0xaf168`**, record at `0x124280`: slot; the listener's x, y (the camera's `+0x48`,
  `+0x50`); its heading - pi; the source's x, y; the range (50000 for an engine); a pitch.
  d = the distance (`pow` by 2, read from the machine code: the decompiler loses its
  arguments). Beyond the range: silent. Within: rate = the sample's × pitch², + that × (last
  d - d) / range × 5; volume = the sample's × ((range - d) / range)²; pan = 32768 - 32767 ×
  cos((1 - across / range) × 1.87), across = the listener-to-source difference along the
  heading's (cos, sin). (A source dead ahead comes out at pan 42435, right of the middle:
  the game's 1.87, kept.)
- **What the race plays:** each car's engine in the slot of its number (`0x1f756`: sample =
  its model, or team + 4 for a model 4; + 0x19, the `T_` samples, on road kinds 7, 13, 34,
  43 -- tunnels; pitch = revs / most revs × 2, so the rate runs from near nothing to four
  times the sample's); the tyres in slot car + 4 (`0x1b36f`: `0x12 +` and `7 +` the road
  kind's two numbers, volume by the slip); in slot 4 the knocks: 0xc-0xe a wall (`0x29571`),
  0xf-0x11 car against car by how hard (`0x245a3`), 0x10 an obstacle (`0x2a639`), 0x12 a
  landing (`0x1bef1`). The track's animations' sounds (`0x2cee4`, slots 7 up, the ambient
  samples) are not run yet.
- **Samples**: `SOUND/GAME.DTR` (0 .. 0x20) and `AMBIENT<track - 1>.DTR` (0x40 ..): name,
  number, loop length, loop count (-1 round and round), volume of 0x7fff; rate (8000 to
  22050), bits, channels, format.
- **The port's own:** the level against the mixer's scale (`s2_sound_gain` 0.5,
  `OS_S2SNDGAIN`: four engines and a skid clipped at 1); the mixer's 3.2 kHz filter and its
  pan law, Screamer's. Measured only as numbers (`OS_SNDWAV`: continuous, no clipping, the
  loudness following the race); how it SOUNDS is Danik's to say. `OS_SNDLOG`, `OS_NOSOUND`.

## S2-5zg. A plain menu for now; the animations put off — 2026-10-14

- **`s2_menu()` (`main.c`), the port's own**: with no track on the command line Screamer 2
  now opens on a list in the race's font -- TRACK (the seven names of `LOCAL/STRINGS.TXT`),
  CAR 1-5, DAY / NIGHT / FOG, OPPONENTS ROOKIE / NORMAL / PRO, 1 / 3 / 5 / 10 LAPS; arrows,
  ENTER starts, ESC leaves. The choice goes to the race through the test switches
  (`OS_TRACK OS_CAR OS_COND OS_DIFF OS_LAPS`), which also keeps it; ESC in a race and EXIT at
  its end come back to the list. A headless run or one with a test switch that picks the
  race (`OS_S2PHYS OS_S2ROBOT OS_S2SCRIPT OS_SNDWAV OS_FRAMELOG OS_SNDLOG OS_NOMENU`) goes
  straight to the track as before. `OS_S2MENU_SHOT=<file>` = its picture. The game's own menu
  (3D scenes, `.BKK`, `SCENEn.PTH`, `0x32xxx`-`0x44xxx`) is not read.
- (The animations: done the same day, S2-5zh.)

## S2-5zh. The track's animations and their sounds; the mirror — 2026-10-14

- **Paths: `MAPn.PTH`** (`0x30ec2` reads it, `0x11c0c` a path): a count, then for each path
  100 bytes (name at `+8`, `MV_<c>...`: the character c is its number, `'1'..'9'` = 0..8,
  `'a'..` = 9..) and three channels -- x, y, height -- each 0x30 bytes (`+0x1c` how many
  keys) and its keys of 0x2c bytes: `+8` time (0..1), `+0xc` value, `+0x10` / `+0x14`
  slopes. Between keys p and q (`0x126ad`) the value is the cubic through p.value, p.value +
  p.slope × h, q.value - q.slope × h, q.value, h = (q.time - p.time) / 3; past the last key
  it is taken 0.001 before it. `0x129d5(path, part)` gives the place and the way it is going.
  England: 17 paths, twelve of birds and five that only mark a place.
- **Rows: `ANIMATIONS` of `MAPnDAT.DTR`**, 0x4c bytes each at `0x126d5c` (0x28 of them from
  the file): name (`@name` = a thing of the track itself: only a sound at its path's first
  place), `+0xc` how long a run takes, `+0x10` when it started, `+0x14` the distance at
  which it starts (-1: 52000), `+0x18` flags (1 it starts when the player comes that near, 2
  once only, 4 it faces the screen -- `0x4c080` at set-up -- and is not turned along its
  way), `+0x1c` its path, `+0x20` on which lap (the player's lap + 1; 0 any), `+0x24` its
  sample, `+0x30` where it is.
- **Every frame (`0x2cee4`)**, only at the top detail: a row is alive when the player is
  within 52000 of it both ways (or it has neither flag 1 nor 2) and the lap fits. A started
  row's part = (the race's clock - its start) / its time, taken round and round, or once
  (flag 2: then it is gone; flag 1 without 2: it waits to be set off again). Its sound: slot
  7 + row -- started silent when the row comes alive, then every frame heard from the camera
  (`0xaf168`, range 80000, pitch 0), stopped when it goes.
- **In the port** this routine and its set-up (`0x31060`) are written out in C on the game's
  own records (`s2_anims_start`, `s2_anims_step` in `s2_hooks.c`; the paths in C tables) --
  not run: the game builds its paths with a module of its own. `s2_anims()` → objects drawn
  between two frames in `main.c` (`A1BIRDS.3DX`, `A2BIRDS.3DX`: squares that face the
  screen, their wings the texture's frames). England: twelve flocks that rise as the player
  comes within 30000-46000, each on one lap (1, 2 or 3), with the birds' sample; the mill,
  the horses and two crowds as sounds. A pitch of 0 makes `0xaf168`'s rate 0: what HMI does
  with that is not read, the port keeps the sample's own rate. `OS_NOANIMS`. Picture:
  `build/png/s2_birds.png` (the flock is small and white, left of the middle).
- **The mirror** (`0x2b873`, `0x13e1a`, `0x754e0`). With `0x11f0c4` set and the view not the
  trackside one the game draws the scene again from its third camera record (`0x1192fc`),
  view 5: the car's place (x `+ car +0xc00`), 400 above the body, heading + pi, the tilt
  and lean turned over, 35 rows, without the car and its shadow (`0x2b6b4`) -- into a
  buffer, and copies it to the screen at (80, 14) taking every other column from right to
  left: 160 × 35, squeezed and turned round. `MIRROR_L.PKC` (kept at `0x1242b0`) is laid
  over it by the race screen. The port: `s2_mirror()` gives that camera; `s2_render()` in
  `main.c` draws the mirror's scene first into the top rows, keeps it, draws the picture,
  lays the strip and the frame over it. Off to begin with, as in the original's pictures;
  **R** turns it on and off, `OS_S2MIRROR=1` starts with it on (the game has it in its
  menu's options: not read where). It costs a second pass of the scene: 0.77 → 1.04 ms a
  frame here. Picture: `build/png/s2_mirror.png`.

## S2-5zi. Birds and cones seen through bumps and walls — 2026-10-14

Danik: "the flying birds and the cones can be seen behind road bumps and walls." Both came
from the rule that the road does not hide what stands in it (the smoke's, S2-5zd; given to
the cones in S2-5ze): it held for ANY piece of drivable track however far in front, and it
was tied to "faces the screen", which the birds do.
- The rule is now an object's own (`scene_object.over`), not every screen-facing one's: the
  smoke has it, the birds do not.
- It holds only while the road is no more than 1 / 0.45 times nearer (`OVER_LIMIT`): the
  ground a puff's lower half is under, not a crest or a bank in front of it.
- The cones no longer use it: they are STOOD ON the ground -- raised by what the model has
  below its middle, 159 -- and hidden by whatever is in front like anything else. In the
  game a cone's middle is at the ground and all of it shows (objects drawn whole, back to
  front), so the port's cone sits that much higher than the game's: its own, by necessity.
Screamer 1's pictures are the same (its smoke keeps the rule, with the limit).

## S2-5zj. The other tracks' moving objects; the kinds of game — 2026-10-14
- **The mirror's camera is drawn between the game's last two frames** like everything else
  (`g_s2_mir_was` / `g_s2_mir_is` in `main.c`): the game moves it 17.7 times a second, and
  Danik saw the steps. (Owed from S2-5zh.)
- **Every track's `ANIMATIONS`** (track 7 has none): 2 birds + a helicopter `aeli`; 3 a
  helicopter (from 10 s); 4 an airship `adirigib`, cable cars `acable`, bulldozers `adoz`;
  5 trains `a1trn` / `a2trn`; 6 a helicopter (from 60 s). Rows named `@...` are sounds only
  (crowd, market, traffic, works, wolves, cows). All objects and paths load; the limits
  (40 rows, 40 paths, 24 keys, 8 kinds of model) hold on every track.
- **Which way an object faces** (`0x2cee4`, flag 4 clear -- all but the birds). From the
  path's direction `way` (x, y, climb) the game puts two angles in the engine's object, in
  2048ths of a turn: `+0x18` = round, the angle of (x, y) -- the same as a car's heading,
  `atan2(x, -y)`; `+0x14` = up / down, the angle of the climb against what is left along
  the ground after turning back by the first; `+0x1c` = 0. The port had the first only;
  now both (`s2_anim.climb`, kept in the row at `+0x44`, the heading at `+0x3c`).
  Level is angle 0 for these (an obstacle's level is pi, pi).
- **The climb's sign** (2026-10-14, Danik: track 4's cable car "doing a wheelie" before the
  finish): the game's +0x14 is the port's pitch the other way round -- on the slope (climb
  0.187) the car leant against its rails by as much as they rise. `o->pitch = -climb` now;
  side views both ways agree with the rails. `OS_S2ANIMALL=1` (a test) starts every object as
  if the car were by it, so they can be looked at anywhere; the look prints the climb too.
- Seen from four sides each (`OS_S2ANIMLOOK=<row>,<degrees>,<distance>,<height>`; with a
  row that is not on it lists those that are): all upright, the helicopter and the
  bulldozer nose first along their paths. The helicopter's rotor is a second block with a
  picture of several frames: it turns with the texture clock, nothing else moves it (the
  object has no frames of its own, header `+0x10` = -1).
- The ambient samples start on every track (`OS_SNDLOG`).

### The kinds of game (the mode word `0x130d10`), surveyed, not built
Set in `0x339e7` by the menu's choice `0x1324dc`:

| choice | mode | cars `0x130c48` | laps `0x130d48` | what else |
|---|---|---|---|---|
| 0 CHAMPIONSHIP | 2 | `0x128a20` | by the league | the cars' set-ups from the CUSTOMIZE screen (`0x128aa4`, else 2 2 -1 2 2 2); weather drawn by chance per track (`0x11320c`: 0 -> 1, 1 -> 2, else either); level from the league's table `0x1130c0`; in league 4 of 4 the others drive car 4 |
| 1 ARCADE | 1 | 4 | menu | level = 2 - ARCADE ENEMIES SKILL; what the port runs now |
| 2 TIME ATTACK | 4 | 2 (the second is the ghost) | 3 | `0x130d50` = ghost on / off; the player's controls record copied for the ghost |
| 3 two players | 0x40 | 2 | menu | both players' teams, cars and controls; split screen |
| 4 (FREE RUN) | 8 | 1 | 1 | the race does not end on the laps (`0x201ee`); each lap's time is shown against the best five (`0x1ffcc`, `0x130d1c`) |

Other bits: 0x10 / 0x20 link and network, 0x80 the attract race (`0x3c9ac`), 0x100 replay.
Where the race looks at them (all decompiled in `build/decomp2`):
- **4 (time attack)**: 34 places. The race's set-up and end (`0x2e1f3`, `0x2f519`,
  `0x2ecb0`: the ghost's file), only the player's car knocks obstacles (`0x2a639`), no
  car-to-car collision or "1ST" marker, the race screen shows no position unless the ghost
  is on (`0x45882`, `0x456fb`), the end's screen `0x48980`.
- **2 (championship)**: the race is over for everyone when fewer than two are still running
  (`0x2b4c5`); the end's screen `0x47e75` and the points table (`0x49f8e`), `LEAGUE REPORT`.
- **8 (free run)**: above; the race screen's right-hand side differs (`0x45882`).
- **1**: only arcade's computer cars get the level's boost (`0x14e20`) and the catch-up.
A test: `OS_S2MODE=<word>` with `OS_S2CARS`. Modes 2, 4 and 8 run without a fault, but the
port's race screen and end are arcade's, and time attack's second car is not a ghost yet.
Not read: the menu's league and custom screens, the ghost's file, the records' file.

## S2-5zk. Time attack with its ghost, the free run, the best times — 2026-10-14
- **Two faults of the converter found on the way** (`tools/s2c.py`; both were in every build
  so far):
  1. `DAT_x._1_1_` -- "the second byte of this word" -- was converted as the whole word.
     `DAT_00130d10._1_1_ & 1` is the mode's bit 0x100 (a replay is running); read as bit 0 it
     was TRUE in every arcade race, so **the best lap and the best total were never taken**
     (`0x201ee`) and the view was not kept at the finish. Now `._o_n_` is n bytes from byte
     o. (Writes of `._0_1_` no longer clear the word's other three bytes either.)
  2. A whole number stored into one of the program's floats is its BITS. `_DAT_00133078 =
     0x40800000` (4.0 seconds) became 1082130432.0: **the gap to the leader at a marker, and
     "BEST LAP!", once shown never went away**, and the lap's list of a free run never
     cleared. Now such a store is a copy of the bits.
  `ab.sh` (interpreter against converted) is where it was: England frame 67, track 4 frame
  106; `r6`, `r7` still the original's to the unit, `r5` frame 71.
- **Time attack** (mode 4; `0x339e7` case 2: two cars, three laps, by day). The ghost is not
  a recording of places: it is **the second car, driven again** by the game's own routines
  from the best race's frames -- each the two times (`0x133350`, `0x133354`) and the six
  controls, 0x20 bytes (`0x4a59a` writes, `0x4a488` reads). The frame is `0x2c132`, not
  `0x2baa4`: the player's car as ever; the ghost's car run in its own recorded frames until
  its clock (`0x133364`) has caught the race's, then shown part-way through the last
  (`0x16878`). Variables in `s2_hooks.c`'s comment. All three routines are in
  `tools/s2_set.txt` now.
  - The two cars start SIDE BY SIDE (`0x2f519`: 1000 to either side by the car's number, no
    row behind), so the ghost must be the car it was; the file says which, the player gets
    the other, and they change sides whenever the ghost is beaten. `s2_me` / `s2_player()`:
    the port no longer takes the player for car 0.
  - The file also holds the race record's entry of the car (team, model, gearbox, set-up) and
    the player's CONTROLS record (0xa8 bytes: the routines read how each control is to be
    taken from it -- without it the ghost stood still).
  - Kept when the race is left with the player's car done and FIRST (`0x2b2a8`): the first
    race always, then only a race that beat the ghost. The port's file:
    `openscreamer.ghosts/s2attack<track>.rpl` (its own layout; 7000 frames at most, 6.5 min).
  - The race screen (`0x45882`, `0x456fb`): POSITION and the bar only while the ghost is
    shown (`0x11f0a4`). Only the player's car knocks obstacles (`0x2a639`).
  - With no ghost to show (none kept for the track, or GHOST CAR off) the second car is still
    in the race, standing on the grid: `0x2c132` takes its model and shadow (car `+0xbf0`,
    `+0xbf8`) out of the engine's list every frame (`0x4c01c`; `0x4bfbe` puts them back when
    there is one). The port drew it -- Danik saw "the ghost does not run" in a first race --
    and now hides it (`s2_car_state.shown`, 2026-10-14).
  - The recorder (`0x4a59a`) writes the six controls from the player's car's controls record
    (car `+0xb4`, `+0xa8`): a race driven by the game's driver (`OS_S2ROBOT` takes that
    record away) is kept but its ghost stands still -- a test's limit, not the game's.
  - The end (`0x48980`): TOTAL, the time, LAP TIME, the best lap, four seconds; then RACE
    AGAIN / EXIT and the list.
  - Checked: a run of 300 frames kept by force (`OS_S2GHOSTLOG=2`) and driven again as the
    ghost ends on the same unit (-34282 179090), from either side; the game's driver through
    three laps (`OS_S2ROBOT`) ends the race and the file is written.
- **A free run** (mode 8: TIME ATTACK's other choice on its car screen, `0x3d234` -- the
  texts "3 LAPS" / "FREE RUN"): the player alone, no end. A lap is timed at five markers A-E
  (`0x1ffcc`, `0x201ee`; car `+0xc44[5]`, best lap's `0x130d1c[5]`); the screen lists them and,
  beside each, + or - against the best lap's; four seconds after a lap ends the list clears
  (`0x13304c`). The screen's own share of it (it writes the running time into the car's
  record) is `s2_free_step`. The best lap's five stay until the menu (`0x3f7a3`): kept in a
  slot that outlives the port's parts.
- **The best times** (`0x353b3`; layout in `s2_hooks.c`'s comment): lists of fifteen (time,
  name of 3 letters, team, model) -- total times by kind of game, track and length of race;
  lap times by kind and track. A race starts with the lists' first as BEST TOTAL / BEST LAP;
  at its end every human car that finished goes in, and the end's screen shows the list,
  six rows, the player's row lit, moving up to it (`0x4746a`, `0x48980`). The port's file:
  `openscreamer.s2records`. NOT read: what the lists hold before anybody has driven (empty
  here), the name's entry (here "YOU"), the replay kept with each row.
- **The clocks' figures were too far apart** and BEST LAP ran off the screen: after a byte 1
  figures stand in columns of the widest letter plus the routine's "space" argument (-1 in
  the small font: 5 apart, as the original's picture has them), not plus 2.
- The plain menu's first line is the kind of game: ARCADE, TIME ATTACK (GHOST CAR ON / OFF
  in the skill's line), TIME ATTACK FREE RUN. `OS_S2GAME=1|4|8`, `OS_S2GHOST=0|1`; tests
  `OS_S2GHOSTLOG`, `OS_S2FREELOG`, `OS_S2ENDTEST` (the end's screen with the race as it
  stands).
- Not compared with the original running: the free run's and time attack's screens (read
  from the code only).

## S2-5zl. The championship — 2026-10-14
- **Read** (`0x431ca` LEAGUE SELECTION, `0x339e7` case 0, `0x47e75` the race's end, `0x43914`
  LEAGUE REPORT, `0x38540` the menu's round; tables and the weather's rule in `s2_hooks.c`'s
  comment):
  - four leagues of 4, 5, 6, 6 races on the tracks in the menu's order (`0x113224`: 0 2 3 4
    5 1 6); the others' level rookie, normal, pro, normal; in league 4 the three others all
    drive their team's FIFTH car;
  - the weather is drawn before each race from a seed of (race + 1) x a number taken as the
    league is chosen: leagues 1-4 have 0, 1, 2, 2 races not in daylight;
  - 10, 6, 3, 0 points by place (`0x113180`; the table has a row for every number of cars of
    one team, all the same). A player who leaves a race is put last;
  - the race's end: CONTINUE, and a box of the cars that have done their laps (place, name,
    car, time, points) -- the others' name is "CPU";
  - the league is WON by being first on points after its last race. That opens: the next
    league (`0x128a18`); one more track for the other kinds of game (`0x128a14` + 3 tracks of
    the menu's order: three as the game starts, the seventh only when league 4 has been won
    with all four teams); for league 4, the winning team's fifth car (`0x128aa0`).
  - In one-player games a race is NOT ended when the others have finished (`0x2b4c5`: that
    rule is the linked game's).
- **Built**: `OS_S2GAME=2`. The plain menu has CHAMPIONSHIP (the track's line becomes the
  league), a TEAM line, and the locks as above (`OS_UNLOCK=1` opens everything). A league's
  state is kept in a slot between the port's parts (`s2_champ_t` in `main.c`), what has been
  won in `openscreamer.s2champ`. After each race the report (`s2_champ_report`): the port's
  plain screen with the game's lines -- the game's own is its 3D menu's, with the teams'
  badges. The race's end screen is the game's layout.
- Tests: `OS_S2REPORTTEST=n` (the report after race n; 9 = after the last, won),
  `OS_S2LEAGUE`, `OS_S2SEED`, `OS_TEAM`.
- **Not done / not read**: the car's CUSTOMIZE screen (six settings a car, `0x128aa4`,
  kept per team and car: championship and time attack use them); the league's own grid
  order; what the game's best-times lists hold at first; two players on one screen (mode
  0x40) -- left with the multiplayer; none of this was compared with the original running.

## S2-5zm. Patches on the road: the slots' memory was freed between parts — 2026-10-14

- **Danik:** on California, flat light-grey squares on the road "all over the map", some with
  a picture in them, that the car bumps into as if it hit something; not in the original.
  Not reproduced by any fresh run (every headless test starts the program anew).
- **The fault:** `os_state_restore()` frees every block a part allocated with the port's
  `malloc` / `calloc` (they are `os_malloc`). What an `os_slot` keeps must outlive the part,
  but the three slots of S2-5zk / zl were made with the port's `calloc`: 5 (the free run's
  best splits), 6 (the best-times lists, 31.5 KB), 7 (the championship). From the second part
  on they pointed at freed memory that the next race had handed to its own things -- track
  objects, cars -- and were written: `k->track = track` at every race start, the whole
  records block copied back at a race's end, the league's state from the menu and the end.
  A track object is shared by all its placements, so one write into its corners or texture
  places shows at every place it is used. Seen directly: after RACE AGAIN the race started
  with BEST LAP / BEST TOTAL read from the freed block (-nan, 2 x 10^20 -> "00:00:00").
- **Mended:** `os_keep_calloc()` (port.c: the C library's own, never freed) for the three;
  slot 3 already used SDL's for the same reason. Rule: **what an `os_slot` points at is made
  with `os_keep_calloc`**.
- Not proved that the patches were this (they were never seen here); to be confirmed by
  Danik. To find such things next time: **F12 in a race also writes `shot_NNN.txt`** -- every
  object owning pixels of the picture, how many, its box on the screen, and what it is (track
  piece V-number, run, place, materials; or car, shadow, puff, spark, obstacle, moving
  scenery). Test switches added on the way: `OS_SKYMARK` (no backdrop but magenta: holes in
  the scene), `OS_TIEMARK` (pixels won on a tie of depth in magenta), `OS_MATDUMP=<routine>`,
  `OS_FLATDUMP=1` (pieces by material / small flat pieces), `OS_S2SPARKLOG=2` (every obstacle).
- Also learnt: California is track 4 (`MAP4`), third in the menu's order; its 53 obstacles
  are fences and cones away from the start; it uses only material routines 16 and 64.

## S2-5zn. The settings' first values, the car's set-ups (CUSTOMIZE) — 2026-10-14

- **Before there is a settings file** (`0x32d53`): every row of every best-times list is
  3599.999 by **"MIL"**, replay 0, `+0xb4` = the row & 3, `+0xf0` = 0; both players' names
  (`0x1289f4`, `0x1289f8`) "MIL"; laps index 1 (3 laps), level option 1, the volumes 0x7fff /
  0x7fff / 0x5fff; every team's chosen car (`0x128a9c`) 1; every set-up 2 2 0 2 2 2. The port:
  the lists' rows and `s2_name` (was "YOU", a guess) are these. The end's table (`0x48980`)
  draws every row's car picture (`0x2e` + 5 × `+0xb4` + `+0xf0`), named or not; rows "n. NAME
  time" (`"\x01 %s. %s      %s"`, one space less from 10 on). The others' entries are named
  "CPU" (`0x110fd2`.., `0x110fde`..).
- **The car's set-ups** (`0x128aa4` + team × 0x188 + car × 0x60 + set-up × 0x18: four teams,
  their first four cars, four set-ups of six numbers -- the team's record is 0x188: the chosen
  car, the fifth car won, 4 × 0x60). The car screen (`0x3d234`) has three steps: the car;
  the set-up, `0x13237c` 0..4 = "Standard", "Custom 1".."Custom 4" (STRINGS.TXT), offered
  only for cars 0-3 and not in arcade; then RACE / CUSTOMIZE (time attack: 3 LAPS / FREE
  RUN). `0x1324e8` = the set-up − 1. At the race (`0x339e7` cases 0 championship, 2 time
  attack, 4 free run): −1 → every car 2 2 −1 2 2 2; else the chosen six into entry 0 (the
  player's; with a ghost `0x327ed` moves entry 0 to the player's car number). Arcade (case 1)
  always 2 2 −1 2 2 2, and its level is 2 − `0x128a34`.
- **CUSTOMIZE** (`0x3e161`): the car turning in the 3D scene and eight items -- HANDLING,
  BRAKES, TIRE TYPE, TIRE PRESSURE, FRONT SUSPENSIONS, REAR SUSPENSIONS, SAVE, EXIT
  (STRINGS.TXT) -- which are the numbers 0, 5, 2, 1, 3, 4 of the six; five steps each (a
  marker at 16 × value + 0x11), the tyres three ("A - Dry", "B - Wet", "C - Snow",
  `0x128030`). Up / Down two items (two columns), Left / Right one; ENTER on an item opens it,
  Left / Right then change it, ENTER again keeps it; SAVE copies the six into the set-up.
  `0x14e20` then makes the car of them (`+8` the tyres, −1 = the track's own; `+4` the
  pressure against the tyre's best, `+0xc` / `+0x10` the springs front / rear, ...).
- **Built (the port's plain menu, not the 3D one):** a SET-UP line (Standard, Custom 1-4)
  for the championship, time attack and free run with cars 1-4; ENTER on a Custom opens CAR
  CUSTOMIZATION (the game's words; the plain menu's font has capitals only, so "B - WET");
  SAVE keeps it in `openscreamer.s2setup` (the port's own file). The race gets the six as
  `OS_S2SETUP=a,b,c,d,e,f` (`s2_race_start` puts them in the player's entry for modes 2, 4,
  8). Checked: Custom 0 0 2 0 0 0 (snow tyres, all soft) on England in time attack: 2700
  units behind the standard set-up after 150 frames; arcade unchanged; the recorded races
  r6, r7, g2 still the original's to the unit.
- **Compared with the original running** (DOSBox, menus driven by keys on the hidden screen:
  title, team, name -- it starts as "MIL" -- the options room (Left / Right: ARCADE,
  CHAMPIONSHIP, TIME ATTACK, ...), track, car with its set-up list and 3 LAPS / FREE RUN,
  transmission; the loading screen shows the track's BEST TIME and BEST LAP, "--:--:--  MIL"):
  time attack's race screen is the port's (LAP 1/3, THIS LAP, TOTAL, BEST LAP, BEST TOTAL, no
  POSITION, no bar: `build/png/s2_original_timeattack_race.png`); the free run's is the
  port's (THIS LAP, BEST LAP, A-E twice: `build/png/s2_freerun_original_vs_port.png`). Found
  by it: the end's list showed an empty row as 59:59:99 -- the game's `0x37e06` writes
  "--:--:--" for 3599.999 (0x4560fffc); the port's race screen now has the same rule
  (`s2_clock`). Not compared: the championship's screens, the end screens (a race would have
  to be finished in the original).
- **A race finished in the original** (`dosauto.py`, scratch: the menus by keys, then the
  player's car handed to the game's own driver by writing 0 to its controls pointer, car
  `+0xb4`, in DOSBox's memory -- as the attract race has it; pictures every 5 s): time attack
  on England, three laps in 250 s. Its end: the same list as the port's (rows, places,
  "MIL", `--:--:--`, the car pictures, RACE AGAIN / EXIT); the race is NOT entered (only
  cars with controls are, `0x353b3`), and BEST LAP stayed `--:--:--` all race. Found by it:
  **the instrument stays on at the end** -- `0x45882` calls `0x46b76` every frame but in
  the attract race; only the texts stop once the car has finished (car `+0x4c` bit 0). The
  port had hidden the dial with them; mended. Picture: `build/png/s2_timeattack_end_original_vs_port.png`.
- **The championship in the original** (the same rig; options room → CHAMPIONSHIP → LEAGUE
  SELECTION → "WELCOME TO LEAGUE 1" → the track's page, TERRAIN / DIFFICULTY, "ACCELERATE TO
  CONTINUE" → car, set-up → loading → the race): the race had ONE lap -- the game settings'
  NUMBER OF LAPS (this copy's settings say 1), as the port's LAPS line. Its end is the port's
  (`build/png/s2_champ_end_original_vs_port.png`: "NAME CAR TIME PTS", the places, the cars'
  pictures, 10 / 6 / 3 / 0, the instrument on) but for one thing: the original offers
  **REPLAY** (on the left, the one taken by ENTER) and CONTINUE; the port has no replay yet and
  shows CONTINUE only. The port's own still: the WELCOME and track pages (the plain menu).
  CONTINUE leads to **LEAGUE REPORT**: big letters, rows "n.", the team's badge, the name
  ("MIL" / "CPU"), the points; RACES COMPLETED / RACES TO GO; "ACCELERATE TO CONTINUE" over
  the menu's 3D scene. The port's report now has that layout in the big font, the race
  screen's small team marks for the badges (the big ones are the 3D menu's), and takes the
  accelerator as well as ENTER: `build/png/s2_league_report_original_vs_port.png`.
- Trap met on the way: this machine's shell does not split `$v` -- `env $v` with two settings
  in `v` passes ONE variable. Write each setting out.

## S2-5zo. Opponents' smoke through bumps and walls; Screamer 1's blend table lost — 2026-10-14

Danik: "Opponent's smoke particles can still be seen through bumps/walls." Two allowances
added up: the road hid no puff unless more than 1 / 0.45 times nearer (S2-5zi), and Screamer
2's puffs were also tested against EVERYTHING as twice as near as they are (`zbias` 2, so their
lower half would not go under the road). A puff behind an earth bank 8000 units away showed
through it (`build/png/s2_smoke_through_bank_before_after.png`, left).
- Both gone. Now (`over_ground`, main.c): a puff pixel `below` units under the puff's middle
  (= its foot) is not hidden by drivable track only if that road is no nearer than
  0.85 × h / (h + below) of the puff's depth -- where flat road between the camera and the
  foot would be. h = the smaller of the camera's height over the road under it and over the
  puff's foot (on an even slope the first is the right one; over a dip the second). The upper
  half, walls, scenery and cars: the plain depth test. Clear views draw the same smoke as
  before; the only change near a car is that a puff no longer paints over the car's own body.
- Test switch: `OS_S2PUFFTEST=1 OS_S2PUFFCAR=<car>` puts five puffs where that car stands,
  with `OS_S2RUN`, `OS_S2LOOK=<car>`, `OS_S2ORBIT=<deg>,<far>,<height>`.
- **Found on the way, a Screamer 1 regression since commit ed49408 (was 36aaf93 before the history was rewritten):** `g_smg` grew to 112 rows
  for Screamer 2's smoke, and the check "the file filled the array" then failed for every
  Screamer 1 `MAPn.SMG` (64 rows) -- the port took it for missing and drew every routine-224
  surface (smoke, shadows, glass, track 4's beams) as a 50 % mix with the strength's palette
  colour: dark squares round the car. The check is now "64 rows (48 for Screamer 2)". Live
  pictures of track 2 with `OS_SMOKETEST` are again the old build's. The regression set's two
  standing differences (track4: a far animated sign; high) are not this.
- **Streaks through the smoke (2026-10-15, Danik: "some streaking in the smoke particles";
  `shot_000`, track 4, a car sliding by the wall).** One-pixel dark lines across the smoke,
  long and straight. A puff's middle is the wheel's foot, and there it was a hair under the
  road as drawn (the car's height against the road's): the row just over the middle had
  `below` -0.4, so `over_ground` (from `below > 0`) did not take it, the plain test did, and
  the road, 1 % nearer, won -- that row of that puff was not drawn. A sliding car's puffs stand
  side by side at one distance, so their missing rows join into a line. Now `over_ground`
  starts 64 units over the middle (`below > -64`); a crest still hides a puff (it is far more
  than 15 % nearer), walls and cars are not ground. Found with a scratch copy that put sixteen
  puffs in a trail at Danik's place (`OS_S2AT=195838,238468,5.897`, view 3) and logged the
  depth test down one column. Screamer 1's pictures (tracks 1, 5) unchanged.

## S2-5zp. The replay — 2026-10-14

Read: `0x2b2a8` (the loop round a race), `0x2e1f3` / `0x2ecb0` (a race set up), `0x4a2d4`
`0x4a3c4` `0x4a407` (the block, writing, reading), `0x4a455` `0x4a59a` `0x4a488` (a frame
written / read), `0x326e2` (the file), `0x2bd7f` (the replay's frame), `0x2d7c0` and `0x132b4`
(keys), `0x463c0` (the band), `0x4746a` / `0x47e75` (the ends). Seen in the original
(`build/png/s2_replay_original_vs_port.png`).
- **What is written down** is what time attack's ghost has (S2-5zk): every frame of an arcade or
  league race (modes with no bit above 4) its two times and the player's six controls, 0x20
  bytes, at the START of the race's frame `0x2baa4` (`0x4a455` -> `0x4a59a`), into a block of
  up to 3,000,000 bytes (`0x4a2d4`). The free run, two players, the attract race are not.
- **Kept** as the end's screen comes up, once (`0x4746a` / `0x47e75`: `0x326e2` writes
  `replays\temp.rpl` -- the frames so far, the player's car, both players' controls records,
  the race's record `0x130d6c`). **REPLAY** leaves the race with that name kept; `0x2b2a8`
  sets the race up again (`0x2e1f3(0)` -> `0x327ed`), the mode gets **0x100**, `0x4a407` turns
  reading on (`0x1330b8`), and the replay's frame (`0x2bd7f`, the same as `0x2baa4` but run
  until its own clock has caught the real one) reads each frame back (`0x4a488`: the times,
  and the controls into car `0x11f0cc`'s record). Everything else is worked out again from
  the same start. When the frames run out the player's car is marked done (`+0x4c |= 1`) and
  its controls let go: it coasts, and the end's screen follows. RACE AGAIN from there is a new
  race; REPLAY again the same replay; EXIT leaves.
- **In a replay:** the view starts as **1, the trackside cameras** (`0x2e1f3`); a **band** over
  the bottom 20 rows, darkened, and in the big letters at (10, 180) "TEAM - VIEW" -- the team
  of the car the cameras follow (STRINGS.TXT, `0x127edc`), the view's word (GAME.TXT: 1
  EXTERNAL, 2 INCAR, 3 BACK, 4 NEAR, 6 INSIDE), while the player's car runs (`0x463c0`). The
  race screen's texts are off in view 1, as in any race (`0x45882`). Keys: **ESC** ends it at
  once (`0x2d7c0`: `0x11f0a8 |= 8` -- the race's frames are no longer run, `0x2b9b1` -- and the
  player's car done); **F9** the trackside cameras, **F11** the cameras follow the next car
  (`0x132b4`, `0x2d7c0`: only in a replay). Nothing goes into the best times (`0x201ee`,
  `0x353b3` not called), and a league race's places are those taken as ENTER was pressed in
  the race itself (`0x47e75`: copied to `0x132384` only when not a replay) -- the replay's end
  shows that table too.
- **The ends:** arcade REPLAY / RACE AGAIN / EXIT in thirds (106 wide), REPLAY first and lit;
  a league race REPLAY / CONTINUE in halves (CONTINUE alone in the middle without a replay).
- **The key table** (`0x100002 + scancode`, read by `0x2d7c0`, `0x132b4`): F1-F4 `0x10003d..`,
  ESC `0x100003`, **M `0x100034` is the mirror** (the port had R: both work now), F11
  `0x100059`.
- **The port:** the recorder hook is the original's three lines (`s2_hooks.c`
  `FUN_0004a455`; the interpreter runs the program's own); the frames are kept in memory that
  outlives the parts (`os_slot(8)` -- the slot table had eight places, `id & 7`, and slot 8
  was the WINDOW's: now sixteen), not in a file; REPLAY starts the race as RACE AGAIN does
  (`os_go`), and `s2_race_start` sets the replay up; the port's step being fixed, the replay
  runs the port's own `0x2baa4` frame for frame. `s2_hud`'s "finished" is now the player's car
  (`0x11f0d0`), not the followed one (after F11 the end never came).
- **Checked:** `OS_S2REPLAYTEST=<frames>` drives a pattern of controls for so many frames, keeps
  the race and starts its replay; with `OS_S2LOG` every frame of the replay is the race's to
  the unit, all four cars, in both builds (600 and 300 frames). Live pictures: the band, F11
  ("ANGELS - EXTERNAL"), F1 ("ANGELS - INCAR"), ESC -> the place and time -> the choices,
  RACE AGAIN. The driving checks (`s2replay.py` r6 r7 g2 exact, r5 as before) are unchanged.
- Not done: replays kept as files / watched from the menu (`0x11f0a0`: a replay named from
  the menu leaves the game at its end), two players' replays.

## S2-5zq. The game's own menu: how it is built (survey) — 2026-10-14

The plain menu (S2-5zg) is to give way to the original's. Seen in the original (DOSBox, earlier
pictures): title -> TEAM SELECTION (the four badges turning in 3D) -> ENTER YOUR NAME (a wheel
of 3D letter tyres) -> the OPTIONS ROOM (a 3D room, turning between ARCADE, CHAMPIONSHIP, TIME
ATTACK, ...) -> arcade: blue-grid pages (S2 ARCADE, TRACK selection with three pictures, CAR
selection with the 3D car and its plate, select TRANSMISSION); time attack / league: 3D pages
(TRACK SELECTION over the map with TERRAIN / DIFFICULTY, CAR SELECTION with the set-ups) ->
the loading picture with the best times.
- **The menu's loop `0x38540`:** the page it is on is an ADDRESS at `0x132494` (start
  `0x3c9ac`, the attract race); `while (page) call page`; a page sets the next and returns, or
  0 to start a race: `0x339e7` (the race's set-up) -> `0x2b2a8` (the race) -> the page after
  by the kind of game (`0x1324dc`: 0 league -> `0x43914` report, 1 arcade -> `0x3b674` ...).
- **The pages** (decompiled; `f_<addr>.c`): `0x3ff37` TEAM SELECTION + ENTER YOUR NAME
  (`0x1324c4` 0 / 1; the name's letters `0x1289f4`; ESC on the team page leaves the game,
  30 s idle -> attract); `0x38d8d` the options room (-> `0x3b674` arcade, `0x3bf06`,
  `0x3f72a`, `0x3ff37`, `0x420c0`, `0x431b3`); `0x3b674` arcade (`arcade.bkk`, "scene");
  `0x3cbb8` / `0x3d234` / `0x3e161` time attack's track, car, CUSTOMIZE; `0x4254f` best times
  (`hiscore.bkk`); `0x431b3` / `0x431ca` LEAGUE SELECTION; `0x43914` / `0x4395b` LEAGUE
  REPORT; `0x41fcf` `bkg_6.bkk`; `0x3988d` `0x3989d` `0x3a870` `0x3bbc4` `0x3bc3f` `0x3edb4`
  `0x3ede5` `0x40a67` `0x40ac3` `0x4189f` `0x41bfe` `0x420c0` `0x394f5` `0x3951f` not yet
  placed. A page runs its own loop of frames (`0x38d48` at the top: keys `0x4cca5`, sound,
  the step `0x4adf8`; `0x753a0` draws the engine's list; `0x7f5a2` shows the frame).
- **What a page needs loaded** is a list of numbers ended by -1 handed to `0x44587` (and
  `0x44f72` lets go of what the next does not): 1 the menu font `gfx/graf%c.dat`
  `menu%c.fnt`, 2 its sprites `menu%c.spr`, 3 STRINGS (`0x127ed8`), 4 a picture of GRAF into
  the picture record `0x1323c8`, 5 the team's room `gfx/menu%c%c.dat` ("map", its materials,
  five `ashadow`), 6 the team's cars `gfx/cars%c0%c.dat` `auto%c`, 7 the logos `gfx/logo%c.dat`
  `a%clog00`.. and the 64 letter tyres `ALETTER`, 8 the screen (far plane `0x7f504(-0xab20)`,
  320 x 200, clip boxes), 9 the maps `gfx/maps%c.dat` `amap%c..`, 10 the plates
  `gfx/plates%c.dat` `a%cplate`, 11 the cars again by `0x4bc94` ...
- **The engine, as the menu uses it:** an object list of 200 records of 0x1c at `0x11da14`
  (+0x0c the object's record: +8 x, +0xc y, +0x10 z, +0x14..+0x1c three angles in 2048ths,
  +0x20 the animation's place; +0x14 loaded; +0x18 its place in the draw list); the draw
  list at `0x11d6f4` (`0x4be04` puts an object in, `0x4beba` takes it out, `0x4bfbe` /
  `0x4c01c` show / hide it: the entry XOR `0xc0000000`; `0x4bda7` takes all out); a camera
  record (`0x131ea4` on the team page: +0 .. +8 angles, +0xc .. +0x14 place; `0x7e8b4` makes
  its matrix); named blocks of memory (`0x4f0c6` make, `0x4f3c8` find, `0x4f033` let go);
  files read through the containers (`0x4eba5` open, `0x4ee3a` read, `0x4eea5` seek,
  `0x4ed79` close); a picture (`0x51532` load "V652...", `0x51b02` copy to the screen),
  fonts (`0x50dd3` load, `0x50f45` text, `0x51020` its width), sprites (`0x50a9c` load,
  `0x50c0a` draw), clip boxes (`0x50d6c`, `0x50fb9`), the darkened box `0x51698`, the
  palette (`0x4fe21` from `%s.COL`, `0x512f1` faded, `0x51664` set), the CD's music
  (`0x4d0f4`..`0x4d7c0`), the keys' table copied each frame to `0x100002 + scancode`
  (`0x4cca5`).
- **The way to carry it over** (as the race): `tools/s2c.py` converts the menu from its loop
  `0x38540` with every page named (`--dispatch`: the pages are reached through the address
  in `0x132494`, which the converter now turns into `s2_dispatch(address)`, a switch; a
  routine's address as a value is a number), the race's own routines left to the port
  (`--hook 0x2b2a8 0x3c9ac 0x339e7 0x30e75`): 148 routines (90 of them the menu's), and
  about a hundred engine and library calls for the port to answer.

### Built so far (2026-10-14): the game's menu runs in the port, to TEAM SELECTION

`openscreamer -game 2` (the default since 2026-10-15; `OS_S2PLAINMENU=1` the port's plain menu) runs the game's loop `0x38540` (converted, `tools/s2_set.txt`)
-- past the CD's check (`0x3162c` answered "there") -- to TEAM SELECTION, drawn by the game's own
page `0x3ff37`: the background, TEAM SELECTION and the team's name in the menu font, and the four
badges in 3D, the chosen one in front -- the original's picture (DOSBox) but for the moment of the
badges' turn. Test switches: `OS_S2MSHOT=<frame>,<file>` (that frame's picture, then the end; the
frame's time fixed at 1/30 s), `OS_S2MKEYS="frame:scancode,..."`, `OS_S2MLOG=1` (containers,
materials, objects, what the engine's list holds).
- **The converter** learnt: routine pointers (`--dispatch`, `s2_dispatch()`); `thunk_` names;
  `func_0x...` (routines Ghidra had not made: decompiled, their callers again); a float's bits
  tested with `&`; `._4_4_` / `._0_4_` / `._0_1_` of locals; a program string indexed; a table
  of pointers indexed; `ram0x...` constants (their value from the image); `uRam`/`iRam`
  globals; `CONCAT11/22`, `SUB84`, `NAN` (s2_mem.h).
- **Ghidra:** `tools/JoinFunc.java` joins routines the analysis cut in two (the menu's pages
  0x3988d, 0x3bbc4, 0x40a67, 0x394f5, 0x3edb4, 0x431b3, 0x43914, 0x4189f began with a short
  prologue and a false second function, even mid-instruction); `tools/s2_sigs.txt` has
  sprintf as varargs (`0x4eaed 0 int int ...`: FixSigs learnt `...`; the menu's file names
  and texts had lost their arguments), the engine's file open with four registers (0x4eba5),
  the loaders of a picture / font / sprites with three (0x51532 0x50dd3 0x50a9c), the fade's
  four stack arguments (0x512f1). `tools/s2regargs.py` (capstone, in the scratch venv) lists
  which of EAX EDX EBX ECX a routine reads before writing -- a hint, not proof.
- **Taken as the game's** (pure engine work): the draw list's routines 0x4bda7 0x4bd1b 0x4be04
  0x4beba 0x4bfbe 0x4c01c, the picture record 0x514fa, the loaders and their frees 0x50a9c
  0x50dd3 0x51532 0x50b71 0x50ef4 0x5160b 0x51521; their decompiling slips are corrected in
  `tools/s2c_fix.txt` (Watcom keeps every register but EAX: EBX is the record all through,
  the open's answer, a read does not change the loop's ECX, a picture's size from its header).
- **Answered by the port** (`port/src/s2_menu_hooks.h`, only while the menu runs: the race is
  as before -- the driving checks are exact): files out of the containers by the engine's file
  record (0x4eba5 0x4ee3a 0x4ef0a 0x4eea5 0x4ed79), named blocks and malloc / free (a first-fit
  allocator over the block; the converted build's block is now 8 MB, its stack the last 512
  KB), containers and their materials and palettes (0x4b49f 0x4feb4 0x4fe21), objects by name
  (0x4b843 / 0x4bc94 note name and container), the screen (0x51b02 picture, 0x50f45 text,
  0x51020 width, 0x50c0a sprite, 0x51698 dark box, 0x51aa4 a fill of the game's screen clears
  the port's), the palette shown (0x51664) and faded (0x512f1), the camera (0x7e8b4) and the
  engine's list drawn (0x753a0 / 0x753e8) by the port's renderer (`scene_object.mat`: the
  engine's turn 0x759b8 written out; the camera as the race's), the frame shown (0x7f5a2), the
  keys (0x4cca5: the table at 0x100002 + scancode from SDL), the step (0x4adf8), sprintf,
  atoi / atof / strstr / strlen. 0xae662 is the game's EXIT (after 0x4b169 / 0x4b1a4 have said
  why): it ends the menu. The block holds the player's STARTL.EXE's data object (copied at the
  menu's start; the engine's variables in its code object come from the image as their pages
  are first used).
- **Danik's first look (2026-10-14): "when I switch badges the rotation is not done properly;
  the letters scroll way too fast"**, three causes:
  1. the carousel's turn (0x336eb) never moved but the last place: its counter is a stack
     local the decompiler left untyped (`undefined4`), taken as unsigned, so `-1 < i` was
     never true. `s2c.py` now makes an untyped four-byte stack local an `int32_t` (16 of the
     race's routines' text changed with it; the driving checks are as they were);
  2. every movement eases by the frame's row of the tables 0x11f2a0 (0x33635, 0x11212 through
     0x1240c0), which the program makes as it starts (0x2ddea -> 0x315a4) and the race's start
     made for itself: the menu had zeros, everything jumped. `s2_ease_tables()` for both;
  3. the game takes no step under 1/50 s (0x4adf8), so a screen showing more than 50 pictures a
     second ran the menu's time ahead (the letters' roll, held, is one a 1/8 s of game time):
     the menu shows no more than 50 a second (s2m_show).
- **"After a while it quits: the game's error 3"** (Danik): the object list full. After 30 s on
  the team's page the game runs its attract race (0x3c9ac): it lets go of every set of the menu
  (0x44f72) and the race's set-up empties the engine's list of 200 (0x4baf6); the port had
  skipped the attract race and gone back to the team's page, which loaded the badges and the
  64 letters again into new places each time (0x4bc94 takes the first place free). Now 0x3c9ac
  is the game's own; the race stand-in (0x2b2a8) does what a race does to the list
  (`s2m_objects_clear`, also 0x4baf6) until races start from this menu; 0x4ba39 lets one object
  go (only out of the draw list), 0x4b843 takes the first free place. Five rounds: 72 places.
- **The menu at the screen's own rate** (Danik: "the menus don't work at the max framerate"):
  the 50-a-second cap is gone. The step is the frame's real time (0x4adf8, under 0.2 s); under
  1/50 s the easing row is made for that step ((0.5 + 0.005 i) ^ (50 x step), pointed at by
  0x1240c0), and the menu's whole-number angles turned by the step (badges, logos, cars: 10
  places, `tools/s2c.py` turn_by -> `s2_turn_by()`) keep what is cut off each frame -- at the
  game's own rates (step 1/50 s or more) the sums are its own. `OS_S2MSTEP=<s>` a test's frame
  time. Checked: a turn at 30 and at 150 frames a second ends in the same places.
- **F5, the frames a second, on every screen**: taken where every screen's events come in
  (`poll_event`, main.c); drawn in the game's menu, the plain menu, the loading screen too.
- **The options room** (0x38d8d): set 5 loads the team's room through 0x4fca9 ("map1" of
  `gfx\menu<team><set>.dat`: MAP1.MMP / .UNR / .MLB / .COL / .BCK -- a track's kind of scene;
  0x50165 is NOT a loader, it lets a material library go -- answered since, for the reflections'
  map). The port loads it as it loads a
  track (`s2m_scene_load`: the room's library first, base 0, the objects' after it; the
  backdrop), 0x753e8 draws backdrop + room + the engine's list (0x753a0 the list alone), 0x502d4
  lets it go. The camera goes between five stations of P_1.MCM (0x32c48), looking at a point
  (0x338ed), swaying (x 0.2 a second). ARCADE, CHAMPIONSHIP (the registrations desk), TIME
  ATTACK (the clock) match the original's pictures. Its materials are routine 16, as a
  track's; the original's pictures look bluer only because DOSBox shows them at 7800 K
  (its log: "Auto-switched to 7800K colour temperature") -- the colours measure the same.
- **The arcade page** (0x3b674): `arcade_L.bkk`, the team's four cars (CARS101, ENVM11) driving at
  the camera along SCENE0..3.PTH (the game's own path routines 0x11c0c / 0x12512, converted) --
  as the original. ENTER goes on; "accelerate" is the controls' record (0x38b2c reads 0x1277b0
  when 0x11f298 is set): not fed yet. The black car's windows (reflections, ENVM) draw as blue
  squares: not right yet. The game reads through an empty list's head (0x1257f, address 8):
  the null page is a page of noughts.
- **Track, car, gearbox pages** (0x4189f, 0x40a67, ...): TRACK selection (three pictures, the
  chosen one lit and framed), CAR selection (the team's cars round a carousel, the plate with
  the name, maxspeed, drive), select TRANSMISSION -- as the original. Two things it took:
  every car of a container has its OWN material library (AUTO1.MLB .. AUTO5.MLB; the engine
  keeps with every object the library current as it was loaded, object +0xb4): the port keys
  libraries by container AND name and gives every object its own (s2m_obj_lib); and the tables
  the program reads as it starts (0x2ddea: CARS.DTR CAR_PARAMS, CRUSC.DTR, MISC.DTR SKILLS,
  CD_TRACKS, DAMAGES) -- "maxspeed:000" without them. The speed is in miles when GAME.TXT's
  unit word begins with M (0x124654, set by a race's set-up 0x2ef4e -- the attract race does it
  before the first page): the race stand-in does that much (`s2m_race_standin`).
  (The original's picture shows HORIZON grey and black from the front; the port's, spinning,
  was caught from behind: to be checked against the original running.)
- **Danik: "cars go in reverse instead of driving at me, then erratically (sideways)"; "on the
  car page the arrows move the selection once, then only ENTER works"**:
  1. the engine's turn (0x759b8) is to be taken by its ROWS for the port: with a heading alone
     an object's x then goes to (cos h, sin h) and its z to (-sin h, cos h), as the race turns its
     cars (o->yaw = the heading). By the columns every object turned the other way -- the cars
     drove backwards, slid on the bends, and their writing showed mirrored (the badges, only
     spinning, had not shown it). `OS_S2MTRANS=1` = the columns, a test;
  2. the car page takes a key only when the cars are within 150 of their places (0x40a67:
     `fcomp QWORD [0x1114de]`): the converter read every unnamed constant (`ram0x...`) as a float
     -- 0.0 here, so no car was ever near enough after the first turn. `s2c.py` now reads it as
     a double when the instruction that reads it does (x87 DC / DD; D8 / D9 four bytes).
- **The race from the game's menu** (2026-10-14): the race's set-up 0x339e7 is the game's
  own now (converted: the record 0x130c48.. as the original makes it). The race (0x2b2a8, but
  not the attract race, mode 0x80) is the port's other part: `s2m_race` gives it what the
  record says -- OS_TRACK (0x130c4c + 1), OS_COND (0x130c50), OS_DIFF (0x130d4c), OS_LAPS
  (0x130d48), OS_S2GAME (0x130d10), OS_TEAM / OS_CAR (entry 0 +0 / +4), OS_S2AUTO (+0xc, the
  gearbox: the race part puts it in the player's entry), OS_S2NAME (+8) -- keeps the menu's
  block, the engine's pages and its own tables (os_slot 9, `s2m_keep`), and goes to the race
  part (`s2m_race_go`, OS_FROM_MENU). When the race is left the menu starts again, puts all of
  it back (`s2m_unkeep`) and its loop goes on right after the race's call (`tools/s2c_fix.txt`
  @38540: a jump from its top to a label after the call) -- so the original's own code picks
  the page after the race (an arcade race: the arcade page). Checked live: team, name, room,
  arcade, track, car, AUTOMATIC -> England, 3 laps, the car chosen -> ESC -> the arcade page.
  **What the race leaves for the menu** (in the game it is one block): the race part keeps,
  as it is left, the race's record, the four cars, the places (0x132384..: count, then per
  place the team, model, car, time -- what 0x47e75's ENTER writes when not a replay; the port's
  end screen is its own, so `s2_race_results_keep` writes it) and the best times (0x1290bc..),
  os_slot 10; the menu puts them back into its block (`s2_race_results_put`). The league's race
  starts from the menu's random numbers (OS_S2SEED = s2_seed after 0x339e7 drew the weather).
  Checked live (`OS_S2MTESTLAPS=1`, `OS_S2ROBOT`): CHAMPIONSHIP -> LEAGUE SELECTION -> WELCOME TO
  LEAGUE 1 -> LEAGUE 1 England (terrain, difficulty, the 3D map) -> CAR SELECTION in the garage
  (set-ups Standard, Custom 1-4) -> the race -> CONTINUE -> the game's own LEAGUE REPORT (the
  places, the teams' badges, 10/6/3/0, races completed 1, to go 3) -> LEAGUE 1 Egypt.
- **The garage car page** (0x3cbb8, joined with its false second half 0x3d234 by JoinFunc):
  maxspeed is pushed straight from the FPU (car +0x5c x 1.188 [/ 1.6]), which the decompiler
  lost: `tools/s2c_fix.txt` @3cbb8 writes it in. (A scan of every sprintf in the menu against
  its format's %s: these were the only ones short.)
- **The settings file, CHOICE.DAT** (2026-10-14): the block 0x1289ec, 0x2077 words (0x81dc
  bytes: the name 0x1289f4, the volumes 0x128a5c / 0x128a60 / 0x128a64, the view a race starts
  in 0x128a68, the league in hand 0x128a10 .. 0x128a98, which leagues and tracks are open, the
  set-ups 0x128a9c.., the best times 0x1290bc..). 0x32d53 reads it as the menu's loop starts
  (none: the defaults), 0x331dc writes it as the loop ends -- the only two places; word i is
  kept as (v + i*0x1050c) ^ i. Both are the game's own, converted; the library's fopen / fread /
  fwrite / fclose (0xb6aac 0xb6b60 0xb6e97 0xb6d3b) are answered in `s2_menu_hooks.h`: the file
  is the port's `openscreamer.s2choice` in its home, the game's bytes as they are (any other
  name is refused; there is none). The save's mode "wb" came out of the converter as a host
  pointer (`tools/s2c_fix.txt` @331dc). Also answered: abs (0xb73c7), itoa (0xb743d, the best
  times' page). **The port's own:** the game's save is also made before every race from the
  menu, and when the window is closed in the menu -- the game saves only as its menu is left,
  and a window closed in a race would lose everything since.
  - The league in hand is NOT kept across a sitting by the game: LEAGUE SELECTION (0x431b3)
    sets races done (0x128a1c) and every points total to 0 each time it is entered. What a
    sitting leaves is the leagues opened, the best times, the name, the settings.
  - The attract race moves 0x128a58 on by one track each time (0..5): kept in the file too.
  - **The race's settings**: 0x339e7 copies the volumes and the view into the race's record
    (0x130d5c 0x130d60 0x130d58 0x130d68), the race keeps the view in use as the player finishes
    (0x201ee, not the trackside one), 0x38540 copies the four back. The race part, whose record
    went back to the menu with zeros there, now takes them from the menu's kept block
    (`s2m_race_settings`) and starts in that view (0x2e1f3: 3, 4, 6, else 2).
  - **The best times** are one set of lists in the game; a race from the game's menu starts with
    the menu's (`s2m_race_records`), and the port's own `openscreamer.s2records` follows them
    (the plain menu reads that).
  - Checked: a fresh home writes 33,244 bytes that decode to the defaults; a name changed in
    the file survives a run's load and save; a league race (`OS_S2ROBOT`, which now gives the
    car back once it has done its laps, so that its time counts) leaves the league table and
    MIL's 1:23.389 in the championship's 1-lap England list; with `openscreamer.s2records`
    deleted, a second sitting's race adds its time beside it.
- **The race's whole record from the menu** (2026-10-14): 0x339e7 builds the record
  0x130c48.. from GAME SETTINGS and the pages -- damage 0x130d54 (DAMAGES, OFF by default; the
  race part had always raced with damage on), 0x130d64 (MUSIC), detail 0x130c54, 0x130c64, the
  entries with the garage's set-up at +0x14. The race part now takes it all from the menu's
  kept block (`s2m_race_record`, after its own set-up) but for the mode and times
  (0x130d10..0x130d2f) and the laps (OS_LAPS); and it loads the cars the record lists
  (`OS_S2ENTRIES` = count, then team and model each; arcade: 4 cars, team and car each one
  on; time attack: 2). Checked: DAMAGES ON on the settings page reaches the race.
- **GAME SETTINGS** (the fifth station: transmission, detail, laps, arcade skill,
  multiplayer enemies, ghost car, damages, music, VIEW HIGH SCORES) and the **HI SCORES
  TABLE** (total / lap, arcade / championship, laps, track, VIEW REPLAY) work; TIME ATTACK
  from the room goes through TRACK SELECTION (the 3D map) and the car page (3 LAPS / FREE
  RUN) into its race. The arcade's S2 ARCADE and TRACKselection cards are the original's.
- **Danik: "first in two league races, 10 points, an opponent 16"**: the league report
  (0x43914) rebuilds the places from the cars only when the player's car has NOT finished
  (car +0x4c & 2), and then puts the player last; otherwise it uses the places 0x47e75 wrote
  at the end screen's ENTER. The port kept the result as the race part was left -- after a
  REPLAY left with ESC the cars are where the replay stopped (a few seconds in, the player
  "done", +0x4c |= 1, not finished): DNK last, the car at the head of the grid first. Now the
  result is kept at ENTER on the race's own end screen and not overwritten from a replay
  (`s2_race_results_keep`, main.c's end-screen ENTER; `OS_S2MLOG` prints what is kept).
- **The California map** (LEAGUE / TRACK SELECTION, Danik's shot: a tower's grey roof with
  nothing under it, the background showing through): the map `amap4-vs` is four blocks
  (`MV_map4`, `a1map4`, `a3map4`, and `a2map4` -- the downtown towers, 377 corners) and
  2599 corners in all; the port's model limit was 2560 (`mesh: block 3 ... array full` in
  the log), so the towers' block was never loaded. `MESH_MAX_VERT` 3072, `MESH_MAX_FACE`
  2048 now (MAPS2's AMAP4-VS has 1886 faces); no other model of either game comes near.
  Checked at the same angle as the original: the original's map record held through
  DOSBox's memory (the original's heap pointers: host = DOSBox's memory block + 0xc03010 +
  the pointer; the map's turn is its record's +0x18, `0x128a0c` the time attack's track),
  the port's by `OS_S2MAPYAW=<0..2047>`; at 0 to 1280 in steps of 256 the two pictures
  agree. The see-through look is the original's too; the lone roofs were the missing block.
  (`OS_CULL` is now read by the game's menu as well, a test.)
- **Damage, read out (2026-10-14).** One routine takes every knock, `0x19aaa(car, side, corner,
  _, share, force)`, called by the walls (`0x29571`: per corner, the corner's speed x |sin| of
  the angle met, the side by that angle) and by car against car (`0x245a3`, both cars, any
  pair; only the knock's sound needs the player). Always: the side's count (car `+0x1c +
  4 side`) grows by (1 - share) x force / 20000; once it reaches 1, a knock over 2000 sets the
  side's mark in car `+0xb8` (0x2000, 0x800, 0x4000, 0x1000 for sides 0..3). With DAMAGES on
  (0x130d54), force x the DAMAGES row of the level (MISC.DTR: pro 5, normal 2.5, rookie 1):
  the corner's damage (`+0xc + 4 corner`, to 2) grows by share x f / 50000 and sets the
  wheel's spring easing row (`wheel +0x28`, which `0x1bef1` eases the spring by through
  `0x111d6`) to 99 d + 55 (1 - d) -- the corner settles back ever more slowly; the whole
  (`car +8`, to 2) by f / 100000 shares, and above 1 the engine's force falls (`0x1f756`: x
  (1 - 0.3 (d - 1)), 30 % at 2). Starts at 0 every race (`0x14e20`); nothing mends it.
- **The look: a car's textures have 128 pictures**, and the engine takes picture
  `frames x phase >> 14` with phase = the object's `+0x20` >> 2 -- Screamer 1's rule (§15),
  so picture = flags >> 9. `0x16878` writes the flags every frame: 0x200 braking, 0x400 /
  0x8000 a pair of wheels turning fast (the blur), and car `+0xb8`'s four marks. AUTO1: the
  wheels' textures change with 0x400 / 0x8000, five body textures with one mark each (dents,
  scrapes, the number and badge scratched), one also with 0x200 (the brake lights). Only
  the cars' libraries have 128-picture textures (none in either game's tracks or menus).
  The port showed picture 0 always (it kept at most 96); now `MAT_FRAMES` 128 and the race's
  cars are drawn with `s2_car_state.look` as their phase; the menu's cars have 0 there.
  `OS_S2LOOKFLAGS=<hex>` (a test) draws every car with those flags; `OS_S2PLAN` works with
  `OS_S2RUN` too (the picture after a planned drive). Checked: crashes on England leave
  0x6c00, and the car shows dents on both flanks and the bonnet; Screamer 1 unchanged.
- **The menu's sounds (2026-10-15).** The same sound manager as the race's (S2-5zf):
  `0xafc70(m, container, name)` reads `<name>.dtr` of `<container>.dat` -- the page loader's item
  12 (its "loaded" flags are `0x1323cc + 4 item`) is MENU.DTR of SOUND, samples 0..14 -- and gives
  their count; `0xaeef8(m)` stops slots 0..31; `0xaf120(m)` stops them and lets the samples go
  (the loader, as the next page does not want the item; `0x31495` after a race). The manager's
  own volume (its `+0x18`, handed to HMI's driver, over every slot): 0x7fff as the program
  starts (`0xae910`), `0xaede0` sets it (the race's load `0x2e1f3` and its end `0x31495`: the
  settings' effects volume `0x128a64` -> `0x130d58`, 0x5fff at first), `0xaec90(m, to, step)`
  eases it (to 0 as a page is left for a race, and the race's pause screen `0x49f8e`). The port:
  `s2_master` in s2_hooks.c; a race from the game's menu now plays at the effects volume.
- **The CD's music (2026-10-15).** The game's CD routines are the DOS CD driver's requests:
  `0x4d5b4(track, again)` plays a disc track (`again`: `0x4d630`, asked every 0.4 s, plays it
  once more as it stops), `0x4d738` stops, `0x4d7c0(left, right)` the CD volume 0..255 (the
  settings' music volume `0x128a5c` >> 7). The menu plays track 2 when MUSIC is on (`0x128a44`
  0; the settings page's MUSIC starts / stops it at once; after every race); a race
  (`0x2e1f3`) plays MISC.DTR's CD_TRACKS for its track -- 9 8 6 7 4 3 5 in the game's order
  (England 9) -- when the record's `0x130d64` (= MUSIC on) is set, not in the attract race.
  The disc's tracks 2..9 are the music (track 1 the data). The port plays FILES
  (`cdaudio.c`): `track_NN.ogg / .mp3 / .wav` in the game's `music` folder, the game's folder,
  or the home's `music` folder; decoded as it plays (stb_vorbis, dr_mp3, dr_wav in
  `port/src/third_party`, public domain), looped; at half the disc's level (`OS_CDVOL`);
  `OS_CDTRACK=<n>` (a test) the race's song, `OS_MUSICLOG` says what plays. The mixer's device
  now runs at 44100 everywhere (the MiSTer's way: each frame of the 22050 mix twice, half-way
  the second time), the song added at the full rate. Danik's disc image made the eight files
  (tracks cut at the cue sheet's INDEX 01; little-endian; `screamer2/music/`, the game's: never
  published). Checked: the menu's song 2, England's 9 in a race from the menu, back to 2 at the
  report; MUSIC off / on; the mix minus the mix without music is the song at half level, in
  time; the MP3 and a mono 22050 WAV the same; the device's output has the song's top octave.
- **The attract race (2026-10-15).** The menu's first page (`0x3c9ac`; also after 30 s idle on
  TEAM SELECTION): ATTRACT.TXT into `0x1286a8`, the record mode 0x80, 4 cars (team n, model n),
  condition 0, level 0, set-ups 2, MUSIC on (`0x130d64` 1 whatever the settings say), the track
  `0x113224[0x128a58]`; no laps given -- the original's record had 0 and no car ever finished.
  After the race `0x128a58` moves on (0..5, in CHOICE.DAT), TEAM SELECTION, the menu's song.
  In the race: view 1 (`0x2e1f3`), every car the game's driver's, the CD song paused at the load
  (`0x4d6f0`) and let go on by the screen; every whole 10 s of the race's clock `0x2d7c0` sets
  the camera record's `+0xae8`, and `0x14acf` then follows the next car at the next trackside
  camera. The race screen (`0x45882`) draws `0x37ea9` instead: stages `0x127ecc` -- black with
  the Virgin logo (sprite 0x6e) sliding in (row 0x55) and "AND", Milestone (0x6f) and
  "PRESENTS", three seconds each once in place; the race out of white (1 / 1.2 s) under
  SCREAMER 2 (0x71, y 4), the song from here (`0x4d758`); the groups up to '#' at y 100 + 16 a
  line (font 0 colour 1, spaced 2), four seconds and a second's gap each; an empty group, or
  ENTER / ESC (`0x131f20` 0x10 0x11 0x20), ends it -- before the race shows at once, else after
  half a second to black -- and `0x11f0a8 |= 3` ends the race. Read in the original (DOSBox,
  its memory): mode 0x80, laps 0, the stages at 3.8 / 7.8 / 11.8 s of the race's clock, the end
  at 87.3 s, no car finished. In the port: `s2_attract_draw` (main.c), `s2_attract_step`
  (s2_hooks.c), the menu's race stand-in runs it, and its return goes on with the rest of
  `0x3c9ac` (`s2m_attract_done`; `s2m_resume` 2: tools/s2c_fix.txt's second label in
  `0x38540`). Checked against the original's pictures (the logos, the credits in turn, the
  cameras, the end at about 90 s), ENTER during the logos, ESC during the credits, the next
  one after 30 s idle on the next track (Egypt, song 6). `tools/s2c.py` kept dropping the `+ n`
  after a program string's name: 0x3c9ac's "attract" (was "@attract"), 0x11d6f's "MV_mauto"
  (the garage cars' wheels: the block was never found) and "NET_SPACE" are now right.
- **The race's pause screen and its CD player (2026-10-15).** ESC in a race (`0x2d7c0`): in a
  replay its end; once the player's car has finished nothing; in the attract race and the
  network game other things; else `0x49f8e`: GAME.TXT's RESUME / RESTART (not in the
  championship) / CD PLAYER (with a drive, `0x4d8fc`) / EXIT (the table `0x124610`), centred at
  y 0x66 - 11 n + 22 i over the standing, darkened race (`0x2b873(0)` draws it with its race
  screen), a box darkened again over them, the chosen one blinking over / under it by bit 0x20
  of the hundredths (`0x4ce0a`); UP / DOWN round, ENTER, ESC = RESUME; the effects eased to 0
  at 32767 a second, back at once to `0x130d58` as it is left. The answer: RESTART -> `0x11f0a8
  |= 1` (the race again), EXIT -> `|= 3` (the race over: a league's race counts the player
  last, `0x43914`), CD PLAYER -> `0x49aaf` and then the race goes on. The CD player: Screamer
  1's again in its own sprites (the picture sprite 10 at (0x1c, 0x28); the buttons
  `0x113480`: x, pressed, plain; the tracks "\3%d" 13 apart from (0x40, 0x4e) font 5 colour
  4; the volume's name font 5 colour 2 and strokes of 66 / 33 / 12 per 1024 at x 0x55 + 3 k,
  `0x498d8`); ENTER: play / pause, eject, the track before / after (played on only if one was
  playing), the volume down / up (480 a hundredth while held), CD (`0x130d5c`) or FX
  (`0x130d58`); what is set goes back to the menu with the race's record. In the port:
  `s2_pause_*` (main.c), the song's pause (`snd_cd_pause`), `s2_volume*`, `s2_sound_pause`;
  the disc's tracks are the song files found (2..); a race not from the game's menu has both
  volumes full. Checked: the four items, the CD player, the volume bar, ESC back to a moving
  race, EXIT; in a league race no RESTART, EXIT -> the report.
- **The cars' reflections (2026-10-15).** Danik's "blue squares" in the black car's windows (the
  arcade page) and every race car's glass. A car's routine-160 material (its glass) is 8 x 8 and
  its picture a placeholder of blue squares (team 2's fifth car: its own 128 x 128); the picture
  the engine draws is another library's: `0x4feb4(name, flags)` with flag 4 makes the library the
  engine's `0xae4e8`, and while there is one, routine 160 (`0xcc510`, the texture at `0xcd427`)
  takes its pixels from that library's first material (picture (frames x the object's `+0x3c`)
  >> 16) and only the SIZE of the face's own material. The race loads "env" of
  `gfx\envp<track><look & 1>1.dat` so (`0x2ecb0`, after the cars), the arcade and car pages "env"
  of `envm<team><look>.dat` (`0x4492f`); `0x50165` lets a library go by name (0 into `0xae4e8` if
  it was that one), `0x50247` all of them. The map wraps at 128 (`0xcd5a1`: the texel's address
  `& 0x7f7f`), and the face's size reads 16 times as far as Screamer 1's (a car's 8: the
  reflected line's whole range spans the map's 128 once; the original's windscreens on the
  arcade page show the map's dark streaks from side to side -- its pixels are colours 31, 114, 7,
  6, 5, 4 of the page's palette, black and greys, read out of DOSBox's memory with the palette
  `0x1338cc`). The coordinates are Screamer 1's (`0xb0040` = Screamer 1's `0xc2230`, the same
  machine code): 127 + 127 r - the slide, along the camera's right and up. **The slide**: the
  object's move since it was last drawn, in the camera's axes, x `0xb04a8` -- a constant in the
  engine's code, 0.008 in the program, which `0x1f1ef` sets to 0.002 (0.03 in a tunnel) for the
  car the camera follows -- added to two sums kept with the object (`+0x30`, `+0x38`), each taken
  to the nearest whole number of 256. Which picture: car `+0xc80` (`0x1f1ef`, by the road's kind
  under it: 0, 0x5556, 0x8000, 0xaaaa), copied to its object's `+0x3c` by `0x16878`. In the port:
  `g_env_mat` (main.c; -1 for Screamer 1, so its pictures are the same), `s2_env_slide`,
  `s2_env_rate`, `s2_car_state.env`; the menu's objects (turned by matrix) now work out the
  reflection too (they had used the file's coordinates) but do not slide. Checked: the arcade
  page against the original's pictures at the same moment, a race's rear window grey as the
  original's (it was blue); Screamer 1 the same, the interpreter build compiles.
- **The clip boxes (2026-10-15).** `0x50d6c(screen, x0, y0, x1, y1)` (sprites) and `0x50fb9` (text)
  keep a box in the screen's record (`+0x28..+0x34`, `+0x4c..+0x58`); the sprite and text drawers
  copy it in (unless `0x133bcc` / `0x133bd0` hold one for everything -- never set) and draw only
  x0 <= x < x1, y0 <= y < y1 (`0xc6435`, `0xc6646`). The menu narrows it once: the HI SCORES TABLE
  (`0x420c0`) draws its list in y 0x77 (sprites) / 0x79 (text) to 0xb4, back to the whole screen
  after. Its list starts two places above the chosen one (of 15) at y 120, 12 a row -- the "120 -
  12 x" scroll the decompiler shows reads a loop's counter the routine has just set to 0, so the
  list jumps, it does not slide (the same in the original) -- and the box cuts the sixth row,
  which the port had shown over the panel's edge. In the port: `s2m_clip` (main.c) through the
  race screen's own `g_clip`. Checked against the original's picture: five rows.
- **The "rooms' lighting table" is the see-through table (2026-10-15).** The menu calls `0x504f2`
  (S2-5zd) with the palette and every smoke colour -1 -- as the options room is set up (`0x38462`)
  and after the attract race (`0x3c9ac`) -- so only the shadow's 16 rows (32..47, towards black)
  are made. What uses them: the rooms' routine-208 materials (MENU11's MAP1.MLB: materials 131
  and 185, texels 32..47 only -- the shadows on the floor). The port had no table in the menu and
  drew them as an even mix; now `s2m_blend` (`OS_S2MLOG` says when it is made).
- **The game's own menu is the default (2026-10-15, Danik).** `-game 2` with no track opens it
  (the attract race first); a test switch that picks a race (`OS_NOMENU`, `OS_S2ROBOT`, the dummy
  video driver ...) still goes straight to it, unless `OS_S2MENU3D=1`; `OS_S2PLAINMENU=1` (or
  `OS_S2MENU3D=0`) is the port's plain menu. The menu sets `OS_S2MENU3D=1` as it starts, so its
  races know to keep their results for it. Checked with no switch at all: the attract race, TEAM
  SELECTION, the room, an arcade race, its pause screen, EXIT back to the arcade page.
- **A game controller in the game's menu (2026-10-15).** With the keyboard the menu takes only ENTER,
  ESC and the arrows (`0x38996`, scancodes 0x1c 0x01 0x50 0x48 0x4b 0x4d -> 0x10 0x20 2 1 4 8 in
  `0x131f20`). "ACCELERATE TO CONTINUE" is a joystick's: with one set up (`0x11f298`) the menu's
  frame `0x38d48` has `0x31888` read the first player's six controls (record `0x127708`: per
  control 0x1c bytes -- in use, 0 button / 1 axis, the device 0 joystick / 1 key, ..., +0x18 the
  button's bit; the values at +0xa8, 0x3ff held), and `0x38b2c` takes them as keys: accelerate =
  0x10 (ENTER), brake = 0x20 (ESC), left 4, right 8, gear up 2 (down), gear down 1 (up) -- a
  joystick's accelerate counts everywhere (the page's "accelerate allowed" only matters for a
  key). The port had no controller in the game's menu at all (it reads SDL's events itself): now
  `s2m_pad` (from `0x4cca5`) sets the record for a controller and answers `0x4cd69` (the
  joystick's buttons) with `controls_pad` -- A / right trigger, B / left trigger, the pad's or
  stick's left / right, right / left shoulder: as in a race. `OS_PADTEST6="n:bits,..."` a stand-in
  controller. Checked with it: accelerate alone carries the menu from TEAM SELECTION through the
  name, the room and the arcade page.
- **The replays as files (2026-10-15).** Read: `0x326e2(name)` writes `replays\<name>.rpl` -- 0x27c
  bytes from `0x1330d4` (the frames' count `0x1330cc`, the player's car `0x11f0d0`, both players'
  controls `0x127708` / `0x1277c8`, the race's record as it started -- `0x130d6c`, which the race's
  load `0x2e1f3` copies from `0x130c48`) then the frames, 0x20 bytes each, from `0x1330ac` (time
  attack `0x1330b0`); every race's as "temp" as its end screen comes up. `0x353b3`, as a car's total
  goes into its best times (not when the mode is 4), renames `temp.rpl` to
  `REP<laps 0-3><kind 0-2><track 0-6><letter>.RPL` (the letter 'A' + the first number from 1 no row
  of the list holds at +0x78; the number goes there, the name into `0x130d34`; an old file of the
  name removed first). The best times' page `0x4254f` lights VIEW REPLAY on a row with a number;
  ENTER puts the name in `0x130d34` and `0x1324e0 = -1`, and the menu's loop then starts the race
  without its set-up `0x339e7` (which clears the name) -- mode 1, the page's track -- and goes back
  to `0x4254f`; the race's load reads the file (`0x327ed(name, 0)`: the record over `0x130c48`,
  but for the volumes, the detail and the view, which stay the player's; the controls; then
  `(name, 1)` the frames from `0x2ecb0`) and runs it as a replay. Nothing in a race draws random
  numbers but the sparks (`0x2aeb9`), so a race is the same again from its frames. In the port,
  written by hand from that: `s2_rpl_write` / `s2_rpl_name` / `s2_rpl_load` (s2_hooks.c), the
  files in `openscreamer.s2replays/` (`S2REPLAYS/` in `os_path`), byte for byte the game's;
  `s2m_race_standin` loads one for VIEW REPLAY; `tools/s2c_fix.txt` @38540 a third way back
  (`s2m_resume` 3, label `s2_after_replay`). `OS_S2REPLAYLOG` says what is written and read;
  `OS_S2REPLAYTEST=<frames> OS_S2REPLAYFILE=1` replays the test race from its file. Checked: the
  test race and its replay from the file, every car's place and speed, 599 frames of 599 the
  same; a race from the menu -> REP010B.RPL (636 + 1554 x 32 bytes) and row 1's number; VIEW
  REPLAY lit on that row and the race run from the file; ESC -> the end screen, EXIT -> the best
  times' page with the row and its replay. (A race driven by `OS_S2ROBOT` cannot be replayed: the
  game's driver has the car, and what is written down is the keys.) Not done: the player's
  original replay files (the game's own REPLAYS folder) are not looked for.
- **The driving against the original again (2026-10-15, `tools/s2dos.py` + `s2replay.py`, England,
  20 s: grid, 1 s left, the climb, a hit at 115 km/h, 1.5 s right).** The player's car is the
  original's TO THE UNIT for 240 frames (13 s) -- through the climb at frame 150 and the hit at
  160 -- then a few units, then hundreds once the others are elsewhere (whom it touches). The
  computer cars: car 1 a unit off at frame 151 (its speed 106.60 against 106.41, pitch and lean
  still the same), car 3 at 160, then hundreds. `s2dos.py` now also writes every car's whole
  record every frame (`<prefix>.cars`, as the port's `OS_S2TRACEFILE`); compared field by field
  (the scratch script drv/rec.py): from FRAME 2 every car carries ONE-BIT differences in its
  springs' sums (`+0x548`.. a wheel, -112.57: -1025432569 against -1025432570) and the revs
  (`+0xc78`), the player's too, whose place they never reach; a computer car's `+0x4fc` (0.271604
  against 0.271608 at frame 4, on the grid) is the first that grows. So the conversion's rules
  are right and some sum is rounded once differently -- where is not found: `tools/s2emu.py`
  with `--hooks` on the real track now stops at frame 0 (the port's first question about the
  road is asked at another place than the program's: the moment its block is dumped has moved
  since it was written -- to mend before it is used again).
  **Then found and mended, in order:**
  1. **The rig's own fault:** `s2dos.py` wrote every number with six decimals; a step of 0.06 needs
     nine digits to be the game's float, so the port was given steps dozens of last bits off, and
     every division by the step (the corners' speeds) differed. Now `%.9g` (s2dos.py, s2replay.py).
     With that alone all four cars were the same to the unit for 168 frames.
  2. **The rig read frames half done:** DOSBox takes ~56 ms a frame here and a read 15 ms after the
     clock moved could catch the game in the middle of its sums (a step of 0.005 between two of
     0.057; a car's `+0x4fc` set in one and not the other). Now a frame's numbers are the LAST
     reading, between two reads of the clock that agree, before the clock moves on.
  3. **The engine's torque (`0x1f756`, the hand-written line of `tools/s2c_fix.txt`):** the game keeps
     revs / the most as a FLOAT (`fstp DWORD [ebp-0x14]`) and sums `G x 0.22 + (r x c - 1)`; the
     line had a double and `(G x 0.22 + r x c) - 1`. The revs were one bit off from the grid on.
  4. **134 constants in 40 routines were floats where the program has doubles:** the decompiler
     writes `(float)0.3` for a constant in a float's sum, but the x87 multiplies by the eight bytes
     it reads (`fmul QWORD`); 0.3 and 0.2 as floats are not the doubles. `tools/s2c.py` now types
     each `(float)v` of a routine as the double the routine reads with a QWORD operand when no
     DWORD operand holds a float of it (marked `/*double*/`; "constants that are the program's
     doubles" in its report). The revs' `(float)0.3` / `(float)0.2` were two of them.
  5. **A sum's order (`0x29571`, the knock's force):** the corner's speed is sqrt(y y + x x + z z) in
     that order; the decompiler prints z z + x x + y y, and at 53 bits three additions in another
     order can round otherwise (`s2c_fix.txt` @29571). The decompiler sorts the terms of a sum:
     a source of such bits wherever three or more floats are added.
  **Now:** all four cars the same to the unit for 13 s (240 frames: the start, a wall at 55 km/h,
  the climb, a hit at 115 km/h), car 3 to the end of the 20 s, car 2 to 18 s; the player's car
  parts first, by a few units after the 1.5 s of right lock -- still from the hit at frame 95,
  where the damage sums are tens of last bits off (a corner's +0xc, the side's +0x20) though the
  corners' speeds are the same: the angle (`0x14dc0`) or the order of the four corners' knocks, not
  found. Tried and of no effect: the sine, cosine and arc tangent left at the x87's 64 bits
  instead of rounded to doubles (`s2_mem.h`; put back).
- **Pictures against the original (2026-10-15).** England and Egypt at the countdown's "2" (`s2dos.py`'s
  `_end.png` against `OS_S2RUN=30 OS_VIEW=3`): the same gantry, cars, places, race screen; the camera
  a few pixels apart (its easing at the start). The port's race starts in view 2 (from the car)
  where Danik's DOSBox starts in view 3: the game's own first value with no settings file
  (`0x32d53`) against his saved one -- not a fault. The menu's pages were compared page by page
  before (S2-5zq); after the doubles' change the arcade page's cars come in the same waves as in
  the original's pictures.
- **The timing question, answered:** the original takes one step per picture, as long as the
  picture took (`0x4adf8`, at most 0.2 s; in Danik's DOSBox 0.062 s, 17.7 a second), and reads the
  keys each step. The port takes steps of 1/25 s (S2-5l: shorter steps change the game's own whole-
  number cuts into a sideways pull), reads the keys every picture and draws between the last two
  steps. From key to picture: the original in DOSBox 56 ms a frame plus its drawing; the port at
  most 40 ms a step plus 40 of drawing between -- no slower than the original as Danik plays it.
  Kept.
- **Not yet:** the network game's library calls (0xb4xxx-0xb5xxx, NET_SPACE) stay unanswered.

## S2-8. THE HAND REWRITE — 2026-10-15

**Why.** Danik (2026-10-15): "When everything is done, we will rewrite manually any routines so that
we conform to the law AND don't need the original binary or any interpreter." `s2_gen.inc` (464
routines, ~29,500 lines) is the game's program in another notation and is never published; the
published Screamer 2 is to carry only the port's own text, and needs neither STARTL.EXE nor an
interpreter.

**How a routine is rewritten.**
- From what this record says the routine does and from the ORDER OF ITS SUMS in the machine code
  (`objdump` of its range) -- not from the decompiler's text. Names say what things are; the
  record's words (car +0x60 "the most revs") go in the comments.
- In `port/src/s2r_*.h` (the car's: `s2r_car.h`), included by `s2_hooks.c` before `s2_gen.inc`;
  the routine keeps its address's name (`FUN_0001f697`, `#define HOOK_1f697`) while converted
  routines still call it, and its address goes on `tools/s2_set.txt`'s `--hook` list so the
  converter leaves it out.
- The x87's way, which decides the last bit: the game's runtime sets 53-bit precision (0x127f,
  `S2_FPU_IN`), so every + - x / of the machine code is one rounding to a double -- written in
  `long double` in the machine code's order; a value rounded to a float only where the game
  stores a DWORD, to a double where it stores a QWORD; the game's constants as the size it reads
  (0.3 a double is not 0.3 a float); the x87's sine, cosine, arc tangent at 64 bits (cosl ...).
- **Checked two ways:** (1) every car's whole record every frame on ten fixed races (`OS_S2TRACEFILE`
  with `OS_S2REPLAYTEST` on the seven tracks, fog, night; and the DOSBox recording's inputs through
  the race frame) against the same before the change -- the same to the byte, or different only
  from where the converted routine had an order of sums other than the machine code's, by last
  bits; (2) the DOSBox comparison (S2-5w; `tools/s2dos.py`, `tools/s2replay.py`) no worse, or
  closer. The scratch tools: `rw/traces.sh` (the ten traces), `rw/cmp.py` (the first frame, car
  and word that differ).

- A routine the converter reached only through one now written by hand must be added to the
  converter's starting list (the first words of `tools/s2_set.txt`), or it drops out.
- **The quick check** (`rw/q.sh`, ~15 minutes): the original's recorded drive through the race
  frame (4 cars, 408 frames) and 80 s of the replay test on tracks 2, 4, 7 -- every car's whole
  record every frame; the camera's place and angles every frame in views 0, 3, 4 (the trackside
  cameras); race pictures by night, in fog and by day -- all against the old program (a copy of
  the build from before the rewrite, `rw/openscreamer_base`), byte for byte.

**Done (2026-10-15), all the same as the converted ones to the byte:** `0x1f697` the gearbox,
`0x1f756` the engine; `0x111d6` `0x11212` easing; `0x12d00` `0x12dd8` `0x12d31` a camera's car,
view and the night / fog distance; `0x16824` `0x1684e` `0x167e2` the car's frame's parts; `0x12462`
`0x12553` `0x12512` `0x1248d` `0x108f5` paths; `0x11253` square root; `0x19c61` drag; `0x1240d`
a record by name; `0x14dc0` the angle between; `0x10388` `0x1030d` a point turned; `0x13223` the
camera's frame; `0x13514` a fixed camera; `0x174ca` the throttle; `0x20970` where two lines meet;
`0x1766c` a point's share along a line (summed in the game's order, which the decompiler's text did
not keep); `0x152f7` `0x1099a` a path's point; `0x1f3ce` the car's clocks; `0x1756f` the brake;
`0x13bab` `0x13e1a` `0x13cbd` the cameras in the car, the mirror's, the dashboard's; `0x13768`
`0x13a8e` `0x135b6` `0x138e8` the trackside kinds; `0x1016b` an angle eased the short way;
`0x11906` the paths' tables emptied; `0x1257f` a node into a path's ordered list; `0x19aaa` a knock
(damage); `0x1f1ef` the road's kind: the look and the reflections' speed; `0x11c0c` a set of paths
read; `0x2073c` tyre smoke; `0x14acf` which trackside camera (and the attract race's change of
car every 10 s); `0x11a28` a car's pictures let go; `0x19810` the wheels' push sideways; `0x126ad`
a point on a curve of values; `0x12dff` the camera each frame; `0x13f38` the chase camera;
`0x14200` the camera fixed to the car; `0x129d5` a moving part's place; `0x201ee` a lap done (best
lap, records, the finish); `0x204af` dust from the wheels; `0x2aeb9` sparks; `0x1ffcc` a split
passed; `0x105cd` three angles turned into three others (the converted text had lost its arc
cosines' arguments and gave 0; not reached in the test races); `0x1d9fc` the car's four corners;
`0x14e20` the car made of its set-up; `0x2348b` where a step crosses a gate; `0x1c4fb` the body
leaning on its springs; `0x19409` the steering; `0x28f96` the ground under the four wheels;
`0x1bef1` the springs; `0x1b36f` the tyres on the ground (grip, slide, squeal); `0x153c6` where
the car is along the road (the game keeps a stretch's share from its first look when it steps
along: so does this); `0x19cac` one tyre on the ground; `0x16878` the car's picture each frame;
`0x22d35` which cars may touch (the game measures their distance in x and HEIGHT, not z: kept);
`0x1b92b` the car's step on the ground; `0x29571` the car against a wall; `0x2a639` the cones;
`0x1e529` the car's speed and way; `0x1c7f8` the car's angles from its corners; `0x2cc74` the
sparks flying; `0x309b0` the road measured; `0x2baa4` the race frame; `0x2d59c` a step along a path;
`0x2c132` the time attack frame (the quick check now takes a time attack race too, tracks 1 and 4,
600 frames); `0x2da06` places and the computers' pace; `0x30b29` the cones put out; `0x31888` the
controls read; `0x2c475` the cones flying.
(`tools/s2c.py` knows the hand-written routines that give nothing back, from `s2r_car.h`: a converted
caller's `x = FUN_...()` of one becomes the call alone. The scratch helper `rw/roots.py` puts every
converted routine a hand-written one calls on the converter's starting list before each build.)
(The orders of sums suspected in `0x1f697` and `0x1f756` round the same as the converted ones here.)
The quick check also takes the camera in all five views, the trackside camera on all seven tracks
(the recorded drive's inputs on each), and a race picture with the mirror on; `rw/bq.sh` converts,
builds and checks only when the build succeeds.
The full ten-race traces of the build with the first 2 routines: the same as before, 3,000 frames
each.
**A CONSTANT IS THE NUMBER THE GAME READS, NOT ITS DIGITS (2026-10-15).** Found writing `0x19810`
(the wheels' push sideways), whose quarter turn `1.5707964f` put the result four float steps off.
Built for the x87 with `-fexcess-precision=standard` (S2-5za), C takes a floating constant at the
processor's full width: `0.8` is 0.8 to 64 bits, not the double the game's `fmul QWORD` reads;
`3.14159274f` is 3.14159274, not the float. And the decompiler prints even a double to nine
figures (`0x1b92b` compares with 1.570796370506286, the listing says `1.57079637`). So:
- `tools/s2c.py` (pass `exact_const`) matches every constant in a routine to one the routine reads
  out of the data (`fld` / `fmul` ... `DWORD` / `QWORD PTR ds:`), takes its value from there (one
  not read so: its type's nearest), and where that is not the digits writes it in hexadecimal with
  the digits in a comment: `0x1.921fb6p+1f/*3.14159274*/`. 471 constants in all.
- The hand-written routines write such a constant with a cast, `(float)3.14159274f`,
  `(double)0.8` (a cast is a real rounding to the type); the header of `s2r_car.h` says so.
- **Against the original in DOSBox** (the England drive e5, every car's whole record every frame,
  `drv/rec.py`): the first frame more than 20 words apart -- the player's car 134 -> 325, car 2
  189 -> 319, car 3 144 -> 311, car 1 158 -> 178; the words apart over the drive, the player's
  21,035 -> 8,217. This was (much of) the player's drift from frame 244 that S2-5w left unexplained.
- So the old reference program was wrong in the same way, and the reference is now rebuilt from the
  working tree each time it matters: `rw/mkbase.sh` (the converted routines alone -- the
  hand-written ones taken off the `--hook` list and `s2r_car.h` emptied, in a copy of `port/`),
  `rw/qref.sh` (the quick check's reference files from it).

**THE WALL: A STORE THE DECOMPILER DROPPED (2026-10-15).** In `0x29571` (the car against a wall) the
point handed to `0x20970` (where the car's line meets the wall's) is the car's x and z
(`fild [car+0x4d8]; fstp [ebp-0x10c]` ... `fild [car+0x4e0]; fstp [ebp-0x104]`); the decompiler kept
the x and dropped the z, so the converted text read a stale word -- and decided wrongly whether the
car spins off the wall. Found because the hand-written routine and the converted one parted at a
wall on track 2 (side by side: the line's share 461 against -36921). `tools/s2c_fix.txt` puts the
store back. Against the original in DOSBox (e5): **the player's car no more than 20 words apart from
the original's over the whole drive** (408 frames; before: from frame 325, and 134 this morning),
1,987 words apart in all (8,221); cars 1 and 2 closer again. This was the drift from frame 244
that S2-5w could not explain.

**A DOUBLE IN A SUM IS NOT ROUNDED TO A FLOAT FIRST (2026-10-15).** The decompiler writes
`(float)dVar10 / fVar1` where the machine code divides the double it holds (`0x19cac`: `fld [len];
fdivr QWORD [q]`); in C the cast is a rounding the game does not do. Found by running the hand
`0x19cac` and the converted one side by side on the same inputs (only the x of the result differed,
last bits). `tools/s2c.py` (pass `double_operands`) drops `(float)` before a `dVarN` inside a sum
(as the whole of what is put into a float it rounds there anyway); `0x16878` had it too (the angle
between, `(float)dVar4 * s0`). Against the original in DOSBox (e5): car 1 the first frame more than
20 words apart 178 -> 338, car 2 319 -> 327, car 3 the same frame with fewer words apart, the
player's car the same.

**THE GAME'S WORDS ARE READ AS FLOATS AND AS WHOLE NUMBERS (2026-10-15).** `F32(a)` and `I32(a)` are
`*(float *)` and `*(int32_t *)` of the same bytes; at -O2 GCC assumes the two never meet and moved
a whole-number read of a word before the float write of it (`0x153c6`: the half-node share +0xcc
written as 1.68, then tested as a whole number against 1.0's bits before the write -- the flag
+0xd0 came out 0 and the share stayed 1.68). `s2_hooks.c` is built with `-fno-strict-aliasing`
(port/CMakeLists.txt, on every processor). The converted routines give the same results with it
(the recorded drive, byte for byte); the hand-written ones need it.

The quick check now also takes the attract race (mode 0x80, which the trackside camera's change of
car every 10 s needs: `0x14acf`'s +0xae8), two pictures at 400 and 800 frames.

**`tools/x87sym.py`** (2026-10-15): a routine's floating-point sums written out in the machine code's
order -- each rounding (fstp / fistp) a numbered value, `t12 = f32((t3 * t4) + F[car+0x94])`, the
program's constants with their values (`0.3d` a double), stores, compares and jumps as they come,
the runtime's sine / cosine / square root / cutting named. It does not follow jumps (a value
written on two paths shows the last), so the branches are read with the decompiler's text beside
it; but the order of sums and where they are rounded -- which the decompiler loses -- it gives
straight. `python3 tools/x87sym.py build/screamer2_flat.bin 0x2348b 0x245a3 a0=p1 a1=p2`.

Also `0x326be` the race texts let go; `0x333c1` / `0x3343d` a sprite centred; `0x33635` / `0x3385b` a
point eased; `0x331dc` the settings saved (CHOICE.DAT; checked against the machine code, run only from
the 3D menu, which the quick check does not reach); `0x3327c` / `0x332dd` a line of text centred;
`0x3348f` the widest of some strings; `0x32c48` the menu's camera paths read; `0x336eb` a point list
turned round. The full ten-race traces (seven tracks, fog, night, the DOSBox drive; ~3000 frames
each) the same as the reference.

**A FLOAT THE ROUTINE WRITES INTO ITS OWN FRAME (2026-10-15).** `0x20a18` and `0x245a3` (the cars'
outlines) make the wheel corners 1.4 times as far from their mean with `mov [ebp-n],0x3fb33333;
fmul DWORD [ebp-n]` -- the float 1.4 -- and `0x20a18` takes the sine and cosine of a quarter turn
written the same way (`0x3fc90fdb`, the float pi/2: its cosine is -4.4e-8, not 6e-17). The decompiler
folds such a word into the sum as a decimal and the converter wrote the double nearest it.
`tools/s2c.py` (`routine_consts`) now also takes the floats a routine writes into its frame as the
constants it reads. The reference changed wherever cars touch -- on the grid at the start of four of
the ten test races (`rt3` frame 0, ...). (The DOSBox drive e5 has no contact that shows in what
`s2replay` prints; the machine code decides it.)

**A SLOT 8 BYTES A CAR, NOT 2 (2026-10-15).** The two players' pages (`0x3988d`, `0x3a870`) and the
set-up page (`0x3e161`) address the drawing list's model slots (`0x130f68.. / 0x130fe0..`, 8 bytes
each: model, block) as `(n * 4 + k) << 3`; the converted text had `(int)(0x130f68u + (n * 4 + k) * 2)`
-- a scale the converter lost from a typed pointer -- so those pages took and showed the wrong models.
The hand-written ones address them as the game does.

**THE LEAGUE'S CARS SET OFF FROM THE SIDES (0x35b86).** Each starts 2000 to one side and 2500 back, the
sides in turn: the game's power routine `0xafed2` (-1 to the n + 2). The port answered every call of
it with 0 (only the ones its fix file rewrote were real), so the converted page started all of them
in the middle. Written as the game has it.

**THE REST, BY HAND (2026-10-15, night).** The computer cars' driver (`0x17726`: its pace along its
line, the throttle lifted by the line's speed share, avoiding a slower car ahead -- the side that
turns the least from the line, within an eighth of a turn -- or lifting behind one), the two-car push
(`0x20a18`), the touching-cars solver (`0x245a3`, with the knock: sparks, sound, springs, the push
split along and across the line from each car's middle), the replay's frames (`0x4a488` / `0x4a59a`),
a curve's point and way (`0x4a831`), the clocks (`0x4af7d`, `0x4b016`), the drawing list (`0x4be04`
.. `0x4c01c`, `0x4bd1b`, `0x4bda7`), the menu's picture files (sprites, fonts, backdrops: `0x50a9c`
.. `0x5160b`), the track's paths' names (`0x1226b`), the game's texts (`0x3248f`), the menu rooms
(`0x11298`, `0x11d6f`), and every page of the menu: the main loop (`0x38540`, with the port's return
from its races), the title (team and name), the main menu, arcade, its tracks and cars, championship
(the leagues, a race's track, the race's choices, the set-up, the report with its points and the
leagues opened, a new league's scene), time attack, options, best times (and their replays), the
loading page, the network and two-player pages (the port has no network: their calls answer
nothing), the page loader / unloader (`0x44587` / `0x44f72`), the keys (`0x38996`, `0x38b2c`), the
cheat words, the letter wheel. The game's names these hand on (file names, formats) are in the hand
file's table `s2r_strings`; the menu's page dispatch and three small tables of the game's numbers
(the gears' ratios, the outline's edges, the wheels' block names, the fade's start) are written there
too (`s2r_tables`, `s2_dispatch`).
Not the converted text's: the loaders test the open's result (it read a stale register); the menu
lets go of its backdrop's block after each race (the converted call had lost its argument).

**No converted file in the build (2026-10-15, night).** With every routine written by hand, the
program is built from `s2r_car.h` alone: `port/CMakeLists.txt` always builds Screamer 2 and needs no
`s2_gen.inc`; the option `S2_REFERENCE` (the developer's) builds the converted routines instead --
the reference the hand-written ones are checked against (`rw/mkbase.sh` configures it so; there
`s2r_car.h` is emptied and the converter's `s2_dispatch` / `s2_gen_tables` / names are used).
`tools/s2c.py` and `s2_gen.inc` remain the developer's, for that reference only.

**Nor the game's program (2026-10-15, night).** The menu took the program's data object out of the
player's STARTL.EXE (`s2_exe.c`, S2-7); it now starts from three routines of the port's:
`s2r_strings` (the 167 names the routines use, by their addresses: `rw/names.py` writes it from
every address in `s2r_car.h`, the tables' ranges left out), `s2r_tables` (the race's small number
tables) and `s2r_menu_tables` (the menu's: the keys' letters, the cheat words, the leagues, the
set-up page's items, the points, the options' rows, each track's weather and their order, each
page's loads and frees, a room car's wheel names, the 3D's fade distances; its comment lists them
with their addresses). The STARTL.EXE read is kept for the reference build only. How it was
checked: (1) the 13 menu pages the same, picture and settings file, without the program as with
it; (2) a copy that writes the block out at the menu's start, once from the port's tables and once
from the program: of the 14 328 bytes that differ (the program's own double constants, which the
hand code writes inline, and names nothing uses), none is touched on nine longer menu walks
(`rw/touch.sh`, an instrumented copy listing every program address read) and none is at an
address the code names, nor inside a table it indexes from one (`0x112d38`, `0x112e64`, `0x112f04`,
`0x113738..44` end before the differences start; `0x113480` is the CD player's buttons, which
`main.c` keeps as its own table).

The developer's reference script `rw/mkbase.sh` takes a hand-written routine's address out of the
converter's `--hook` list only (an engine routine that is also on `--take` stays taken: the first
version took it out of `--take`, and the reference lost those routines at the link).

**The history rewritten (2026-10-16, Danik: "yes, rewrite the four local commits").** The four
unpushed commits whose files held `s2_gen.inc` (6e9c2c0, 609daef, 734d6ae, 5c02767) and the eight
after them were made again with that one file taken out (`git filter-branch --index-filter`, over
`origin/main..main`): now bc31ced, c465e43, baeae9c, 60588e9, then 290d816 (was ba5117c) and ed49408
(was 36aaf93); the six before are unchanged. Each new commit has the old one's message, author and
date, and differs from it by that file only; the last one's files are the old last one's exactly.
The old copies were then dropped from the repository (`refs/original`, the reflog, `gc --prune=now`);
`git rev-list --all --objects` names no `s2_gen.inc`. A bundle of the branch as it was is kept in
`build/w/gitbak/` (ignored; never to be published). The work in progress was set aside and put back
the same, byte for byte.

## S2-9. HIGH RESOLUTION: what STARTH.EXE changes — 2026-10-16

Danik: "start on hi-res". `S2.BAT` (the original's own launcher) runs `STARTH.EXE`, so that is the
version Danik plays.

**It is 640 x 400, not 640 x 480.** DOSBox's picture of it (Ctrl+F5) is 1920 x 1440 with 400 distinct
rows (320 of them 3 tall, 80 of them 6) and 640 columns of 3: a 640 x 400 screen shown 4:3. (The
launcher's comment "640x480" is wrong.) The port's high resolution is already 640 x 400.

**The same program with other constants.** `tools/unle.py --fixups screamer2/STARTH.EXE` ->
`build/screamer2h_flat.bin`, Ghidra project `build/ghidra2h` (as S2-3, entry 0xB3EF0, the same 792
seeds: `build/w/ghidra_h.sh`), `tools/ExportFuncs.java` lists both programs' routines (entry, size). The
routines paired in link order (`build/w/hi_pair.py`: 1277 pairs), compared with relocated addresses and
call distances left out: **88 routines differ**, every one in what is drawn and loaded:
- the picture's size: rows 200 -> 400 (`0x13514`..`0x14200` the cameras' record `+0x1c`, `0x2b873`
  the mirror's box 0x63 / 0xc7 -> 0xc6 / 0x18e, `0x4f6c4` the width x 2), the screen 64000 -> 256000
  bytes (`0x2b2a8`, `0x358b9`, `0x36d22`, `0x37b41`), a row 320 -> 640 (`0x4fbe0` shl 6 -> 7, `0x51698`,
  `0x51b02`, `0x51774`, `0x753a0`);
- the file set's letter 'L' -> 'H' (`0x34ec6`, `0x41fcf`, `0x44587` twice, `0x2e1f3`, `0x2ef4e`);
- every position on the race's screens doubled (`0x45882`, `0x456fb`, `0x46b76`, `0x4746a`,
  `0x47e75`, `0x48980`, `0x49aaf`, `0x49f8e`, `0x463c0`, `0x498d8`, `0x499d2`, `0x4962c`, `0x47128`,
  `0x37ea9` whose easing constant is 1000 -> 2000) -- what the port's `g_s` already does;
- the menu's text helpers (`0x3327c`, `0x332dd`, `0x333c1`) keep their sums in the 320 x 200 layout:
  the H font's widths are halved, the spacing 2 -> 4, the place doubled as it is drawn;
- the dashboard table (`CRUSC.DTR`, 640 units) is used whole, not halved (`0x13cbd`, `0x49071`);
- the lens: the engine's `(x << 7) / z` (0xc925b and three more) is doubled after (0xc946b in H)
  unless a flag is set -- the same view across twice the pixels, as the port draws it;
- the rest are jump tables inside the code that moved (`0x14e20`, `0x19aaa`, `0x245a3`, `0x339e7`,
  `0x38540`, `0x38b2c`, `0x4b1a4`), the engine's own drawing and the frame's copy to the card.
**None in the driving, the collisions, the computers' cars or the rules.** Both programs decompiled
side by side: `build/w/decl`, `build/decomp2h`, the differences `build/w/hl_cdiff.txt`.

**Pictures of the original at 640 x 400:** `build/w/dos/hishots.py <dir> high "Return:3,...,SHOT:1"`
(DOSBox on the hidden display, its own capture). The race screen of the port's `OS_RES=high` against
the original's: the same layout and artwork. **The game's menu is not done in high**: it draws in
the top left quarter (the plain menu too).

**The routines first paired were not all of them.** Ghidra's lists of the two programs differ where
it cut routines differently, so a second pass (`build/w/hi_pair2.py`) takes every routine start known
-- both Ghidra projects and every hand-written routine -- finds each in STARTH by its own bytes, and
compares each stretch to the next: 84 stretches differ below the engine, among them some thirty
menu pages the first pass missed (`0x3343d`, `0x3988d`, `0x3edb4`, `0x3ff37`, ...). Every file
letter of the menu, all 18 `sprintf(..., 'L')` of it, is 'H' in STARTH.
`build/w/show_a.py <addr>` lists a stretch's changed constants; `build/w/hi_odd.txt` those that
are not a plain doubling.

**In the port (2026-10-16):** `s2_screen_hi()` (main.c: `g_s == 2`, not the MiSTer's wide) and in
`s2r_car.h` `S2_HI`, `S2K` (1 / 2) and `S2_SET` ('L' / 'H'):
- the menu's primitives draw what they are given in the screen's own pixels (`s2m_sprite`,
  `s2m_text` with `hud_text`'s `g_hud_native`, `s2m_picture` and `s2m_dark` to 640 x 400, the
  clip boxes, `MENUH.FNT` / `MENUH.SPR`, the 3D's eye line 200);
- the hand-written routines take STARTH's numbers where it has others: every place handed to
  a sprite, a text, a box or a clip doubled (`build/w/scale_calls.py <addr>`, used only where
  the program's list shows nothing but doubling, its count checked against the list); a text's
  gap 2 -> 4; the text helpers halve the H font's widths and double the place
  (`0x3327c`, `0x332dd`, `0x333c1`, `0x3343d`, `0x3348f`); the screen 64000 -> 256000 bytes
  (also the block `s2_alloc`s for it: at 64000 the clear ran over the next block); the two
  players' halves 0x7f80 / 0x7a80 -> 0x1fe00 / 0x1ea00 (rows twice, each twice as long);
  a place worked out in floating point is multiplied by 2.0 BEFORE it is cut, not after
  (`0x3e161` CUSTOMIZE's sliding items, `0x4189f`'s strip); `0x420c0`'s line step is the H
  font's height x 1.2 x 0.5; the cameras' rows 200 -> 400 (the mirror's 35 -> 70, the dashboard
  view's the table's number whole);
- the port's own race screens already doubled the low set; now also in high: the attract's
  logo in the screen's pixels (from 2000, 1280; aimed at the H sprite's middle; its height 10
  NOT doubled, as STARTH has it), the dashboard from CRUSC.DTR's numbers whole and
  `COCKn_H.PKC`, the mirror's frame `MIRROR_H.PKC`, a camera's rows taken as given (doubling
  them again overran the mirror's 100-row strip: noise under the mirror);
- the picture hook (`0x51b02`) lets a picture have 400 rows.
**Checked:** the track page is the original's to the pixel (DOSBox's picture of it, its runs of
rows taken one each: `build/w/dos/unscale.py`; 0 of 256000 differ); the name page's title,
background, name and cursor the same, its turning 3D at another moment; the race screen's
labels and figures at the same places; Screamer 1's high resolution (two tracks, the menu)
unchanged; low resolution unchanged (the quick check and the 13 menu pages). `-res low|high`
chooses it for one run; the one kept is still Screamer's OPTIONS (`openscreamer.cfg`).

**A RESOLUTION line in the game's settings (2026-10-16, Danik: "add a switch for resolution in
game settings, above 'detail level'").** The port's own, on the options page (`0x420c0`): its
rows are now 0 TRANSMISSION, 1 RESOLUTION (`:320X200` / `:640X400`, the words written into the
routine's own stack in the game's memory and drawn as the game draws its lines), 2..8 the
game's DETAIL LEVEL .. MUSIC, 9 VIEW HIGH SCORES; the ten lines fit the page's box in both
sizes. ENTER on it: the game's settings saved (`0x331dc`), the menu left by the game's own exit
(`0xae662`) with `s2m_res_change` set; `s2_menu3d` (main.c) keeps the choice in
`openscreamer.cfg` (`resolution=`), sets it for the run and begins the menu again
(`os_go`) with `OS_S2MGOTO=options`: the menu's loop (`0x38540`) then goes straight to the
options page, starts the music as its first page would, and the cursor is on RESOLUTION.
Checked both ways (the settings file and the picture's size after it), DETAIL LEVEL still
steps round. On the way: the menu's floating-point mode (`S2_FPU_IN`) is now set before
the jump the game's exit comes back to, and put back after it (it was left set).
Danik's first try: (1) after the change, ESC from the page showed the options room facing a wall
-- the restart skipped the name page, which as it ends reads the team's camera paths for the rooms
(`p_<team>.mcm`, `0x32c48` into `0x131f34..`: the rooms' camera places `0x131f38`, aims
`0x131fec`); the restart now reads them, after the settings (`0x32d53`, the team), and sets the
room's choice to OPTIONS (`0x132378` = 4); (2) the music did not come back -- the first page
(`0x3c9ac`) copies the settings' volumes into the race record's (`0x130d5c` CD, `0x130d60`,
`0x130d58`) before it starts the song, which the restart did not: the song played at volume 0.
Both now as the skipped pages leave them. (3) "note: no picture at .../IMPLET_L.PKC" (and CDFRONTL,
IMPRES_L) on every exit: Screamer's race-screen pictures, asked for by the shared `hud_load`;
Screamer 2 no longer asks. Tests: `OS_S2MKEYSAFTER` = the keys after the restart; `OS_SNDWAV`
now records the menu's sound too, a frame's worth a frame; `OS_SNDLOG` prints the CD's volume.
Danik again: a start with high saved fell back to low ("the last start in it did not get as far as
a picture") and then "cannot open /MAP0OPP.SPL". The start-up mark (`starting_high=1` in
openscreamer.cfg, cleared by the first picture shown: `res_started`) was never cleared by Screamer
2's menu, so the hand-over to the attract race found it and took high for broken; and `res_fail`'s
restart handed `os_go` the very buffers `os_next` writes into (they came out empty). Now
`s2m_show` clears the mark, and `res_fail` copies its arguments first. Checked on the hidden X
display, started as Danik starts it: two starts in high in a row stay high; a start with the mark
left on falls back cleanly and its race runs.

## S2-10. Choosing the game — 2026-10-16

Danik's decision of 2026-10-13 ("with no option and both games' data found, a menu offers Screamer 1
/ Screamer 2"), now built: "do the game chooser. -game 1 starts screamer 1". With both folders found
and no `-game` (`os_both_games()`), Screamer's menu starts on a page of its own (`PAGE_GAME`, 9) of two
lines, SCREAMER and SCREAMER 2 (texts 66, 67, the port's), its lettered cubes as every page of that
menu; the line lit at first is the game chosen last (`game=2` in openscreamer.cfg). ENTER: the choice
kept, `OS_GAME_CHOSEN`, and the program begun again (`os_go`) -- SCREAMER with its intro and menu as
ever, SCREAMER 2 after `os_select_game(2)` (port.c: the data folder becomes the other one found, its
DATA / GFX / ANIMS folders found again, every container read so far let go -- the two games share
names such as GEN, LOCAL, MAP1 -- and its start-up check) with its own menu. ESC on the page leaves the
program; ESC on Screamer's main page comes back to it. `-game N` or `-data DIR` never shows it.
`OS_MENU_PAGE=9 OS_MENU_SHOT=0,0,f.ppm` takes its picture. Checked on the hidden X display: SCREAMER 2
-> its check and its menu, `game=2` kept and lit on the next start; SCREAMER -> its intro race.

## S2-11. The controller's rumble — 2026-10-16

Danik: "implement controller vibration (rumble) in screamer2, the same way we did in screamer1". The
port's own in both games (the originals had none); Screamer's machinery (main.c `rumble_kick`,
`rumble_show`, the heavy and the light motor, a knock held a moment then dying away in a fifth of a
second) fed by `s2_rumble_step` after every frame of the race, from the player's car
(`s2_feel_read`, s2_hooks.c):
- **a knock**: the game's one knock routine (`0x19aaa`, walls and other cars) notes the hardest force
  the player's car takes (`s2_knock_note`, the routine's only port line); a thump by it, 3000 = full
  (the game counts 2000 as a hard one);
- **grass**: the wheels on the ground whose road kind's flags & 3 are 2 (the green blades,
  `0x11ce00` + 0x24 a kind) -- the heavy motor by them and the speed (+0x500, ~120 at the top),
  shuddering with the place;
- **a skid**: the game's slip (car +0xc34) under 0.93 -- when its tyres smoke (`0x204af`) -- the light
  motor, more the further under; the start's wheelspin is one.
Nothing in a replay, the attract race, the pause screen, or once the player has finished. Measured
with the game's driver at the wheel: slip under 0.93 in 9 % of the frames (the start, real slides),
grass where it cuts across. Switches as Screamer's: `OS_NORUMBLE`, `OS_RUMBLE_GAIN`, `OS_RUMBLELOG`;
`OS_S2FEELLOG` prints what is read each frame. Not felt here (no controller on this machine): Danik's.

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
