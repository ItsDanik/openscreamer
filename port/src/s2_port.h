/*
 * SCREAMER 2: between the game's routines (s2r_car.h, s2_hooks.c) and the rest of the port.
 */
#ifndef OS_S2_PORT_H
#define OS_S2_PORT_H

#include <stdint.h>

/* ---- the routines' side (s2_hooks.c) */
int  s2_reset(void);                         /* an empty block */
int  s2_available(void);                     /* the game's routines are in this build */
void s2_gen_strings(void);                   /* (the reference build's names, made by tools/s2c.py; else nothing) */
int  s2_dtr(const char *file, const char *section, uint32_t dest, int stride, int row);

/* a race: the track 1-7, its condition 0-2, the cars' teams 0-3 and models 0-4; car 0 is
   the player's.  0 if the game's files will not have it. */
extern int s2_level;                         /* 0 pro, 1 normal, 2 rookie; before s2_race_start */
int  s2_race_start(int track, int condition, int ncars, const int *team, const int *model);
void s2_sound_start(int track);            /* the samples of a race read and the mixer started: before s2_race_start */
void s2_controls(int accelerate, int brake, int left, int right, int gear_up, int gear_down);
/* the player's controls as controls.c has them: accelerate and brake 0..1, the steering -1 full right
   .. 1 full left (a stick's through the game's own curve when a controller is there) */
void s2_controls_pad(float throttle, float brake, float steer, int gear_up, int gear_down);
/* the MiSTer core's Cheats page: its triggers selected since the last asking (bit 0 = the first),
   0 elsewhere (main.c) */
int  s2_osd_triggers(void);
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
    int   shown;                             /* the car is in the picture (0x2c132 takes time attack's ghost out) */
    /* the car object's flags (its record +0x20, 0x16878): 0x200 braking, 0x400 / 0x8000 the
       wheels turning fast, 0x800 0x1000 0x2000 0x4000 a side knocked hard (0x19aaa, car +0xb8);
       the engine draws each texture's picture number flags >> 9 of its 128 (frames x phase >> 14,
       phase = flags >> 2) -- the brake lights, the wheels' blur, the dents */
    int   look;
    int   env;      /* which picture of the reflections' map (0x1f1ef, car +0xc80 -> the object's +0x3c) */
} s2_car_state;
void s2_car(int i, s2_car_state *c);
float s2_env_rate(void);
void  s2_replay_from_file(void);   /* a test: the kept replay read back from its file */    /* how fast the reflections slide, texture steps a unit moved (0xb04a8) */
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
void s2_mirror_set(int on);               /* the mirror on or off (0x11f0c4) */
int  s2_mirror(s2_camera_state *c);        /* the mirror's camera after the last frame, or 0 when there is none to draw */
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
    float lap_best;                          /* the car's best lap of this race (+0xc40) */
    unsigned mode;                           /* the kind of game (0x130d10) */
    int   ghost;                             /* time attack: the ghost is shown (0x11f0a4) */
    const char *split_now, *split_best;      /* a free run: five texts of 11 bytes each -- this lap's times at A..E, and against the best lap's */
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
typedef struct { int id; const char *name; float x, height, y, heading, climb; int faces_screen; } s2_anim;
int  s2_anims(s2_anim *out, int max);     /* the track's animations that are to be seen now (birds ...) */
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

