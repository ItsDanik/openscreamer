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

int      mister_on(void);               /* the core is there */
/* the core's OSD option Resolution: 0 Lores 320x200, 1 Hires 640x400 */
int      mister_res_high(void);
/* whether the game's hi-res files are there: the OSD greys Hires out without */
void     mister_hires_there(int there);
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
   the OSD's Menu OK / Menu Back arrive as the two stick buttons */
void     mister_pad_mapping(void);
/* once a picture: with OS_FPSLOG=1, pictures a second and the time a picture
   took to make, every ten seconds */
void     mister_frame_done(void);
/* the picture (32 bits a pixel) to the screen, at the next field */
void     mister_show(void *window, const void *pixels, int w, int h);

/* what the process ends with: the launcher's contract */
int      mister_exit_code(void);

#endif
