/*
 * drive.c - the player's car, ported from STARTL.EXE.
 *
 * Every frame the game runs (0x2002f) these on the player record at 0xf918:
 *
 *   0x16f24  integrate: move the car, apply wall shove, wrap the angles
 *   0x15a70  -> 0x18a04 race clock, 0x18da0 ENGINE + GEARBOX, 0x16094 THROTTLE,
 *               0x16140 BRAKE, 0x1628c STEERING
 *   0x15b9c  -> 0x168a4 suspension bob, 0x17d04 SKID / SPIN, 0x1813c surface
 *               sound, 0x18518 crash states, 0x188a8 spin recovery, 0x19970 smoke
 *   0x15aa8  -> 0x1820c lap markers, 0x19490 lap count, 0x11e74/0x11f98/0x16b88
 *               body-corner collision, 0x11a30 wheel contact, 0x16c90 WALL HIT,
 *               0x16698 ride height from the four wheels
 *
 * The routines in capitals are ported below, line for line where the
 * decompiler output was complete (build/decomp/player_update.c keeps it).
 * The collision (which corner is off the track, which wall is nearest) is in
 * main.c, player_walls(), also from the game; so are the four-wheel ground
 * contact (player_contact), the suspension (suspension_step), the crash and
 * the smoke.
 *
 * THE DAMPING TABLE.  The game smooths values with
 *     v = (v - target) * damp[i] + target                     (0x22b60)
 * where damp[i] comes from a table built at start-up (0x1571c) for every
 * frame rate:
 *     damp[fps][i] = pow(0.5 + i * 0.005, 50 / (fps + 1))
 * so `i` is a stiffness from 0.5 (snappy) to 0.995 (lazy), defined at 50
 * frames a second.  The port steps the physics at exactly 50 Hz.
 */
#include "drive.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

#define FPS 50.0f

static float damp(int i)
{
    return powf(0.5f + (float)i * 0.005f, 50.0f / (FPS + 1.0f));
}

float drive_damp(int i) { return damp(i); }

static void approach(float *v, float target, int i)        /* 0x22b60 */
{
    *v = (*v - target) * damp(i) + target;
}

/* 0x220fc: the same for an angle -- near the target it eases through a sine */
static void approach_angle(float *v, float target, int i)
{
    if (fabsf(target - *v) < 1.5707963f)
        *v = sinf(*v - target) * damp(i) + target;
    else
        *v = (*v - target) * damp(i) + target;
}

static float sqrt0(float x) { return x < 0.0f ? 0.0f : sqrtf(x); }   /* 0x22bcc */

/* per-gear pull, the table at 0x1d84 in the data segment */
static const float GEAR_PULL[8] = { 0.0f, 5.0f, 3.5f, 2.0f, 0.95f, 0.78f, 0.72f, 0.0f };

/* --------------------------------------------------------------- CARS.DTR */

/* values carry a type letter: d decimal, f float, x hex, s string */
static double dtr_num(const char *tok)
{
    if (tok[0] == 'x') return (double)strtoul(tok + 1, NULL, 16);
    return atof(tok + 1);
}

int cars_dtr_load(car_spec *out, int max, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    int in = 0, n = 0, field = 0;
    while (fgets(line, sizeof line, f) && n < max) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!in) { if (!strncmp(p, "CARS_DATA", 9)) in = 1; continue; }
        if (!strncmp(p, "END_LIST", 8)) break;
        for (char *tok = strtok(p, " \t\r\n"); tok; tok = strtok(NULL, " \t\r\n")) {
            if (tok[0] == '*') break;                  /* comment to end of line */
            car_spec *c = &out[n];
            switch (field) {
            case 0:
                memset(c, 0, sizeof *c);
                snprintf(c->name, sizeof c->name, "%s", tok + 1);
                break;
            case 1: c->livery    = (int)dtr_num(tok); break;
            case 2: c->maxspd    = (float)dtr_num(tok); break;
            case 3: c->maxrpm    = (float)dtr_num(tok); break;
            case 4: c->grip      = (float)dtr_num(tok); break;
            case 5: c->accel     = (float)dtr_num(tok); break;
            case 6: c->automatic = (int)dtr_num(tok); break;
            case 7: c->nshift    = (int)dtr_num(tok); break;
            default: c->ratio[field - 7] = (float)dtr_num(tok); break;   /* 1..6 */
            }
            if (++field == 14) { field = 0; n++; }
        }
    }
    fclose(f);
    return n;
}

/* ------------------------------------------------------------------ set-up */

