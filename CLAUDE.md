# OpenScreamer — porting *Screamer* (1995, MS-DOS) to Linux, Windows and the MiSTer

Reverse-engineering + reimplementation. A native SDL2 port (Linux x86/ARM, Windows), and
a MiSTer hybrid core (game on the ARM, FPGA for 15 kHz video / audio / input / FM chip).

**This file is the summary. `docs/NOTES.md` is the full record** (this file as it stood
on 2026-10-12, before compaction): every address, proof, measurement and dead end, in
sections numbered 1–32. **Before working on an area, read its section there** (the index
below gives the numbers). New findings: detail into `docs/NOTES.md`, one line here.

---

## Working agreement — read this first

**Danik does not write code or decompile.** He is the domain expert on the MiSTer and on
these ports, and he does verification and game-feel judgement. Do not hand him code to
review, patches to apply, or compiler errors to triage.

- **Claude:** all code, tooling, decompilation, builds, debugging. This machine has
  Ghidra 12.1.2 (`/opt/ghidra`), python3, wine, objdump. The `Read` tool opens PNGs:
  *look at rendered output yourself*; verify every change with `--shot` before handing over.
- **Danik:** real hardware, whether it looks / feels like the original (his original runs
  in DOSBox, not on this machine), scope / licence / target decisions.

Prefer "here is a build and a screenshot" over "please check this function". Keep
everything buildable with one command. Plain language, not jargon.

---

## The game's files

`screamer/` holds what the project uses: `STARTL.EXE` (decompiled), `STARTH.EXE` (the
hi-res program, a separate link, **unread**), `DATA/` (GEN LOCAL MAP1-6 + `I` variants
MUSIC SOUND `.DAT`), `GFX/` (GAME GRAF MENU MAP1-6{L,H} `.DAT`), `CONFIG/CONTROLS.CFG`
`DEFCTRL.CFG`, and `HMIDRV.386` / `HMIDET.386` / `HMIMDRV.386` (read for the mixer and the
music driver; the port does not use them). In older notes `DATA/…` means `screamer/DATA/…`.

- **The port reads the containers directly** (`port.c` `packed_open`): code asks for
  `build/dec/<CONTAINER>/<FILE>` and `os_fopen` answers out of the `.DAT` (LZW in C,
  `fopencookie` stream; a temp file on Windows). **Trap:** `fmemopen(NULL,n,"w+")` gave
  subtly different pictures — do not go back to it. `OS_PACKDUMP=<dir>` dumps what is read.
- **Start-up check** (`game_check`): 16 DATA + 9 low-res GFX containers needed, six
  `MAPnH` optional; every file must unpack to its listed size; else exit 1.
- `build/dec/` (unpacked files, for the Python tools) is made by `tools/extract.sh`.

**Legal: the game is still sold (GOG, Steam). Never publish `screamer/` or anything made
from it** (`build/dec`, `build/png`, `build/screamer_flat*.bin`, `build/decomp`). Check
`git status` before every commit. `screamer2/` holds the Screamer 2 game files (ignored; support for it is the next work).

---

## The binary and how to decompile it

`STARTL.EXE`, md5 `a83834098e965ebc4bde48d0162542b1`, 1,034,919 bytes. Watcom C/C++32,
DOS/4GW stub + **LE** image. Objects: 1 code at `0x010000` (898,979), 2 BSS at `0x0F0000`,
3 data+stack at `0x100000`. Entry `0x2FEB8` (Watcom CRT). No symbols.

**Relocation (settled empirically):** references in the file are object-relative —
data target = stored + `0x100000`, code target = stored + `0x010000`. This holds for
`push` immediates and for tables too (e.g. filler table "0x722e0" is at flat `0x822e0`).
The transform cannot tell a pointer from a coincidental constant (`0xFF` → a string):
where it matters, require the target to be a known string start / function entry.
Never conclude anything from raw byte-pattern scans; enumerate real operands via Ghidra.

```bash
tools/ghidra-ext/install.sh       # once: Watcom __watcall cspec as x86:LE:32:watcom (user-level)
python3 tools/unle.py screamer/STARTL.EXE -o build/screamer_flat.bin --map build/screamer_layout.json
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer -import build/screamer_flat.bin \
    -processor x86:LE:32:watcom -loader BinaryLoader -loader-baseAddr 0x0 -noanalysis \
    -scriptPath tools -postScript CreateEntry.java
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer -process screamer_flat.bin -analysisTimeoutPerFile 1800
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer -process screamer_flat.bin -noanalysis \
    -scriptPath tools -postScript RelocateRefs.java
/opt/ghidra/support/analyzeHeadless /tmp/ghix screamer -process screamer_flat.bin -noanalysis \
    -scriptPath tools -postScript DecompFunc.java 0x10bfc
```

