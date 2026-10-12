OpenScreamer @VERSION@ for Linux (x86-64)
=======================================

A reimplementation of Screamer (Graffiti / Virgin Interactive, 1995, MS-DOS),
written by reading the original program: the six tracks and their reversed
versions, championship, time attack, cone carnage and slalom, replays,
records, the menus and the intro, with the original's sound and FM music.

Beta. Please report problems at https://github.com/ItsDanik/openscreamer

YOU NEED THE ORIGINAL GAME
  OpenScreamer contains no part of the game. Screamer is still sold: buy it,
  and copy its folder -- the one that holds DATA and GFX -- next to the
  program, named "screamer":

      openscreamer
      screamer/DATA/GEN.DAT ...
      screamer/GFX/GAME.DAT ...

  (or anywhere, and start the program with  -data <that folder>). The game's
  own files are read as they are; nothing has to be unpacked. Upper or lower
  case does not matter. At every start the files are checked and anything
  missing is listed by name.

YOU NEED SDL2
  Any x86-64 Linux of 2018 or later (glibc 2.27) with SDL2 installed:
      Debian, Ubuntu, Mint:   sudo apt install libsdl2-2.0-0
      Fedora:                 sudo dnf install SDL2
      Arch:                   sudo pacman -S sdl2
  (Steam's and most games' installations already brought it.)

RUN
      ./openscreamer
  from a terminal, in this folder. If it says "permission denied":
      chmod +x openscreamer

  ./openscreamer --help   lists the options, the keys, the game controller's
                          buttons and the files it keeps.

KEYS
  Menus:  arrows, ENTER, ESC.
  Race:   UP / DOWN accelerate / brake, LEFT / RIGHT steer,
          ALT / CTRL (or A / Z) gears on cars with manual transmission,
          C next view (F1 bumper, F2 cockpit, F3 near chase, F4 far chase),
          M radar, P the CD player (songs and volumes),
          BACKSPACE back onto the road, ESC leave (it asks first),
          F5 FPS counter, F11 full screen, F12 screenshot.
  Game controller: left stick or d-pad steers, right / left trigger (or A / B)
          accelerate / brake, shoulders change gear, Y view, X radar,
          START = ENTER, BACK = ESC. It rumbles on wall hits, crashes,
          contact, grass and skids (OS_NORUMBLE=1 before the command
          switches that off).

  OPTIONS in the game's menu has the picture's size (320x200 or 640x400),
  the detail, difficulty, laps, speech and music.

FILES IT KEEPS
  ~/.config/openscreamer/   settings, best times, championship, replays,
                            time-attack ghosts (or -home <folder>)

LICENCE
  GPL-3.0 (LICENSE.txt). The FM chip is Nuked OPL3 by Nuke.YKT,
  LGPL-2.1 or later (LICENSE-nuked-opl3.txt).
  Source: https://github.com/ItsDanik/openscreamer
