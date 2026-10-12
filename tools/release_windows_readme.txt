OpenScreamer @VERSION@ for Windows (64-bit)
==========================================

A reimplementation of Screamer (Graffiti / Virgin Interactive, 1995, MS-DOS),
written by reading the original program: the six tracks and their reversed
versions, championship, time attack, cone carnage and slalom, replays,
records, the menus and the intro, with the original's sound and FM music.

Beta. Please report problems at https://github.com/ItsDanik/openscreamer

YOU NEED THE ORIGINAL GAME
  OpenScreamer contains no part of the game. Screamer is still sold: buy it,
  and copy its folder -- the one that holds DATA and GFX -- next to the
  program, named "screamer":

      openscreamer.exe
      SDL2.dll
      screamer\DATA\GEN.DAT ...
      screamer\GFX\GAME.DAT ...

  The game's own files are read as they are; nothing has to be unpacked.
  At every start the files are checked; if something is missing a box says
  what.

RUN
  Double-click openscreamer.exe. Windows 10 or 11, 64-bit.
  (Windows may warn about a program from an unknown publisher: "More info",
  then "Run anyway".)

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
          contact, grass and skids.

  OPTIONS in the game's menu has the picture's size (320x200 or 640x400),
  the detail, difficulty, laps, speech and music.

FILES IT KEEPS
  %APPDATA%\openscreamer\   settings, best times, championship, replays,
                            time-attack ghosts, and openscreamer.log (what
                            the program has to say; look there if it does
                            not start)

  From a command prompt:  openscreamer.exe --help   lists the options;
  -data <folder> names the game's folder, -home <folder> the settings'.

LICENCE
  GPL-3.0 (LICENSE.txt). The FM chip is Nuked OPL3 by Nuke.YKT,
  LGPL-2.1 or later (LICENSE-nuked-opl3.txt). SDL2.dll is the Simple
  DirectMedia Layer, zlib licence (LICENSE-SDL2.txt).
  Source: https://github.com/ItsDanik/openscreamer