`__watcall`: args in EAX, EDX, EBX, ECX, then stack; return EAX; caller cleans.

**For the FPU-heavy game logic:** `python3 tools/patch_fpu.py build/screamer_flat.bin
build/screamer_flat_fpu.bin` (inlines Watcom's helpers), import as its own project
(`/tmp/ghix2`, `-noanalysis`), decompile with `tools/DecompAt.java <entry>...`.
**`0x4d7b4` truncates, it does not round**: Ghidra's `ROUND()` is a `(int)` cast.
Ghidra drops stack-passed float args to `0x22b60` / `0x22bcc` / `0x220fc` and to `pow`:
read those from `objdump -D -b binary -m i386 -M intel`. A label `_LAB_00018fe4_4` means
`0x18fe4 + 4`. A table in the data object may be overwritten at start-up: look for its
loader before trusting the image's values.

Pitfalls: Ghidra has no LE loader (import the flat image, never the EXE); no PyGhidra in
headless (scripts are Java, need `-scriptPath`); never pipe headless output through `tail`
(log to a file); keep `unle.py`'s short-page assert.

Decompiled text is kept in `build/decomp/*.c` (player_update, collision, camera,
opponent, hud_text_sprites, timers, race_end, props_and_animations, smoke, sound, menu,
menu_scene, modes, time_attack, championship, replay, records, cheats, demo_camera,
remaining_logic, hmi_fmmidi). All of the game-logic range `0x10000`–`0x30000` has been
read; unread: the network code's insides, the 3D engine / drivers the port replaces.

---

## File formats (all solved; details in NOTES "Asset files", "How the track is built")

- **`.DAT` container:** data, then directory of 32-byte records (16-byte name XOR `0x3A`,
  offset, stored size, unpacked size, method), then uint32 directory size. Method 0 =
  stored, 1/2/3 = **LZW** (LSB-first, 9–12 bits, clear 256, end 257, no header).
  `tools/dat_container.py list|extract`, `tools/lzw.py`.
- **`.3DX` / objects in `MAPn.UNR`:** header `+0x14` count, `+0x18` offsets; block header
  `+0x00` bounding radius, `+0x0C` position record, `+0x10` vertices, `+0x14` faces,
  `+0x18` flags (`0x2000` reflective, `0x800` hub, `0x1` tyre, `0x200` body), `+0x1C`
  vertex array (int16 xyz), `+0x20` per-vertex normals or −1, **`+0x24` per-vertex UVs**
  (int16 u, v, 0), `+0x28..+0x44` eight per-octant face-order rows. Face = 36 bytes:
  `+0x02` flat colour, `+0x04` material (byte offset in MLB), `+0x06` n, `+0x08` closed
  loop of vertex byte offsets (/6), `+0x14` normal (16384 = 1, points AWAY from the
  visible side). Local Y is up. Use the stored winding as is.
- **`MAPn.UNR`:** blob size, object count, offsets. **`MAPn.MMP`:** two-level 256×256 grid;
  cell = five (base, count) pairs; runs 0/1 = 64-byte piece records (`V`, X, HEIGHT, Y,
  heading `T`, 2048 = a turn); **run 0 = the drivable area**; array 2 = road centre
  points (angle, x, y), array 3 = wall segments (angle, mid, two ends).
- **`MAPn.MLB`:** 16-byte material = filler routine (16 solid, 64 colour 0 transparent,
  224 see-through via SMG, 128 flat colour from the face, 160 reflection), texture
  record, U / V extent; texture = frame count + frame offsets; pixels are 256 bytes a
  row always. **`MAPn.SMG`:** 64 × 256 blend table (row 0 identity). `.COL` = VGA palette.
  `.BCK` = 512×256 PCX backdrop.
- **`.SPL`:** SEVERAL splines per file: `count, start_node, count × 56 bytes`. Follow
  `next` (`+0x10`). The game's curve is a quadratic B-spline over the HANDLES (`+0x14`,
  `+0x20`), two pieces a node. `OPP` = 9 opponents + player (start nodes = the grid);
  `ANM` moving scenery; `CAM` intro cameras; `RPL` trackside cameras; `OST` / `CNG` /
  `SLA` = PROP LAYOUTS by game mode (height field = prop kind).