/* the tail of 0x136c8: turn the table row into the constants the frame uses */
void drive_init(car_state *c, const car_spec *spec, float wheelbase)
{
    memset(c, 0, sizeof *c);
    c->spec = *spec;
    c->wheelbase = wheelbase;
    c->surface = 1.0f;
    c->k_rpm          = spec->maxrpm / spec->maxspd * 0.02223f;
    c->k_engine_brake = spec->maxspd * 0.03f;
    c->k_brake        = spec->maxspd * 0.3f;
    c->k_wall         = spec->maxspd * 0.3f;
    c->grip_slide     = spec->grip * 16000.0f;
    c->grip_spin      = spec->grip * 26000.0f;
    c->grip_decel     = spec->grip * 16000.0f;
    c->grip_lock      = spec->grip * 11500.0f;
}

float drive_kmh(const car_state *c) { return c->speed * 340.0f / 21000.0f; }

/* ------------------------------------------------------------------ 0x18da0 */

static void engine(car_state *c, const drive_input *in)
{
    const car_spec *s = &c->spec;
    const float M = s->maxrpm, dt = DRIVE_DT;

    /* 0x18df0 -> 0x193e7: until the green light none of the gearbox or the
       drive is run -- the engine just follows the pedal, as it does in
       neutral.  So the player can rev on the grid, and an automatic car held
       over 8500 rpm at the green takes first with its wheels spinning. */
    if (c->waiting) {
        approach(&c->rpm, c->throttle * M, 0x4b);
        if (c->speed < dt * 500.0f) c->gear = 0;
        return;
    }

    if (!s->automatic) {
        if (in->shift > 0 && c->gear < s->nshift) c->gear++;
        if (in->shift < 0 && c->gear > 0)         c->gear--;
    } else {
        /* change up past 8500 rpm, down below 4000 (each nudged by the gear) */
        if (c->rpm > (float)(c->gear + 8500)) {
            if (c->gear < s->nshift) c->gear++;
        } else if (c->rpm < (float)(4000 - c->gear) && c->gear > 1) {
            c->gear--;
        }
        if (c->launch) c->gear = 1;
    }

    if (c->gear == 0) {
        /* neutral: the car coasts down and the engine follows the pedal */
        c->speed -= dt * 500.0f;
        if (c->speed < 0.0f) c->speed = 0.0f;
        approach(&c->rpm, c->throttle * M, 0x4b);
    } else {
        float ratio = s->ratio[c->gear];
        float top = s->maxspd;                       /* +0x510, the speed cap */

        /*
         * Normally the engine is locked to the wheels.  The exception is
         * pulling away: high revs and almost no speed with the pedal down --
         * the wheels spin (state 5), the revs fall and the car gains a flat
         * 1000 units/s each second until the two meet.
         */
        if (c->rpm <= s->grip * 6000.0f || c->speed >= 2000.0f) {
            c->rpm = ratio * c->speed * c->k_rpm;
            c->launch = 0;
        } else if (in->throttle > 200.0f / 1023.0f) {
            c->launch = 1;
            c->rpm -= dt * 500.0f;
        } else {
            c->rpm = ratio * c->speed * c->k_rpm;
            c->launch = 0;
        }

        /* how hard this gear pulls: strongest in the middle of its range */
        float vgear = M / (c->k_rpm * ratio);        /* speed at max rpm */
        float pull, curve;
        if (s->nshift == 1) {
            pull  = s->accel;
            curve = sqrt0(1.0f - c->speed / vgear);  /* argument not recovered:
                                                        only the one-gear "Raz
                                                        car" takes this path */
        } else {
            float a = (2.0f - (vgear - c->speed) / (vgear * 0.5f)) - 1.0f;
            if (c->gear == 1 && a < 0.0f) a = 0.2f;
            curve = (1.0f + sinf(a * 1.5707964f)) * 0.5f;
            pull  = s->accel * GEAR_PULL[c->gear];
        }
        if (c->rpm < c->throttle * M)
            c->rpm += c->throttle * pull * curve * dt;

        if (c->rpm <= M) {
            if (c->throttle * M < c->rpm) {          /* engine braking */
                c->rpm -= sqrt0(c->rpm / M) * c->k_engine_brake * dt;
                if (c->rpm < M * c->throttle) c->rpm = M * c->throttle;
            }
        } else {                                     /* over-revving */
            c->rpm -= (c->rpm - M) * 2.0f * dt;
            if (c->rpm < M * c->throttle) c->rpm = M * c->throttle;
        }

        float before = c->speed;
        c->speed = c->launch ? c->speed + dt * 1000.0f
                             : c->rpm / (c->k_rpm * ratio);
        if (c->speed > top) c->speed = top;
        /* slowing faster than the tyres can manage locks them */
        if (c->grip_decel * dt < before - c->speed) c->lock = 1.0f;
    }

    /* stopped: back to neutral.  (From there the automatic box revs up in
       neutral and takes first again past 8500 rpm, which is the launch.) */
    if (c->speed < dt * 500.0f) c->gear = 0;
}

