/*
 * controls.h - what the player's hands are doing: the six controls of a race.
 *
 * The game (0x24d40) fills six words once a frame, each 0..0x3ff -- accelerate,
 * brake, left, right, gear up, gear down (0x1987c..0x19890) -- from six
 * 0x1c-byte records read out of CONFIG\CONTROLS.CFG (0x1b2f4).  A record is a
 * KEY (kind 1: the word is 0x3ff while the key at +0x18 is down) or a JOYSTICK
 * axis or button (kinds 0 and 2), and the driving code treats the two apart:
 * a key's steering and braking are eased in, a stick's are taken as they are.
 */
#ifndef SCREAMER_CONTROLS_H
#define SCREAMER_CONTROLS_H

#include <SDL.h>

typedef struct {
    float throttle;       /* 0..1 */
    float brake;          /* 0..1 */
    float steer;          /* -1 = full right .. +1 = full left */
    int   steer_analog;   /* the steering comes from a stick: no easing (0x163e0) */
    int   brake_analog;   /* the brake comes from a trigger: no easing (0x16187) */
    int   shift;          /* +1 / -1 on the step a gear control goes down */
} controls_state;

/* key[i]: a key's name for control i (accelerate, brake, left, right, gear up,
   gear down) from the port's settings, or "" / NULL = the game's own file */
void controls_init(const char *const key[6]);
void controls_read(controls_state *out);
/* SDL_PollEvent, with the game controller's buttons turned into the keys the
   program already knows (see controls.c) */
int  controls_poll(SDL_Event *ev);
/* 1 = the controller's buttons work the menus (arrows, ENTER, ESC);
   0 = they are the car's (only START, BACK and Y are turned into keys) */
void controls_menu_mode(int on);
/* the controller's heavy and light motor, 0..1 each; call it every picture */
void controls_rumble(float low, float high);
const char *controls_key_name(int i);

#endif
