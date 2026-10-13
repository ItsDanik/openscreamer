/*
 * SCREAMER 2: between the game's routines (s2_gen.c, s2_hooks.c) and the rest of the port.
 */
#ifndef OS_S2_PORT_H
#define OS_S2_PORT_H

#include <stdint.h>

/* ---- the routines' side (s2_hooks.c) */
void s2_reset(void);                         /* an empty block */
void s2_gen_strings(void);                   /* (made by tools/s2c.py) */
int  s2_dtr(const char *file, const char *section, uint32_t dest, int stride, int row);

/* a race: the track 1-7, its condition 0-2, the cars' teams 0-3 and models 0-4; car 0 is
   the player's.  0 if the game's files will not have it. */
int  s2_race_start(int track, int condition, int ncars, const int *team, const int *model);
void s2_controls(int accelerate, int brake, int left, int right, int gear_up, int gear_down);
void s2_go(void);                            /* the start: before it the cars are held */
void s2_frame(float step);                   /* one frame of the game's race routine, `step` seconds */
typedef struct {
    int   x, height, y;                      /* car +0x4d8.. : the game's world */
    float angle[3];                          /* car +0x4e4, +0x4e8 (the heading), +0x4ec */
    float speed, steer, rpm;
    int   gear;
    int   wheel_down[4], wheel_ground[4], wheel_kind[4];   /* wheel +0x08 on the ground, +0x44 its height, +0x5C the road kind */
} s2_car_state;
void s2_car(int i, s2_car_state *c);
void s2_peek(int i, uint32_t from, int n);

/* ---- the port's side (main.c): the track */
/* what is under (x, y): 1 and its height, the surface's normal (x, up, y; any length) and
   the piece's road kind (its record's last word), or 0 */
int  s2p_ground(int x, int y, int *height, int normal[3], int *kind);
/* the road-centre point (walls = 0: angle, x, y) or the wall segment (1: angle, middle x, y,
   and its two ends) nearest (x, y), as the track's file has it; 0 if there is none near */
int  s2p_nearest(int walls, int x, int y, float rec[7]);

#endif