/* the kind of game (the mode word): 1 arcade, 2 championship, 4 time attack, 8 free run */
extern int s2_mode, s2_ghost_on;
void s2_race_music(void);              /* the race's CD song, as its set-up chose it */
void s2_attract_step(void);            /* a frame of the attract race (mode 0x80) */
int  s2_player_done(void);             /* the player's car has done its laps */
void s2_sound_pause(float dt, int on); /* the effects under the race's pause screen */
int  s2_volume(int which);             /* the race record's CD (0) / effects (1) volume, 0..0x7fff */
void s2_volume_set(int which, int v);
int  s2_attack_plan(int track, int *team, int *model);   /* before the cars are loaded: time attack's ghost, its car into team[1], model[1] */
void s2_race_leave(void);                                /* as the race is left: the ghost kept, if it is to be */
int  s2_replay_offered(void);                             /* the end's screen has REPLAY (an arcade or league race, written down) */
void s2_replay_take(int on);                             /* REPLAY taken (1): the next start of this race is its replay */
typedef struct { float knock, slip, speed; int x, y, down, grass, finished; } s2_feel;
void s2_feel_read(s2_feel *f);                           /* the player's car, for the controller's rumble (main.c) */
int  s2_replaying(void);                                 /* this race is a replay (the mode's 0x100) */
void s2_replay_stop(void);                               /* ESC in a replay: its end */
void s2_replay_next_car(void);                           /* F11 in a replay: the cameras follow the next car */
void s2_replay_keep(void);                               /* the race's frames kept for REPLAY (done as the end's screen comes up) */
int  s2_replay_band(int *team, int *view);               /* a replay's band under the picture: shown, the followed car's team, the view */
/* the championship: a league's seven numbers (races, tracks opened, fifth car, league opened, the others' level, two for the
   weather), the order of the tracks, a race's weather and the seed it leaves, the cars by their places */
extern const int s2_league[4][7], s2_track_order[7];
extern unsigned s2_race_seed;
int  s2_champ_weather(int league, int race, int *left, unsigned number);
typedef struct { int car, team, model, done; float time; } s2_result;
int  s2_results(s2_result *r);
extern char s2_name[4];                                  /* the player's three letters */
int  s2_records_enter(void);                             /* the end's screen comes up: the race into the best times; the player's row or -1 */
const void *s2_records_list(void);                       /* the list to show: 15 float times, +0x3c 15 names of 4 bytes, +0xb4 15 teams, +0xf0 15 models */
void s2_splits_forget(void);                             /* the menu was gone back to: a free run has no best lap any more */
int  s2_player(void);                                    /* which car is the player's */

/* SCREAMER 2'S OWN MENU: the game's routines (s2_hooks.c) and the port's screen (main.c) */
typedef struct { char cont[16], name[16], lib[32]; int x, y, z, a[3], frame; } s2m_item;   /* an object of the engine's list (lib: its materials) */
typedef struct { int x, y, z, a[3]; } s2m_cam;                                      /* its camera record */
int  s2_menu_res_change(void);                           /* the options page asked for the other resolution: 1 low, 2 high */
int  s2_menu_run(void);                                  /* the menu's loop (0x38540): 0 when the program is to end */
void s2m_base(const uint8_t *six);
void s2m_palette(const uint8_t *six);
int  s2_screen_hi(void);                                  /* Screamer 2 at 640 x 400: STARTH's constants (docs/NOTES2.md S2-9) */
void s2m_fonts(void);
void s2m_picture(int x, int y, int w, int h, const uint8_t *px);
int  s2m_text(int x, int y, const char *str, int font, int colour, int space);
int  s2m_text_wide(const char *str, int font, int space);
void s2m_sprite(int x, int y, int n);
void s2m_dark(int x, int y, int w, int h);
void s2m_clear(uint8_t idx);
void s2m_materials(const char *cont, const char *name, int flags);
void s2m_clip(int text, int x0, int y0, int x1, int y1);
void s2m_blend(const int smoke[3], const int dust[3]);       /* 0x504f2 */
int  controls_pad(int sw[6]);                                /* controls.c: the game controller as the six controls */    /* 0x50d6c (sprites) / 0x50fb9 (text) */
void s2m_materials_go(const char *cont, const char *name);   /* 0x50165: a library let go */
void s2m_materials_all_go(void);                             /* 0x50247: all of them */
void s2m_draw(const s2m_item *it, int n, const s2m_cam *cm, int room);
void s2m_scene_load(const char *cont, const char *name);
void s2m_scene_free(void);
int  s2m_show(void);
extern int g_s2m_frames;
void s2m_race_go(int track);
void s2_race_results_keep(void);                         /* a race from the game's menu: what it leaves for the menu */
const uint8_t *s2m_keys(void);
float s2m_elapsed(void);
int  s2m_hundredths(void);

#endif