/* ------------------------------------------------------- 0x16094 / 0x16140 */

static void pedals(car_state *c, const drive_input *in)
{
    /* no drive while sliding, locked or spinning */
    if (c->throttle >= 0.0f && c->slide_want == 0.0f && c->lock == 0.0f && !c->spin)
        c->throttle = in->throttle;
    else
        c->throttle = 0.0f;

    /* 0x16174: a joystick axis IS the brake (word / 1023); a key or a button
       brings it up to full */
    if (in->brake <= 0.0f)   c->brake = 0.0f;
    else if (c->analog & 4)  c->brake = in->brake;
    else                     approach(&c->brake, 1.0f, 0x52);

    if (fabsf(c->drift) < 0.1f) {
        c->speed -= c->k_brake * DRIVE_DT * c->brake;
        if (c->speed < 0.0f) c->speed = 0.0f;
        if (c->grip_lock < c->brake * c->speed && !c->spin) {
            /* braking that hard with a front wheel on the grass spins the
               car instead of locking the wheels (0x16259) */
            if (c->rough) c->spin = 1;
            else          c->lock = c->brake;
        }
    }
}

/* ------------------------------------------------------------------ 0x1628c */

static void steering(car_state *c, const drive_input *in)
{
    int stiff = c->launch ? 0x58
              : (c->lock == 0.0f && c->slide_want == 0.0f && !c->spin) ? 0x50 : 0x55;

    /* 18 degrees of lock at a standstill, falling to 1 degree at top speed */
    c->steer_max = (1.0f - sqrt0(c->speed / c->spec.maxspd)) * 0.296706f + 0.0174533f;

    float want = in->steer * c->steer_max;
    /* 0x163bb: a stick's wheel is where the stick is, unless the car is
       sliding or leaving the line -- then it is eased like the keys' */
    if ((c->analog & 2) && stiff == 0x50) c->steer = want;
    else approach(&c->steer, want, stiff);
    if (fabsf(c->steer) < 1e-5f) c->steer = 0.0f;
    /* 0x16458: on the grass the wheel is tugged about -- by up to 0.05 rad at a
       crawl, less the faster the car goes, one way or the other from one spot
       to the next -- and held to the lock */
    if (c->rough) {
        int spot = (int)c->x + (int)c->y;
        float tug = (float)(spot & 0x3f) * 0.0008f * ((spot & 1) ? -1.0f : 1.0f);
        c->steer += sqrt0(1.0f - c->speed / c->spec.maxspd) * tug;
        if (fabsf(c->steer) > c->steer_max) c->steer = c->steer < 0.0f ? -c->steer_max : c->steer_max;
    }

    if (c->steer != 0.0f) {
        /* the car turns about a point one wheelbase / tan(lock) to its side */
        c->radius = c->wheelbase / tanf(c->steer);
        c->turn   = DRIVE_DT * c->speed / c->radius;
        c->yaw   += c->turn;
    } else {
        c->radius = 0.0f;
        c->turn   = 0.0f;
    }
}

/* ------------------------------------------------------- 0x17d04 / 0x188a8 */