- **`.MRK`:** 32-byte lap markers: radius, kind (1 tunnel), `+0x08` opponents' pace,
  `+0x0C` slip, flags (2 line / checkpoint, 4 "bend" commentary), x, y. `SLA.MRK` = gates.
- **`.DTR`:** plain text tables, values prefixed `d` `f` `x` `s`. `CARS.DTR`, `MAPn{A..E}.DTR`
  (letter = difficulty), `MAPnDAT.DTR` (`OSTACLES` — the typo is the data's — `ANIMATIONS`,
  `SOUND_FX`), `SFXSBASE.DTR`, `MENU.DTR`. Texts: `LOCAL.DAT` `.TXT`.
- **`.PKC`** run-length pictures; **`.FNT`** multi-font, 2 pixels a byte, 16-colour ramps;
  **`.SPR`** sprite sheets; `GRAF.DAT` plain PCX. Sound: `.RAW` 8-bit unsigned 11025 Hz.
  Music: `MUSICn.HMP` (HMI MIDI) + `MELODIC.BNK` / `DRUM.BNK` (AdLib banks).
- Tools: `render3dx.py`, `render_track.py`, `mmp_dump.py`, `mlb_dump.py`.

---

## The port (`port/`)

```
port/src/main.c      materials, rasteriser, cameras, race, HUD, menus, modes (most of it)
port/src/mesh.c unr.c scene.c   objects, MAPn.UNR, MAPn.MMP
port/src/drive.c     the player's car (header comment lists every game routine)
port/src/sound.c music.c fmchip.c nuked/   mixer, HMI music player, OPL chip (Nuked OPL3, LGPL)
port/src/controls.c  keys, CONTROLS.CFG, game controller
port/src/port.c      one-program glue, directories, containers, Windows differences
port/src/mister.[ch] the MiSTer core's side, behind MISTER_HYBRID
port/attic/          dead first versions, kept as a record only
```

```bash
cmake -S port -B port/build && cmake --build port/build
port/build/openscreamer                       # intro, menu, race
port/build/openscreamer build/dec/MAP1 1      # straight into a race on track 1
port/build/openscreamer build/dec/MAP5 5 --shot out.ppm 300   # one headless frame
SDL_VIDEODRIVER=dummy OS_DRIVE_DEMO=3000 port/build/openscreamer build/dec/MAP1 1   # robot lap
tools/release_linux.sh ; tools/release_windows.sh             # dist/ packages
```

**Headless tests must set `OPENSCREAMER_HOME` to a scratch directory** — they can
overwrite records, ghosts and the name. `openscreamer --help` lists options, keys, files.

