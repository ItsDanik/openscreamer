OpenScreamer @VERSION@ for Linux (x86-64)
=======================================

A reimplementation of Screamer (Graffiti / Virgin Interactive, 1995) and
Screamer 2 (Milestone / Virgin Interactive, 1996), both MS-DOS, written by
reading the original programs.
  Screamer:   the six tracks and their reversed versions, championship,
              time attack, cone carnage and slalom, replays, records, the
              menus and the intro, with the original's sound and FM music.
  Screamer 2: the game's own 3D menu, arcade, the championship's four
              leagues, time attack and its ghost, the seven tracks with
              their weather, set-ups, damage, replays and best times, the
              CD's music; 320x200 or 640x400. Not in it: two players, the
              network game, the 65,000-colour versions.

Beta. Please report problems at https://github.com/ItsDanik/openscreamer

YOU NEED THE ORIGINAL GAMES
  OpenScreamer contains no part of either game. Both are still sold: buy
  them, and copy each game's folder -- the one that holds DATA and GFX --
  next to the program, named "screamer" and "screamer2":

      openscreamer
      screamer/DATA/GEN.DAT ...
      screamer/GFX/GAME.DAT ...
      screamer2/DATA/GAME.DAT ...
      screamer2/GFX/GAME1.DAT ...
      screamer2/music/track_02.ogg ...   (Screamer 2's CD music, see below)

  (or anywhere, and start the program with  -data <that folder>). The game's
  own files are read as they are; nothing has to be unpacked. Upper or lower
  case does not matter. At every start the files are checked and anything
  missing is listed by name.

  With both folders there, a first page asks which game to play (the last
  one chosen is lit); -game 1 or -game 2 starts one straight away.

  Screamer 2's music was the CD's audio tracks: put them in a folder "music"
  inside "screamer2", one file a track named by its number on the disc --
  track_02.ogg, track_03.ogg ... (.ogg, .mp3 or .wav). Without them it plays
  without music.

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

  Screamer 2: R (or M) the mirror; ESC pauses, with the CD player.

  OPTIONS in Screamer's menu has the picture's size (320x200 or 640x400),
  the detail, difficulty, laps, speech and music; GAME SETTINGS in Screamer
  2's options room has its own (RESOLUTION, above DETAIL LEVEL).

FILES IT KEEPS
  ~/.config/openscreamer/   settings, best times, championship, replays,
                            time-attack ghosts (or -home <folder>)

LICENCE
  GPL-3.0 (LICENSE.txt). The FM chip is Nuked OPL3 by Nuke.YKT,
  LGPL-2.1 or later (LICENSE-nuked-opl3.txt).
  Source: https://github.com/ItsDanik/openscreamer