static void skid(car_state *c)
{
    const float dt = DRIVE_DT;

    if (!c->spin) {
        if (c->lock == 0.0f) {
            /* cornering force: speed squared times how fast the heading turns,
               and more for every wheel that is on the grass -- 1000 a front
               wheel, 600 a rear one (0x17e5c) */
            float f = c->speed * c->speed * c->turn * 0.0001f / dt * c->surface
                    + (float)((c->grass[0] + c->grass[2]) * 1000 + (c->grass[1] + c->grass[3]) * 600)
                      * (c->turn < 0.0f ? -1.0f : 1.0f);
            if (fabsf(f) <= c->grip_slide) {
                c->slide_want = 0.0f;
                c->slip = 0.0f;
            } else if ((c->brake + 0.5f) * f <= c->grip_spin) {
                /*
                 * Past the limit: the body swings out (slip), and the car is
                 * to run wide at  surface * 0.2 * speed^2 / radius.
                 *
                 * THE GAME IS LOP-SIDED HERE, and the port follows it.  The
                 * force f and the turning radius carry the SIGN of the turn.
                 * The spin test just above (0x17efa..0x17f0f) compares f
                 * itself, not its size, with the limit: with f negative it
                 * can never be passed.  And the running-wide speed below is
                 * negative for a negative radius, which the `< 10` test at
                 * the end of this routine (0x1809b) wipes out every frame.
                 * So only turning the way that makes the heading GROW does a
                 * car run wide or spin from cornering alone; turning the
                 * other way the body swings out and that is all.  Until
                 * 2026-10-11 the port made both directions alike ("always
                 * outward"), which the game's code does not do.
                 */
                f += (f <= 0.0f) ? c->grip_slide : -c->grip_slide;
                c->slip = f * 0.00015f;
                if (fabsf(c->slip) > 1.53938f)
                    c->slip = (f < 0.0f ? -1.0f : 1.0f) * 1.53938f;
                c->slide_want = c->radius != 0.0f
                              ? c->surface * 0.2f * (c->speed * c->speed / c->radius) : 0.0f;
                c->slide_dx = sinf(c->yaw - 1.5707964f);
                c->slide_dy = -cosf(c->yaw - 1.5707964f);
            } else {
                c->spin = 1;                     /* too much, especially braking */
            }
        } else {
            /* wheels locked: slide straight on until the speed comes down */
            c->lock *= damp(98);
            if (c->lock * c->speed < 2000.0f) c->lock = 0.0f;
            approach(&c->slip, 0.0f, 7);
            c->yaw += c->slip * 0.2f;
        }
        approach_angle(&c->drift, sqrt0(fabsf(cosf(c->roll)) * 2.0f) * c->slip, 0x5a);
    }
    approach(&c->slide, c->slide_want, 0x50);
    if (c->slide < 10.0f) c->slide = 0.0f;

    /* 0x188a8: a spin throws the tail round once and then settles */
    if (c->spin == 1) {
        float dir = (c->turn < 0.0f) ? 1.0f : -1.0f;
        c->spin = (int)lroundf(dir + 3.0f);              /* 2 or 4 */
        c->drift += dir * 6.2831855f;
    } else if (c->spin > 1) {
        float dir = (float)(c->spin - 3);
        float rate = (fabsf(c->drift) + 0.2f) / 3.1415927f * 3.0f * dt;
        int done = 0;
        if (dir > 0.0f) { c->drift -= rate; if (c->drift < 0.0f) done = 1; }
        if (dir < 0.0f) { c->drift += rate; if (c->drift > 0.0f) done = 1; }
        if (done) { c->drift = 0.0f; c->spin = 0; }
    }
}

/* ------------------------------------------------------------------ 0x16f24 */

static void integrate(car_state *c)
{
    const float dt = DRIVE_DT;
    float cp = cosf(c->pitch);

    /* forward */
    c->x += sinf(c->yaw) * cp * c->speed * dt;
    c->y -= cosf(c->yaw) * cp * c->speed * dt;

    /* shoved off a wall */
    c->x += c->shove_x * c->shove * dt;
    c->y += c->shove_y * c->shove * dt;
    c->yaw_rate *= damp(80);
    c->yaw += c->yaw_rate * dt;
    c->shove *= damp(91);
    if (fabsf(c->shove) < 250.0f) c->shove = 0.0f;

    /*
     * Running wide in a skid (0x172d8): the car is moved along (sin, -cos) of
     * heading - 90 degrees by the slide speed.  The game sets that direction
     * when the slide starts (0x17f4d) and keeps it; the slide speed is never
     * negative (see the skid routine), so there is one direction only.
     */
    float k = sqrt0(fabsf(cosf(c->roll)));
    c->x += k * dt * c->slide * c->slide_dx;
    c->y += k * dt * c->slide * c->slide_dy;

    while (c->yaw < 0.0f)        c->yaw += 6.2831855f;
    while (c->yaw > 6.2831855f)  c->yaw -= 6.2831855f;
}

/* 0x16c90: glancing a wall scrubs speed by how squarely it was hit.
   `side` is (front-right corner out) - (front-left corner out): the game turns
   the nose with exactly that, so a hit with the tail alone does not steer. */
void drive_hit_wall(car_state *c, float push_x, float push_y, float side,
                    float wall_angle)
{
    float len = sqrtf(push_x * push_x + push_y * push_y);
    if (len < 1e-3f) return;
    /* how squarely the wall was hit: |sin(heading + wall angle)| (0x16cec) */
    float square = fabsf(sinf(c->yaw + wall_angle));
    c->shove_x = push_x / len;
    c->shove_y = push_y / len;
    /* 0x16df4: square^4 * speed * 2 + 2000, at most 5000 -- so a glancing
       scrape gets little more than the flat 2000, and only a square hit the
       full shove */
    c->shove = square * square * square * square * c->speed * 2.0f + 2000.0f;
    if (c->shove > 5000.0f) c->shove = 5000.0f;
    c->speed -= c->speed * 0.9f * square;
    if (c->speed < 0.0f) c->speed = 0.0f;
    c->yaw_rate = side * 5.0f * square;        /* the nose is turned along the wall */
    c->lock = 0.0f;
    c->slip = 0.0f;
    c->slide_want = 0.0f;
    if (!c->spin) c->drift = 0.0f;
}

void drive_step(car_state *c, const drive_input *in)
{
    integrate(c);
    engine(c, in);
    pedals(c, in);
    steering(c, in);
    skid(c);
}