**Design:** 320×200 software framebuffer (HIGH = 640×400, the game's two sets, both
shown 4:3; `OS_TALL` / `OS_WIDE` are the MiSTer's sizes); SDL2, linked **dynamically**
(the state restore between parts puts the program's `.data` / `.bss` back — a static SDL
would be inside it; on Windows SDL2 must stay a DLL); a depth buffer, not the engine's
painter's sort; fixed 50 Hz physics; triangles filled by two threads.
Lens: `screen = centre + coord × 128 / depth`, eye line 65 % down. 1 m = 328 units.
One process: `main()` loops over `session()`; parts hand over with `os_next` / `os_go`.
Data directory: `-data`, `$OPENSCREAMER_DATA`, `./screamer`, … ; home: `-home`,
`$OPENSCREAMER_HOME`, cwd if it holds the port's files, else `~/.config/openscreamer`
(`%APPDATA%\openscreamer`). Files: `openscreamer.cfg`, `.records`, `.champ`,
`.replays/`, `.ghosts/` — the port's own formats, never the game's `CHOICE.DAT`.

**Keys:** arrows drive, A/Z gears, C / F1–F4 views, F5 FPS, F11, F12 shot, M radar zoom,
P CD player, BACKSPACE back on the road, ESC = PLAY AGAIN?. `-d` developer mode: TAB /
1–4 (DRIVE RIDE WALK FLY), mouse look, `[` `]` HOME, V, I, G, K, L, N.

**Test switches** (environment; more in NOTES by section): `OS_DRIVE_DEMO=n` robot,
`OS_CAR`, `OS_VIEW`, `OS_LAPS`, `OS_TIMER`, `OS_DIFF=0..4`, `OS_MODE=cones|slalom|attack`,
`OS_GHOST`, `OS_CHEAT=<word|bits>`, `OS_UNLOCK=1`, `OS_RES=low|high`, `OS_DETAIL=0..3`,
`OS_INTRO=1`, `OS_NOINTRO`, `OS_MENU_SHOT=screen,pick,file`, `OS_MENU_PAGE`,
`OS_MENU_KEYS="UDLREXS"`, `OS_MENU_STEPS`, `OS_POS=x,y,z` + `OS_HEADING`, `OS_YAW`,
`OS_PITCH`, `OS_OVERHEAD`, `OS_INSPECT`, `OS_ANIMCLOCK`, `OS_SNDWAV=<file>`, `OS_SNDLOG`,
`OS_MUSIC=n`, `OS_MUSICVOL`, `OS_SFXVOL`, `OS_SFXLP`, `OS_MUSICLOG`, `OS_PADTEST`,
`OS_REPLAYCHECK`, `OS_CYCLETEST`, `OS_LOAD_SHOT`; off-switches `OS_NOCARS NOOPP NOHUD
NOTIMER NOPROPS NOANIMS NOSOUND NOMUSIC NOSPEECH NOREPLAY NOCRASH NOLEAN NOSNAP NOTIE
NORUMBLE`; dumps `OS_WALLCHECK HOLES TRACKMASK WHATAT HITDUMP GROUNDDUMP OPPDUMP PROPDUMP
MODELOG TILTCHECK STACKED DOPPLERLOG INTRO_LOG`.

---

## What was read out of the game — index into `docs/NOTES.md`

| § | area | the essentials |
|---|---|---|
| 1–5 | track | The track IS the MMP-placed UNR pieces; UVs per vertex; no lighting; Bezier / B-spline paths |
| 6–7 | cars, `.DTR` | 12 blocks a car, each with its own position; nose towards local −Z; 13 cars = 7 models × 2 |
| 8 | driving | player record `0xf918`; race frame `0x2002f`; engine, brake, steering, skid, grass, ground contact |
| 9 | walls | run 0 = drivable; wall segments; four body corners at the wheels; wall response `|sin|⁴` |
| 10 | cameras | view table `0x1da0`, player gets rows 0, 1, 2, 6; fly-across on change; cockpit `.PKC` + needles |
| 11 | opponents | follow their spline at `max_spd × pace`; two-circle car collisions; one overtaking rule |
| 12 | race screen | fonts, sprites, clocks, countdown (15 s, + left/5 + bonus), radar, start lights, end screen |
| 13 | backdrop | copied 512×256, twice round per turn |
| 14 | props, moving scenery | props from the mode's layout spline; scenery moves only within 52000 units |
| 15 | animation | clock 3.33 s; texture frames; car frames (paint / brake / slow); moving parts; wheels, shadow, smoke, lean, reflections, crash |
| 16 | heading snap | stored `T` slightly short; the port snaps to 5° (its own) |
| 17 | sound | sample = row of `SFXSBASE.DTR`; engine pitch; commentary events and triggers; Doppler; pan; song order from `CD_TRACKS` |
| 18 | menus | pages, items, assets; mesh limits 2560 / 1792 ("array full" is never harmless) |
| 19 | gameplay | difficulty = data-file letter (default B); what the cameras follow |
| 20 | special modes | cone carnage, slalom, time attack + ghost (`.ATK`) |
| 21 | championship | 53 stages, four leagues, points 9/7/5, targets 42/46/50/54 |
| 22 | replay | the game records CONTROLS and re-runs the race; PLAY AGAIN? |
| 23 | records, name, unlocks | five laps + five totals a track; name before every game; level unlocks tracks |
| 24 | reversed tracks | 7–12 = `MAPnI.DAT` + road direction turned |
| 25 | cheats, menu sounds | TAZOR VTELO INVER ABURN MONTY JOINT CLOCK MIRRO UPDOW; saved replays |
| 26 | menu scene, intro | one 3D scene, letters are cubes; attract race with credits and per-car cameras |
| 27 | hi-res, settings | hi-res = low doubled (layout not read from STARTH); `openscreamer.cfg`; crash fall-back |
| 28 | CD player, last pass | every race fades 0.5 s; listener = player's car (camera in the intro); tick = 1/100 s |
| 29 | one program | state save / restore, directories, developer mode |
| 30 | the audit | DETAIL = far plane (27808 / 27808 / 43808; ULTRA is the port's); `track_under` = first face in the game's order; running order; lop-sided skid; smoke; props tilt; carpets and same-place objects; HMI mixer read (no filter in it) |
| 31 | loading, controls | loading screen; `CONTROLS.CFG`; controller layout (port's own); revving on the grid |
| 32 | music | HMI player in STARTL + `fmmidi.com` from `HMIMDRV.386` → OPL2 register writes; nine voices |

## Hard-won rules — do not rediscover

- **When a format question stalls, find the code that consumes the field.** Statistics
  and plausible pictures agreed on wrong answers for days.
- **If you are adding a knob, a per-material table or a "skip these pieces" rule, stop:**
  the data already says it.
- Dead ideas (never resurrect): textures as 256×16 strips; lit/shaded colour pairs;
  world-space UVs; reordering face corners; `T` "is not a heading"; a road built from
  splines; the port's own road-edge measurement; a car's size from its model (track 4's
  includes the headlight beam); skid marks (there are none); view-table row 5 as a
  driving view; the striped wall `V81` "missing half" (it is right).
- Sign traps: the game's wheel `track` is negative (car leans the wrong way otherwise);
  the port's lean is + when the RIGHT side is higher; the skid really is lop-sided.
- Only the hub disc spins, not the tyre block. A car drawn anywhere needs a `car_pose`.
- Per-FRAME quantities in the game (Doppler, lean kicks, prop lift, wheel turn): the port
  assumes 30 frames/s.
- Opponents: no smoke, no suspension, never hit props. Opponents never swap places among
  themselves. `beat` and `well` commentary have no trigger in the game.
- Overlaps in the data (two objects in one place, grass "carpet" tiles above the road)
  are settled in `raster_tri`, not by editing data.
- Orbit a car with `OS_POS` / `OS_HEADING` shots from eight sides; one side-on shot
  misses things.

## The port's own (not the game's)

By necessity: depth-buffer renderer, SDL mixer and pan law, the 3.2 kHz output filter on
effects, chip loudness against effects (2.6 × `OS_MUSICVOL`), 50 Hz step, save formats,
the CD player's stand-in songs. By decision: heading snap; the wall put-back (30 units
INSIDE the line, never outwards); ULTRA detail; hi-res layout = low doubled; BACKSPACE;
developer mode; one-second loading hold; controller layout; rumble.

## Open

1. Danik to judge: a real controller (§31); the lop-sided skid, the signs' grey backs,
   the heading snap, the music's loudness (§14, 16, 17, 30); smoke "a bit darker" in the
   original and the engine's timbre — both unexplained, a DOSBox recording would help.
2. Not built: network game, the attract mode that plays a replay file (mode bit `0x80`),
   the CD's eject, replays kept with the game's records; STARTH.EXE unread; `fmmidi3.com`
   (OPL3) unread; the 84 MB of static arrays.
3. Windows build never tried on real Windows; Linux release not on another distribution.

---

## Repository, version, releases

`github.com/ItsDanik/openscreamer` (public, GPL-3): `port/`, `tools/`, `docs/`, this file,
`README.md`. `.gitignore` keeps out the game, `build/`, Ghidra's project and what the
program writes. Version = `VERSION` + one per commit since it changed
(`tools/version.sh`; `openscreamer --version`); nobody edits a number to commit.
Releases: `tools/release_linux.sh` (Debian 11 container, glibc 2.27, SDL2 ≥ 2.0.9) and
`tools/release_windows.sh` (MinGW-w64, tested under wine only; under wine's dummy video
use `SDL_RENDER_DRIVER=software`); GitHub release `v<version>`, pre-release while beta.

**MiSTer:** the core is `ItsDanik/openscreamer_MiSTer` (`~/MiSTer-openscreamer`, released;
its own CLAUDE.md), taking this source as a submodule. Branch `mister` is kept equal to
`main`: work on either, fast-forward the other. The FM chip is in the framework
(`~/Hybrid_MiSTer`, host version 10, jtopl); there only `fmchip.c` is replaced.
