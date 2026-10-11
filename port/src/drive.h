/*
 * drive.h - the player's car: engine, gearbox, brakes, steering, skidding.
 *
 * PORTED FROM THE GAME, not invented.  The original updates the player's car
 * once a frame with about 25 small routines (the list is in drive.c); this file
 * carries the ones that decide how the car MOVES, with the game's own
 * constants.  Field comments give the offset inside the game's player record
 * (which lives at 0xf918 in the data segment), so the two can be compared.
 *
 * Units are the game's: distance in world units, speed in units per second,
 * angles in radians.  The speedometer is  km/h = speed * 340 / 21000  (the
 * car table's own comment gives 340 km/h for a max speed of 21000).
 */
#ifndef SCREAMER_DRIVE_H
#define SCREAMER_DRIVE_H

/* one row of CARS_DATA in GEN.DAT's CARS.DTR -- the game copies it verbatim
   into the player record at +0x470 */
typedef struct {
    char  name[12];       /* "AUTO00" */
    int   livery;         /* 0 or 0x8000: the second paint job */
    float maxspd;         /* +0x480 */
    float maxrpm;         /* +0x484 */
    float grip;           /* +0x488 */
    float accel;          /* +0x48c */
    int   automatic;      /* +0x490  1 = the game shifts for you */
    int   nshift;         /* +0x494  number of gears */
    float ratio[7];       /* +0x498.. gear 1..nshift (index 0 unused) */
} car_spec;

typedef struct {
    float throttle;       /* 0..1 */
    float brake;          /* 0 or 1 (a key) */
    float steer;          /* -1 = full right .. +1 = full left */
    int   shift;          /* +1 / -1 on the frame a gear key goes down */
} drive_input;

typedef struct {
    car_spec spec;

    /* where it is: x, y on the ground, z up (the port's axes) */
    float x, y, z;
    float yaw;            /* +0x4cc  heading; forward = (sin yaw, -cos yaw) */
    float pitch, roll;    /* +0x4c8, +0x4d0 -- set by the caller from the ground */

    float speed;          /* +0x50c */
    float rpm;            /* +0x7a8 */
    int   gear;           /* +0x7ac  0 = neutral */
    int   launch;         /* state 5 at +0x148: wheels spinning off the line */
    int   waiting;        /* the lights are not green yet (flag 1 at 0x18e18 clear): set by the caller */

    /* how this step's controls are made (controls.h): bit 1 = the steering is a
       stick's, bit 2 = the brake is a trigger's.  The caller sets it before
       drive_step(); it is not in drive_input because that is what replay
       files are made of. */
    int   analog;
    float throttle;       /* +0x144 */
    float brake;          /* +0x114 */
    float steer;          /* +0x108  front-wheel angle */
    float steer_max;      /* +0x10c */
    float turn;           /* +0x2f4  heading change this step */
    float radius;         /* +0x2f8  turning radius, 0 = straight */

    float lock;           /* +0x110  wheels locked / sliding straight on */
    float slip;           /* +0x4f0  how far past the grip limit */
    float slide_want;     /* +0x4f4 */
    float slide;          /* +0x4fc  sideways speed while skidding */
    float drift;          /* +0x128  angle between the body and its heading */
    int   spin;           /* +0x170  1 = lost it */

    float yaw_rate;       /* +0x548  knocked sideways by a wall */
    float shove, shove_x, shove_y;   /* +0x54c, +0x550, +0x558 */

    float wheelbase;      /* +0x2ec  distance between the axles */
    float surface;        /* +0x670  grip of the surface under the car, 1 = tarmac */
    int   rough;          /* +0x2f0  a front wheel is on rough ground (grass) */
    float slide_dx, slide_dy;   /* +0x500, +0x508  which way a slide carries the car */
    int   grass[4];       /* wheel +0x2c  this wheel is on grass (front-left, rear-left, front-right, rear-right) */

    /* derived once from the spec, as the game does when the race loads */
    float k_rpm;          /* +0x174 */
    float k_engine_brake; /* +0x178 */
    float k_brake;        /* +0x17c */
    float k_wall;         /* +0x180 */
    float grip_slide;     /* +0x184 */
    float grip_spin;      /* +0x188 */
    float grip_decel;     /* +0x18c */
    float grip_lock;      /* +0x190 */
} car_state;

/* the physics runs at a fixed 50 steps a second, the game's own reference rate */
#define DRIVE_DT 0.02f

int   cars_dtr_load(car_spec *out, int max, const char *path);
void  drive_init(car_state *c, const car_spec *spec, float wheelbase);
void  drive_step(car_state *c, const drive_input *in);
/* the car ran into something whose face lies `angle` radians off its heading */
void  drive_hit_wall(car_state *c, float push_x, float push_y, float side,
                     float wall_angle);
float drive_kmh(const car_state *c);
/* the game's per-frame smoothing factor for stiffness i (see drive.c) */
float drive_damp(int i);

#endif
