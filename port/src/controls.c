/*
 * controls.c - the six controls of a race, from the keyboard and a game controller.
 *
 * KEYS: the game's own CONFIG\CONTROLS.CFG (0x1b2f4 reads it: six records of
 * 0x1c bytes, then 0x28 joystick-calibration records, then ten ints -- the
 * joystick's port numbers).  A record, seven ints:
 *     +0x00  1 = in use
 *     +0x04  1 = a joystick AXIS (else a button)
 *     +0x08  kind: 1 key, 0 joystick (as it reads), 2 joystick used as a switch
 *     +0x0c  axis number        +0x10  stick number
 *     +0x14  the side that counts: +1 or -1 (a control fires only when the
 *            word's sign is this -- how one axis serves two controls)
 *     +0x18  the key's keyboard scan code / the button's bit
 * The file as shipped: Up, Down, Left, Right, Alt, Ctrl.  The scan codes are
 * the PC keyboard's, where the arrows and the number pad's 8 / 2 / 4 / 6 are
 * the same four codes and so are the two Ctrls and the two Alts: both of each
 * pair work here.
 *
 * GAME CONTROLLER: the port's own.  The file's joystick records are the
 * calibration of a DOS game-port stick (centre, counts per step) and mean
 * nothing to SDL, so the layout is fixed: left stick = the steering axis,
 * triggers = accelerate / brake axes, with the game's own treatment of an
 * axis (0x24e1c..): scaled to +-0x3ff, nothing under 0x66.  Buttons A / B and
 * the d-pad are switches, as keys are.
 */
#include <stdio.h>
#include <string.h>
#include "controls.h"
#ifdef MISTER_HYBRID
#include "mister.h"
#endif
#include "port.h"

static SDL_Scancode g_key[6][2];           /* each control's key, and its twin (the other Ctrl, the number pad) */
static int g_ready, g_menu = 1;
static SDL_GameController *g_pad;
static int g_steer_stick;                  /* the steering last came from the stick */
static int g_gear_was[2];
static int g_stick_was[2];                 /* the stick as menu arrows: -1 / 0 / +1, across and up-down */

/* PC keyboard scan code -> SDL's, with the twin of the keys the PC cannot tell apart */
static void scan_to_sdl(int scan, SDL_Scancode out[2])
{
    static const SDL_Scancode row[0x59] = {
        [0x01] = SDL_SCANCODE_ESCAPE, [0x02] = SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
        SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0,
        SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB,
        [0x10] = SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y,
        SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET,
        SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_RETURN, SDL_SCANCODE_LCTRL,
        [0x1e] = SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H,
        SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE,
        SDL_SCANCODE_GRAVE, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_BACKSLASH,
        [0x2c] = SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N,
        SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_RSHIFT,
        SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE, SDL_SCANCODE_CAPSLOCK,
        [0x3b] = SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
        SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10,
        [0x47] = SDL_SCANCODE_HOME, SDL_SCANCODE_UP, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_KP_MINUS,
        SDL_SCANCODE_LEFT, SDL_SCANCODE_KP_5, SDL_SCANCODE_RIGHT, SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_END,
        SDL_SCANCODE_DOWN, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_INSERT, SDL_SCANCODE_DELETE,
        [0x57] = SDL_SCANCODE_F11, SDL_SCANCODE_F12,
    };
    static const struct { int scan; SDL_Scancode twin; } twins[] = {
        { 0x1d, SDL_SCANCODE_RCTRL }, { 0x38, SDL_SCANCODE_RALT }, { 0x1c, SDL_SCANCODE_KP_ENTER },
        { 0x47, SDL_SCANCODE_KP_7 }, { 0x48, SDL_SCANCODE_KP_8 }, { 0x49, SDL_SCANCODE_KP_9 },
        { 0x4b, SDL_SCANCODE_KP_4 }, { 0x4d, SDL_SCANCODE_KP_6 }, { 0x4f, SDL_SCANCODE_KP_1 },
        { 0x50, SDL_SCANCODE_KP_2 }, { 0x51, SDL_SCANCODE_KP_3 }, { 0x52, SDL_SCANCODE_KP_0 },
        { 0x53, SDL_SCANCODE_KP_PERIOD },
    };
    out[0] = (scan > 0 && scan < 0x59) ? row[scan] : SDL_SCANCODE_UNKNOWN;
    out[1] = SDL_SCANCODE_UNKNOWN;
    for (unsigned i = 0; i < sizeof twins / sizeof twins[0]; i++)
        if (twins[i].scan == scan) out[1] = twins[i].twin;
}

