# OpenScreamer

A reimplementation of *Screamer* (Graffiti / Virgin Interactive, 1995, MS-DOS) for Linux,
written by reading the original program. It plays the whole game: the six tracks and their
reversed versions, the championship, time attack, cone carnage and slalom, replays, records,
the menus and the intro, with the original's sound and its FM music.

**Beta.** Version numbers look like `0.4.12b`; `openscreamer --version` prints it.

## You need the original game

OpenScreamer contains no part of the game. *Screamer* is still sold (GOG, Steam): buy it and
copy its folder — the one that holds `DATA` and `GFX` — next to the program as `screamer`,
or name it with `-data DIR`. The game's own files are read as they are; nothing has to be
unpacked. At every start the files are checked and anything missing is listed by name.

## Building

SDL2, CMake and a C compiler:

```sh
cmake -S port -B port/build && cmake --build port/build
port/build/openscreamer
```

`openscreamer --help` lists the options, the keys and the game controller's buttons.
Settings and saves go to `~/.config/openscreamer` (or `-home DIR`).

## What is in here

| | |
|---|---|
| `port/` | the program (C, SDL2) |
| `tools/` | what was used to read the original: unpackers for its files, Ghidra scripts, viewers |
| `CLAUDE.md` | the notebook of the whole project: every file format, where each rule of the game is in the original program, what is exact and what is the port's own |
| `VERSION`, `tools/version.sh` | the version: the number in `VERSION`, its last part raised by one with every commit since |

A MiSTer FPGA core built from this source is planned as `openscreamer_MiSTer`.

## Credits

- *Screamer* is by Graffiti (later Milestone), published by Virgin Interactive Entertainment.
  This project is not affiliated with them.
- Written by Danik with Claude (Anthropic); the first day's groundwork with DeepSeek.

## License

GPL-3.0, see `LICENSE`. That covers this source only, not the game or its data.
