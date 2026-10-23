/*
 * mister.h - the MiSTer hybrid core (branch `mister` only): the game runs on
 * the MiSTer's ARM, the OpenScreamer FPGA core shows the picture at 15 kHz
 * and has the sound, the input and the music's FM chip.  SDL's "mister"
 * drivers (the framework's, hybrid/sdl2) do most of it; this is the rest.
 * Without the core every call here does nothing, and the program runs as the
 * PC's does (which is how it is tested, against hybrid/tools/fakecore).
 */
#ifndef OS_MISTER_H
#define OS_MISTER_H

#include <stdint.h>

int      mister_on(void);               /* the core is there */
/* the core's OSD option Resolution: bit 0 = 240 rows (more of the 3D view;
   the game's 200-row artwork is not stretched), bit 1 = 640 columns (the
   high set's flat artwork, every second row of it) */
int      mister_res_mode(void);
/* the HUD margins of the core's CRT Options (HUD (Top), HUD (Bottom)): rows
   between the race's top / bottom things and that edge of a 240-row picture,
   0..24 (10 without the core) */
void     mister_hud_margins(int *top, int *bottom);
/* the OSD's lines that are greyed or hidden: HUD (Top) / (Bottom) while the
   picture has 200 rows, each game's cheats in the other game */
void     mister_menu_mask(int tall, int game);
/* the core's OSD option Rumble: 0 off, 1 on, 2 strong */
int      mister_rumble_on(void);
/* the OSD's Cheats page (core/OpenScreamer.sv).  Screamer's switches as the
   bits of its option word (the typed words' 0x1d270): 1 / 2 the props' second /
   third set, 4 the countdown stands still, 0x10 mirrored, 0x20 upside down;
   and Afterburner Difficulty */
int      mister_cheats(void);
int      mister_cheat_afterburner(void);
/* the triggers selected since the last call: bit 0 = T[56] .. bit 3 = T[59]
   (Screamer: the Bullet, all the tracks; Screamer 2: all the tracks, the
   fifth cars, all the leagues).  Called from the menus only */
int      mister_triggers(void);
/* the clock: milliseconds counted in the core's fields (59.64 a second), so
   that a picture is one field of game time however long it took to draw */
unsigned mister_ticks(void);

/* the music's chip in the core (fmchip.c asks): 1 when it is there and on */
int      mister_fm_start(void);
void     mister_fm_write(int reg, int value);
void     mister_fm_at(int frame);       /* where in the block of audio being made, in 44.1 kHz frames */
void     mister_fm_gain(float gain);    /* 1 = the chip as it comes */

/* the core's joystick is given SDL's game-controller names, in the order of
   the core's button list ("J1" in core/OpenScreamer.sv): Accelerate = A,
   Brake = B, Gear Up / Down = the shoulders, View = Y, Radar = X, Back, Start;
   the OSD's Menu OK / Menu Back arrive as the two stick buttons, the core's
   own Menu OK / Menu Back buttons as MISC1 and PADDLE1 */
void     mister_pad_mapping(void);
/* once a picture: with OS_FPSLOG=1, pictures a second and the time a picture
   took to make, every ten seconds */
void     mister_frame_done(void);
void     mister_mark(int stage);
float    mister_last_work(void);         /* how long the last picture took to make, ms (0: not known) */
void     mister_second_cpu(void);        /* the calling thread goes to the processor the game's main thread is not on */
extern long mister_stat[4];           /* the two threads' sharing, for the log */
extern long mister_pixels, mister_rows, mister_tris;   /* counted by the rasteriser, the main thread's */
extern long mister_helper_cnt[3];                       /* ... and the helper's: triangles, rows, pixels */
/* the picture (palette numbers, a byte a pixel) and its palette (256 of
   0x..RRGGBB) to the screen, at the next field; or (later) only the wait for
   the field here, and the rest by the thread that calls mister_show_job() --
   1 when it was left to that thread.  See mister.c. */
void     mister_show(void *window, const void *pixels, const uint32_t *pal, int w, int h);
int      mister_show_later(void *window, const void *pixels, const uint32_t *pal, int w, int h);
void     mister_show_job(void);
void     mister_show_wait(void);

/* which game, from the core's menu entry (the set name OpenScreamer_S1 /
   _S2) or, the core started by itself, the core's list of those whose files
   are there: 1 or 2, with its folder (relative to the game's, the current
   one) in dir; 0 when there is none, after the core's message saying what to
   copy where (it stays until the player leaves the core) */
int      mister_pick_game(char *dir, int size);
/* port.c: how many of the containers game `game` (1, 2) needs are not in
   `dir` (looked for, not read; -1: no DATA, GFX or ANIMS folder there), the
   first names in `names` (MISTER_NAMES + 1 characters), *more the others */
#define  MISTER_NAMES 110
int      os_game_missing(const char *dir, int game, char *names, int *more);
/* a message on the core's picture that stays until the player leaves the
   core, for files that cannot be used; `folder` the game's data folder as the
   program knows it, for the text */
void     mister_fatal(const char *text);
const char *mister_game_path(const char *folder);

/* what the process ends with: the launcher's contract */
int      mister_exit_code(void);

#endif