void controls_init(const char *const key[6])
{
    /* DEFCTRL.CFG's six, for when there is no file */
    static const int def[6] = { 0x48, 0x50, 0x4b, 0x4d, 0x38, 0x1d };
    int rec[6][7];
    int have = 0;
    FILE *f = os_config_open("CONTROLS.CFG");
    if (!f) f = os_config_open("DEFCTRL.CFG");
    if (f) { have = fread(rec, sizeof rec, 1, f) == 1; fclose(f); }
    for (int i = 0; i < 6; i++) {
        scan_to_sdl(def[i], g_key[i]);
        /* a record that is a key, in use, counting on its positive side */
        if (have && rec[i][0] == 1 && rec[i][2] == 1 && rec[i][5] == 1) {
            SDL_Scancode k[2];
            scan_to_sdl(rec[i][6], k);
            if (k[0] != SDL_SCANCODE_UNKNOWN) { g_key[i][0] = k[0]; g_key[i][1] = k[1]; }
        }
        if (key && key[i] && key[i][0]) {
            SDL_Scancode k = SDL_GetScancodeFromName(key[i]);
            if (k != SDL_SCANCODE_UNKNOWN) { g_key[i][0] = k; g_key[i][1] = SDL_SCANCODE_UNKNOWN; }
            else fprintf(stderr, "openscreamer.cfg: no key is called \"%s\"\n", key[i]);
        }
    }
    g_ready = 1;
}

const char *controls_key_name(int i)
{
    return SDL_GetScancodeName(g_key[i][0]);
}

static void pad_find(void)
{
#ifdef MISTER_HYBRID
    mister_pad_mapping();                               /* MISTER: the core's joystick as a game controller */
#endif
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER) && SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return;
    for (int i = 0; !g_pad && i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) g_pad = SDL_GameControllerOpen(i);
}

void controls_menu_mode(int on) { g_menu = on; }

/* the game controller's two motors, 0..1 each (the heavy one, the light one).
   Called every picture: the controller is only told when the strength has
   changed, or to keep it going. */
void controls_rumble(float low, float high)
{
    static int was_low = -1, was_high = -1;
    static Uint32 told;
    int l = low <= 0.0f ? 0 : low >= 1.0f ? 65535 : (int)(low * 65535.0f);
    int h = high <= 0.0f ? 0 : high >= 1.0f ? 65535 : (int)(high * 65535.0f);
    l &= ~0x7ff; h &= ~0x7ff;                    /* (32 steps: no need to tell it of less) */
    if (!g_pad) { was_low = was_high = -1; return; }
    Uint32 now = SDL_GetTicks();
    if (l == was_low && h == was_high && (!(l | h) || now - told < 300)) return;
    SDL_GameControllerRumble(g_pad, (Uint16)l, (Uint16)h, 1000);
    was_low = l; was_high = h; told = now;
}

/* an axis as the game makes it: +-0x3ff, and nothing under 0x66 (0x24ed4) */
static int axis_word(int raw)
{
    int v = raw * 0x3ff / 32767;
    if (v > 0x3ff) v = 0x3ff;
    if (v < -0x3ff) v = -0x3ff;
    return (v < 0x66 && v > -0x66) ? 0 : v;
}

static int key_down(const Uint8 *keys, int i)
{
    return (g_key[i][0] != SDL_SCANCODE_UNKNOWN && keys[g_key[i][0]])
        || (g_key[i][1] != SDL_SCANCODE_UNKNOWN && keys[g_key[i][1]]);
}

