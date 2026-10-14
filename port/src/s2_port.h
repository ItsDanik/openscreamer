/*
 * SCREAMER 2: between the game's routines (s2_gen.c, s2_hooks.c) and the rest of the port.
 */
#ifndef OS_S2_PORT_H
#define OS_S2_PORT_H

#include <stdint.h>

/* ---- the routines' side (s2_hooks.c) */
int  s2_reset(void);                         /* an empty block */
void s2_gen_strings(void);                   /* (made by tools/s2c.py) */
int  s2_dtr(const char *file, const char *section, uint32_t dest, int stride, int row);

/* a race: the track 1-7, its condition 0-2, the cars' teams 0-3 and models 0-4; car 0 is
   the player's.  0 if the game's files will not have it. */
extern int s2_level;                         /* 0 pro, 1 normal, 2 rookie; before s2_race_start */
int  s2_race_start(int track, int condition, int ncars, const int *team, const int *model);
void s2_sound_start(int track);            /* the samples of a race read and the mixer started: before s2_race_start */
void s2_controls(int accelerate, int brake, int left, int right, int gear_up, int gear_down);
void s2_go(void);                            /* the start: before it the cars are held */
void s2_frame(float step);
void s2_frame_times(float raw);              /* a test: the next frames' unsmoothed time (0x133354) */
void s2_clock_set(float clock);              /* a test: the race's clock */                   /* one frame of the game's race routine, `step` seconds */
typedef struct {
    int   x, height, y;                      /* car +0x4d8.. : the game's world */
    float angle[3];                          /* car +0x4e4, +0x4e8 (the heading), +0x4ec */
    float speed, steer, rpm;
    int   gear;
    /* what 0x16878 hands the engine for the picture: the front wheels' turn (+0x13c, eased),
       the body's own nose-up and lean on its springs (+0xc18, +0xc20) and lift (+0x4f4),
       each wheel's turning (wheel +0x10) and how far its spring is pushed in (wheel +0x48) */
    float steer_shown, body_pitch, body_roll, wheel_spin[4];
    int   body_lift, wheel_in[4];
    /* which blocks of the car's model those are (0x2f519 finds them by name, 0x4bc03): the
       body's four (fauto, rauto, a third, MV_mauto: car +0x1f8 +0x200 +0x208 +0xbf4) and each
       wheel's two -- the first turns with the wheel, both steer (wheel +0x50, +0x58) */
    int   body_block[4], wheel_block[4][2];
    /* for the shadow (0x1f4a8): the ground's height under the car (+0x4f8) and its slope
       along and across the car (+0xc0c, +0xc14) */
    int   ground; float ground_pitch, ground_roll;
    int   wheel_down[4], wheel_ground[4], wheel_kind[4];   /* wheel +0x08 on the ground, +0x44 its height, +0x5C the road kind */
} s2_car_state;
void s2_car(int i, s2_car_state *c);
/* the player's view: 2, 6 from the car; 4 close behind; 3 far behind (the one a race starts in) */
void s2_view(int view);                      /* (not taken once the game has locked the view: the race over) */
typedef struct {
    int   x, height, y;                      /* the camera's place, camera +0x48 */
    float angle[3];                          /* +0x54 down / up, +0x58 the heading (as a car's), +0x5c the lean */
    int   view;
    int   shot;                              /* view 1: which trackside camera (+0x24) -- a change is a cut */
    int   rows;                              /* +0x1c: the rows the scene is drawn in (200; view 6: the dashboard's first number / 2, 0x13cbd) */
    int   fade_near, fade_far;               /* the night's / fog's distances for this view (0x11373c, 0x113738, made positive) */
} s2_camera_state;
void s2_camera(s2_camera_state *c);
extern int s2_laps;                          /* how many laps a race is (0x130d48); before s2_race_start */
/* what the game's race screen (0x45882, 0x46b76, 0x456fb) shows of the player's car */
typedef struct {
    int   ncars, position, lap, laps, gear, finished, wrong_way, human_view;
    float clock;                             /* the race's, 0x13335c: the cars are let go at 3 */
    float this_lap, total;                   /* car +0xc3c, +0xc38 */
    const char *best_lap, *best_total;       /* 0x132fa4, 0x132fb0: as the game writes them */
    const char *message; float *message_left;        /* 0x132f98 ("+00:01:23" at a checkpoint) while 0x133078 > 0 */
    float *best_lap_left;                    /* 0x13307c: BEST LAP! blinks while it is > 0 */
    float speed, revs;                       /* car +0x500, car +0xc78 / car +0x60 */
    int   view;                              /* the camera's view (2 6 4 3 from the car, 1 trackside) */
    int   leader;                            /* the car that is first, if it is not the player's (and the mode marks it: 0x4962c), or -1 */
    int   bend;                              /* the bend's arrow to show now (a sprite), or -1 (0x454fc) */
    int   dash; const void *dash_rec;        /* the player's dashboard (0x124574) and its record of CRUSC.DTR's table, 0x58 bytes */
    int   team[4]; float ahead[4];           /* every car: its team, and how far the player is ahead of it / 200000, -1 .. 1 */
} s2_hud_state;
void s2_hud(s2_hud_state *h);
void s2_put(int i, int x, int y, float heading);
void s2_throw(int i, int height, float speed, float step);
void s2_dump(const char *path);
void s2_robot(int on);
typedef struct { int kind, car, x, height, y, phase, carried; } s2_puff;     /* (carried: x, height, y are from the car's place) */
int  s2_puffs(s2_puff *out, int max);
typedef struct { int id, kind; float x, height, y, angle[3]; } s2_spark;          /* (id: which of the game's sixty records) */
int  s2_sparks(s2_spark *out, int max);
typedef struct { int id; const char *name; float x, height, y, angle[3]; int flying; } s2_obstacle;
int  s2_obstacles(s2_obstacle *out, int max);
int  s2_laps_done(int i);
int  s2_touched(void);
void s2_state(void);
void s2_peek(int i, uint32_t from, int n);

/* ---- the port's side (main.c): the track */
/* what is under (x, y): 1 and its height, the surface's normal (x, up, y; any length) and
   the piece's road kind (its record's last word), or 0 */
int  s2p_ground(int only_whether, int x, int y, int *height, int normal[3], int *kind);   /* (only_whether: 0x28ce6's argument 1 -- is there ground at all; nothing else is given) */
/* the road-centre point (walls = 0: angle, x, y) or the wall segment (1: angle, middle x, y,
   and its two ends) nearest (x, y), as the track's file has it; 0 if there is none near */
int  s2p_nearest(int walls, int x, int y, float rec[7]);

#endif
