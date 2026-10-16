# OpenScreamer

A reimplementation of *Screamer* (Graffiti / Virgin Interactive, 1995, MS-DOS) and *Screamer 2*
(Milestone / Virgin Interactive, 1996, MS-DOS) for Linux and Windows, written by reading the
original programs.

- **Screamer**: the whole game — the six tracks and their reversed versions, the championship,
  time attack, cone carnage and slalom, replays, records, the menus and the intro, with the
  original's sound and its FM music.
- **Screamer 2**: the game's own 3D menu with its rooms, arcade, the championship's four
  leagues, time attack with its ghost car, the seven tracks with their weather, the car set-ups,
  damage, replays and best times, the attract race, its sound and the CD's music — in 320 x 200
  or 640 x 400, as its two original programs. Not in it: two players and the network game, and
  the 65,000-colour versions.

**Beta.** Version numbers look like `0.4.12b`; `openscreamer --version` prints it.

## You need the original games

OpenScreamer contains no part of either game. Both are still sold (GOG, Steam): buy them and copy
each game's folder — the one that holds `DATA` and `GFX` — next to the program, *Screamer*'s as
`screamer` and *Screamer 2*'s as `screamer2` (or name one with `-data DIR`). The games' own files
are read as they are; nothing has to be unpacked. At every start the files are checked and
anything missing is listed by name.

With both folders there, a first page asks which game to play (the last one chosen is lit);
`-game 1` or `-game 2` starts one straight away.

**Screamer 2's music** was the CD's audio tracks. Put them in a folder `music` inside
`screamer2`, one file a track, named by the track's number on the disc: `track_02.ogg`,
`track_03.ogg` ... (`.ogg`, `.mp3` or `.wav`). Without them the game plays without music.

## Download

Built programs are on the [releases page](https://github.com/ItsDanik/openscreamer/releases):

- **Linux** (x86-64): `openscreamer-<version>-linux-x86_64.tar.gz`. It needs SDL2 installed
  (`libsdl2-2.0-0`, `SDL2` or `sdl2`, by distribution) and runs on any distribution of 2018 or
  later. Unpack it, put the games' folders next to the program as `screamer` and `screamer2`, start
  `./openscreamer`.
- **Windows** (10 or 11, 64-bit): `openscreamer-<version>-windows-x86_64.zip`. Unpack it, put the
  games' folders next to `openscreamer.exe` as `screamer` and `screamer2`, and double-click the
  program. Settings and saves go to `%APPDATA%\openscreamer`.

## Building

SDL2, CMake and a C compiler:

```sh
cmake -S port -B port/build && cmake --build port/build
port/build/openscreamer
```

`openscreamer --help` lists the options, the keys and the game controller's buttons.
Settings and saves go to `~/.config/openscreamer` (or `-home DIR`).

The picture's size is chosen in each game's settings: RESOLUTION in *Screamer*'s OPTIONS and in
*Screamer 2*'s GAME SETTINGS (320 x 200 or 640 x 400, both shown 4:3); `-res low|high` sets it for
one run.

## What is in here

| | |
|---|---|
| `port/` | the program (C, SDL2) |
| `tools/` | what was used to read the original: unpackers for its files, Ghidra scripts, viewers |
| `CLAUDE.md` | the summary of the whole project: the working rules, the file formats, where to look |
| `docs/NOTES.md`, `docs/NOTES2.md` | the full record for each game: every file format, where each rule of the game is in the original program, how it was checked, what is exact and what is the port's own |
| `VERSION`, `tools/version.sh` | the version: the number in `VERSION`, its last part raised by one with every commit since |

The same source runs on the MiSTer FPGA as a hybrid core:
[openscreamer_MiSTer](https://github.com/ItsDanik/openscreamer_MiSTer) (the code for it is in
`port/src/mister.c` and behind `MISTER_HYBRID`; an ordinary build has none of it).
`tools/release_linux.sh` and `tools/release_windows.sh` (MinGW-w64, on Linux) make the releases.

## Credits

- *Screamer* is by Graffiti (later Milestone), *Screamer 2* by Milestone, both published by
  Virgin Interactive Entertainment. This project is not affiliated with them.
- Written by Danik with Claude (Anthropic); the first day's groundwork with DeepSeek.
- The music's FM chip is [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) by Nuke.YKT
  (`port/src/nuked/`, LGPL 2.1 or later).
- *Screamer 2*'s music files are read with [stb_vorbis](https://github.com/nothings/stb) by Sean
  Barrett and [dr_mp3 / dr_wav](https://github.com/mackron/dr_libs) by David Reid
  (`port/src/third_party/`, public domain or MIT-0).

## License

GPL-3.0, see `LICENSE`. That covers this source only, not the games or their data.
`port/src/nuked/` keeps its own licence (LGPL 2.1 or later), and `port/src/third_party/` its
own (public domain or MIT-0), stated in their files.