void controls_read(controls_state *out)
{
    if (!g_ready) controls_init(NULL);
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    int sw[6], stick = 0, gas = 0, stop = 0;
    for (int i = 0; i < 6; i++) sw[i] = key_down(keys, i);
    /* A and Z as well, when they are nobody's: Alt is a poor gear lever under a window manager */
    int a_free = 1, z_free = 1;
    for (int i = 0; i < 6; i++) {
        if (g_key[i][0] == SDL_SCANCODE_A) a_free = 0;
        if (g_key[i][0] == SDL_SCANCODE_Z) z_free = 0;
    }
    if (a_free && keys[SDL_SCANCODE_A]) sw[4] = 1;
    if (z_free && keys[SDL_SCANCODE_Z]) sw[5] = 1;
    if (g_pad) {
        sw[0] |= SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_A);
        sw[1] |= SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_B);
        sw[2] |= SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        sw[3] |= SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        sw[4] |= SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        sw[5] |= SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        stick = axis_word(SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX));
        gas   = axis_word(SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
        stop  = axis_word(SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    }
    /* OS_PADTEST="stick,right trigger,left trigger" (SDL's raw numbers, +-32767):
       a controller that is not there, held so -- I have no hands to hold one */
    if (getenv("OS_PADTEST")) {
        int v[3] = { 0, 0, 0 };
        sscanf(getenv("OS_PADTEST"), "%d,%d,%d", &v[0], &v[1], &v[2]);
        stick = axis_word(v[0]); gas = axis_word(v[1]); stop = axis_word(v[2]);
    }
    /* 0x1611c: the pedal is the word over 1023, key or axis alike */
    out->throttle = sw[0] ? 1.0f : (float)gas / 1023.0f;
    /* 0x16174: an axis IS the brake; a switch is eased to full (drive.c) */
    out->brake_analog = !sw[1] && stop > 0;
    out->brake = sw[1] ? 1.0f : (float)stop / 1023.0f;
    /* 0x1633b: the wheel wants word / 1024 of the lock.  Keys: left less
       right, eased.  A stick: the one signed word, taken as it is. */
    if (sw[2] || sw[3]) g_steer_stick = 0;
    else if (stick) g_steer_stick = 1;
    out->steer_analog = g_steer_stick;
    out->steer = g_steer_stick ? (float)-stick * 0.0009765625f
                               : (float)((sw[2] - sw[3]) * 0x3ff) * 0.0009765625f;
    /* 0x18e0a: one gear a press (the game's latch at +0x140) */
    out->shift = (sw[4] && !g_gear_was[0]) ? +1 : (sw[5] && !g_gear_was[1]) ? -1 : 0;
    g_gear_was[0] = sw[4]; g_gear_was[1] = sw[5];
}

static int as_key(SDL_Event *ev, SDL_Keycode sym)
{
    Uint32 stamp = ev->common.timestamp;
    memset(ev, 0, sizeof *ev);
    ev->type = SDL_KEYDOWN;
    ev->key.timestamp = stamp;
    ev->key.state = SDL_PRESSED;
    ev->key.keysym.sym = sym;
    ev->key.keysym.scancode = SDL_GetScancodeFromKey(sym);
    return 1;
}

int controls_poll(SDL_Event *ev)
{
    if (!g_pad) pad_find();
    while (SDL_PollEvent(ev)) {
        switch (ev->type) {
        case SDL_CONTROLLERDEVICEADDED:
            if (!g_pad) g_pad = SDL_GameControllerOpen(ev->cdevice.which);
            continue;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (g_pad && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pad)) == ev->cdevice.which) {
                SDL_GameControllerClose(g_pad);
                g_pad = NULL;
            }
            continue;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (ev->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_START: return as_key(ev, SDLK_RETURN);
            case SDL_CONTROLLER_BUTTON_BACK:  return as_key(ev, SDLK_ESCAPE);
            case SDL_CONTROLLER_BUTTON_Y:     if (!g_menu) return as_key(ev, SDLK_c); continue;
            case SDL_CONTROLLER_BUTTON_X:     if (!g_menu) return as_key(ev, SDLK_m); continue;
            default: break;
            }
            if (!g_menu) continue;
            switch (ev->cbutton.button) {
#ifdef MISTER_HYBRID
            /* MISTER: in menus the OSD's Menu OK / Menu Back, which the core's
               joystick has as two buttons of their own (mister.c) */
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:  return as_key(ev, SDLK_RETURN);
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return as_key(ev, SDLK_ESCAPE);
#else
            case SDL_CONTROLLER_BUTTON_A:          return as_key(ev, SDLK_RETURN);
            case SDL_CONTROLLER_BUTTON_B:          return as_key(ev, SDLK_ESCAPE);
#endif
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    return as_key(ev, SDLK_UP);
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return as_key(ev, SDLK_DOWN);
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return as_key(ev, SDLK_LEFT);
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return as_key(ev, SDLK_RIGHT);
            default: continue;
            }
        case SDL_CONTROLLERAXISMOTION: {
            /* the stick pushed over counts as one press of an arrow */
            int ax = ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ? 0 : ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY ? 1 : -1;
            if (ax < 0) continue;
            int now = ev->caxis.value > 16000 ? 1 : ev->caxis.value < -16000 ? -1 : 0;
            int was = g_stick_was[ax];
            g_stick_was[ax] = now;
            if (!g_menu || !now || now == was) continue;
            return as_key(ev, ax == 0 ? (now > 0 ? SDLK_RIGHT : SDLK_LEFT) : (now > 0 ? SDLK_DOWN : SDLK_UP));
        }
        default: return 1;
        }
    }
    return 0;
}
